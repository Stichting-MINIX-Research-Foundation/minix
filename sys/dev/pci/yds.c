/*	$NetBSD: yds.c,v 1.56 2014/08/17 08:54:44 tsutsui Exp $	*/

/*
 * Copyright (c) 2000, 2001 Kazuki Sakamoto and Minoura Makoto.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Yamaha YMF724[B-F]/740[B-C]/744/754
 *
 * Documentation links:
 * - ftp://ftp.alsa-project.org/pub/manuals/yamaha/
 * - ftp://ftp.alsa-project.org/pub/manuals/yamaha/pci/
 *
 * TODO:
 * - FM synth volume (difficult: mixed before ac97)
 * - Digital in/out (SPDIF) support
 * - Effect??
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: yds.c,v 1.56 2014/08/17 08:54:44 tsutsui Exp $");

#include "mpu.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/fcntl.h>
#include <sys/kmem.h>
#include <sys/device.h>
#include <sys/proc.h>

#include <dev/pci/pcidevs.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <sys/audioio.h>
#include <dev/audio_if.h>
#include <dev/mulaw.h>
#include <dev/auconv.h>
#include <dev/ic/ac97reg.h>
#include <dev/ic/ac97var.h>
#include <dev/ic/mpuvar.h>

#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/microcode/yds/yds_hwmcode.h>
#include <dev/pci/ydsreg.h>
#include <dev/pci/ydsvar.h>

/* Debug */
#undef YDS_USE_REC_SLOT
#define YDS_USE_P44

#ifdef AUDIO_DEBUG
# define DPRINTF(x)	if (ydsdebug) printf x
# define DPRINTFN(n,x)	if (ydsdebug>(n)) printf x
int	ydsdebug = 0;
#else
# define DPRINTF(x)
# define DPRINTFN(n,x)
#endif
#ifdef YDS_USE_REC_SLOT
# define YDS_INPUT_SLOT 0	/* REC slot = ADC + loopbacks */
#else
# define YDS_INPUT_SLOT 1	/* ADC slot */
#endif

static int	yds_match(device_t, cfdata_t, void *);
static void	yds_attach(device_t, device_t, void *);
static int	yds_intr(void *);

#define DMAADDR(p)	((p)->map->dm_segs[0].ds_addr)
#define KERNADDR(p)	((void *)((p)->addr))

static int	yds_allocmem(struct yds_softc *, size_t, size_t,
			     struct yds_dma *);
static int	yds_freemem(struct yds_softc *, struct yds_dma *);

#ifndef AUDIO_DEBUG
#define YWRITE1(sc, r, x) bus_space_write_1((sc)->memt, (sc)->memh, (r), (x))
#define YWRITE2(sc, r, x) bus_space_write_2((sc)->memt, (sc)->memh, (r), (x))
#define YWRITE4(sc, r, x) bus_space_write_4((sc)->memt, (sc)->memh, (r), (x))
#define YREAD1(sc, r)	bus_space_read_1((sc)->memt, (sc)->memh, (r))
#define YREAD2(sc, r)	bus_space_read_2((sc)->memt, (sc)->memh, (r))
#define YREAD4(sc, r)	bus_space_read_4((sc)->memt, (sc)->memh, (r))
#else
static uint16_t YREAD2(struct yds_softc *sc, bus_size_t r)
{
	DPRINTFN(5, (" YREAD2(0x%lX)\n", (unsigned long)r));
	return bus_space_read_2(sc->memt, sc->memh, r);
}

static uint32_t YREAD4(struct yds_softc *sc, bus_size_t r)
{
	DPRINTFN(5, (" YREAD4(0x%lX)\n", (unsigned long)r));
	return bus_space_read_4(sc->memt, sc->memh, r);
}

#ifdef notdef
static void YWRITE1(struct yds_softc *sc, bus_size_t r, uint8_t x)
{
	DPRINTFN(5, (" YWRITE1(0x%lX,0x%lX)\n", (unsigned long)r,
		     (unsigned long)x));
	bus_space_write_1(sc->memt, sc->memh, r, x);
}
#endif

static void YWRITE2(struct yds_softc *sc, bus_size_t r, uint16_t x)
{
	DPRINTFN(5, (" YWRITE2(0x%lX,0x%lX)\n", (unsigned long)r,
		     (unsigned long)x));
	bus_space_write_2(sc->memt, sc->memh, r, x);
}

static void YWRITE4(struct yds_softc *sc, bus_size_t r, uint32_t x)
{
	DPRINTFN(5, (" YWRITE4(0x%lX,0x%lX)\n", (unsigned long)r,
		     (unsigned long)x));
	bus_space_write_4(sc->memt, sc->memh, r, x);
}
#endif

#define	YWRITEREGION4(sc, r, x, c)	\
	bus_space_write_region_4((sc)->memt, (sc)->memh, (r), (x), (c) / 4)

CFATTACH_DECL_NEW(yds, sizeof(struct yds_softc),
    yds_match, yds_attach, NULL, NULL);

static int	yds_open(void *, int);
static void	yds_close(void *);
static int	yds_query_encoding(void *, struct audio_encoding *);
static int	yds_set_params(void *, int, int, audio_params_t *,
			       audio_params_t *, stream_filter_list_t *,
			       stream_filter_list_t *);
static int	yds_round_blocksize(void *, int, int, const audio_params_t *);
static int	yds_trigger_output(void *, void *, void *, int,
				   void (*)(void *), void *,
				   const audio_params_t *);
static int	yds_trigger_input(void *, void *, void *, int,
				  void (*)(void *), void *,
				  const audio_params_t *);
static int	yds_halt_output(void *);
static int	yds_halt_input(void *);
static int	yds_getdev(void *, struct audio_device *);
static int	yds_mixer_set_port(void *, mixer_ctrl_t *);
static int	yds_mixer_get_port(void *, mixer_ctrl_t *);
static void *	yds_malloc(void *, int, size_t);
static void	yds_free(void *, void *, size_t);
static size_t	yds_round_buffersize(void *, int, size_t);
static paddr_t	yds_mappage(void *, void *, off_t, int);
static int	yds_get_props(void *);
static int	yds_query_devinfo(void *, mixer_devinfo_t *);
static void	yds_get_locks(void *, kmutex_t **, kmutex_t **);

static int	yds_attach_codec(void *, struct ac97_codec_if *);
static int	yds_read_codec(void *, uint8_t, uint16_t *);
static int	yds_write_codec(void *, uint8_t, uint16_t);
static int	yds_reset_codec(void *);

static u_int	yds_get_dstype(int);
static int	yds_download_mcode(struct yds_softc *);
static int	yds_allocate_slots(struct yds_softc *);
static void	yds_configure_legacy(device_t);
static void	yds_enable_dsp(struct yds_softc *);
static int	yds_disable_dsp(struct yds_softc *);
static int	yds_ready_codec(struct yds_codec_softc *);
static int	yds_halt(struct yds_softc *);
static uint32_t yds_get_lpfq(u_int);
static uint32_t yds_get_lpfk(u_int);
static struct yds_dma *yds_find_dma(struct yds_softc *, void *);

static int	yds_init(struct yds_softc *);

#ifdef AUDIO_DEBUG
static void	yds_dump_play_slot(struct yds_softc *, int);
#define	YDS_DUMP_PLAY_SLOT(n, sc, bank) \
	if (ydsdebug > (n)) yds_dump_play_slot(sc, bank)
#else
#define	YDS_DUMP_PLAY_SLOT(n, sc, bank)
#endif /* AUDIO_DEBUG */

static const struct audio_hw_if yds_hw_if = {
	.open		  = yds_open,
	.close		  = yds_close,
	.drain		  = NULL,
	.query_encoding	  = yds_query_encoding,
	.set_params	  = yds_set_params,
	.round_blocksize  = yds_round_blocksize,
	.commit_settings  = NULL,
	.init_output	  = NULL,
	.init_input	  = NULL,
	.start_output	  = NULL,
	.start_input	  = NULL,
	.halt_output	  = yds_halt_output,
	.halt_input	  = yds_halt_input,
	.speaker_ctl	  = NULL,
	.getdev		  = yds_getdev,
	.setfd		  = NULL,
	.set_port	  = yds_mixer_set_port,
	.get_port	  = yds_mixer_get_port,
	.query_devinfo	  = yds_query_devinfo,
	.allocm		  = yds_malloc,
	.freem		  = yds_free,
	.round_buffersize = yds_round_buffersize,
	.mappage	  = yds_mappage,
	.get_props	  = yds_get_props,
	.trigger_output	  = yds_trigger_output,
	.trigger_input	  = yds_trigger_input,
	.dev_ioctl	  = NULL,
	.get_locks	  = yds_get_locks,
};

static const struct audio_device yds_device = {
	.name    = "Yamaha DS-1",
	.version = "",
	.config  = "yds"
};

static const struct {
	uint	id;
	u_int	flags;
#define YDS_CAP_MCODE_1			0x0001
#define YDS_CAP_MCODE_1E		0x0002
#define YDS_CAP_LEGACY_SELECTABLE	0x0004
#define YDS_CAP_LEGACY_FLEXIBLE		0x0008
#define YDS_CAP_HAS_P44			0x0010
} yds_chip_capabliity_list[] = {
	{ PCI_PRODUCT_YAMAHA_YMF724,
	  YDS_CAP_MCODE_1|YDS_CAP_LEGACY_SELECTABLE },
	/* 740[C] has only 32 slots.  But anyway we use only 2 */
	{ PCI_PRODUCT_YAMAHA_YMF740,
	  YDS_CAP_MCODE_1|YDS_CAP_LEGACY_SELECTABLE },	/* XXX NOT TESTED */
	{ PCI_PRODUCT_YAMAHA_YMF740C,
	  YDS_CAP_MCODE_1E|YDS_CAP_LEGACY_SELECTABLE },
	{ PCI_PRODUCT_YAMAHA_YMF724F,
	  YDS_CAP_MCODE_1E|YDS_CAP_LEGACY_SELECTABLE },
	{ PCI_PRODUCT_YAMAHA_YMF744B,
	  YDS_CAP_MCODE_1E|YDS_CAP_LEGACY_FLEXIBLE },
	{ PCI_PRODUCT_YAMAHA_YMF754,
	  YDS_CAP_MCODE_1E|YDS_CAP_LEGACY_FLEXIBLE|YDS_CAP_HAS_P44 },
	{ 0, 0 }
};
#ifdef AUDIO_DEBUG
#define YDS_CAP_BITS	"\020\005P44\004LEGFLEX\003LEGSEL\002MCODE1E\001MCODE1"
#endif

static const struct audio_format yds_formats[] = {
	{NULL, AUMODE_PLAY | AUMODE_RECORD, AUDIO_ENCODING_SLINEAR_LE, 16, 16,
	 1, AUFMT_MONAURAL, 0, {4000, 48000}},
	{NULL, AUMODE_PLAY | AUMODE_RECORD, AUDIO_ENCODING_SLINEAR_LE, 16, 16,
	 2, AUFMT_STEREO, 0, {4000, 48000}},
	{NULL, AUMODE_PLAY | AUMODE_RECORD, AUDIO_ENCODING_ULINEAR_LE, 8, 8,
	 1, AUFMT_MONAURAL, 0, {4000, 48000}},
	{NULL, AUMODE_PLAY | AUMODE_RECORD, AUDIO_ENCODING_ULINEAR_LE, 8, 8,
	 2, AUFMT_STEREO, 0, {4000, 48000}},
};
#define	YDS_NFORMATS	(sizeof(yds_formats) / sizeof(struct audio_format))

#ifdef AUDIO_DEBUG
static void
yds_dump_play_slot(struct yds_softc *sc, int bank)
{
	int i, j;
	uint32_t *p;
	uint32_t num;
	bus_addr_t pa;

	for (i = 0; i < N_PLAY_SLOTS; i++) {
		printf("pbankp[%d] = %p,", i*2, sc->pbankp[i*2]);
		printf("pbankp[%d] = %p\n", i*2+1, sc->pbankp[i*2+1]);
	}

	pa = DMAADDR(&sc->sc_ctrldata) + sc->pbankoff;
	p = sc->ptbl;
	printf("ptbl + 0: %d\n", *p++);
	for (i = 0; i < N_PLAY_SLOTS; i++) {
		printf("ptbl + %d: %#x, should be %#" PRIxPADDR "\n",
		       i+1, *p,
		       pa + i * sizeof(struct play_slot_ctrl_bank) *
				N_PLAY_SLOT_CTRL_BANK);
		p++;
	}

	num = le32toh(*(uint32_t*)sc->ptbl);
	printf("numofplay = %d\n", num);

	for (i = 0; i < num; i++) {
		p = (uint32_t *)sc->pbankp[i*2];

		printf("  pbankp[%d], bank 0 : %p\n", i*2, p);
		for (j = 0;
		     j < sizeof(struct play_slot_ctrl_bank) / sizeof(uint32_t);
		     j++) {
			printf("    0x%02x: 0x%08x\n",
			       (unsigned)(j * sizeof(uint32_t)),
			       (unsigned)*p++);
		}

		p = (uint32_t *)sc->pbankp[i*2 + 1];
		printf("  pbankp[%d], bank 1 : %p\n", i*2 + 1, p);
		for (j = 0;
		     j < sizeof(struct play_slot_ctrl_bank) / sizeof(uint32_t);
		     j++) {
			printf("    0x%02x: 0x%08x\n",
			       (unsigned)(j * sizeof(uint32_t)),
			       (unsigned)*p++);
		}
	}
}
#endif /* AUDIO_DEBUG */

static u_int
yds_get_dstype(int id)
{
	int i;

	for (i = 0; yds_chip_capabliity_list[i].id; i++) {
		if (PCI_PRODUCT(id) == yds_chip_capabliity_list[i].id)
			return yds_chip_capabliity_list[i].flags;
	}

	return -1;
}

static int
yds_download_mcode(struct yds_softc *sc)
{
	static struct {
		const uint32_t *mcode;
		size_t size;
	} ctrls[] = {
		{yds_ds1_ctrl_mcode, sizeof(yds_ds1_ctrl_mcode)},
		{yds_ds1e_ctrl_mcode, sizeof(yds_ds1e_ctrl_mcode)},
	};
	u_int ctrl;
	const uint32_t *p;
	size_t size;
	int dstype;

	if (sc->sc_flags & YDS_CAP_MCODE_1)
		dstype = YDS_DS_1;
	else if (sc->sc_flags & YDS_CAP_MCODE_1E)
		dstype = YDS_DS_1E;
	else
		return 1;	/* unknown */

	if (yds_disable_dsp(sc))
		return 1;

	/* Software reset */
	YWRITE4(sc, YDS_MODE, YDS_MODE_RESET);
	YWRITE4(sc, YDS_MODE, 0);

	YWRITE4(sc, YDS_MAPOF_REC, 0);
	YWRITE4(sc, YDS_MAPOF_EFFECT, 0);
	YWRITE4(sc, YDS_PLAY_CTRLBASE, 0);
	YWRITE4(sc, YDS_REC_CTRLBASE, 0);
	YWRITE4(sc, YDS_EFFECT_CTRLBASE, 0);
	YWRITE4(sc, YDS_WORK_BASE, 0);

	ctrl = YREAD2(sc, YDS_GLOBAL_CONTROL);
	YWRITE2(sc, YDS_GLOBAL_CONTROL, ctrl & ~0x0007);

	/* Download DSP microcode. */
	p = yds_dsp_mcode;
	size = sizeof(yds_dsp_mcode);
	YWRITEREGION4(sc, YDS_DSP_INSTRAM, p, size);

	/* Download CONTROL microcode. */
	p = ctrls[dstype].mcode;
	size = ctrls[dstype].size;
	YWRITEREGION4(sc, YDS_CTRL_INSTRAM, p, size);

	yds_enable_dsp(sc);
	delay(10 * 1000);		/* nessesary on my 724F (??) */

	return 0;
}

static int
yds_allocate_slots(struct yds_softc *sc)
{
	size_t pcs, rcs, ecs, ws, memsize;
	void *mp;
	uint32_t da;		/* DMA address */
	char *va;		/* KVA */
	off_t cb;
	int i;
	struct yds_dma *p;

	/* Alloc DSP Control Data */
	pcs = YREAD4(sc, YDS_PLAY_CTRLSIZE) * sizeof(uint32_t);
	rcs = YREAD4(sc, YDS_REC_CTRLSIZE) * sizeof(uint32_t);
	ecs = YREAD4(sc, YDS_EFFECT_CTRLSIZE) * sizeof(uint32_t);
	ws = WORK_SIZE;
	YWRITE4(sc, YDS_WORK_SIZE, ws / sizeof(uint32_t));

	DPRINTF(("play control size : %d\n", (unsigned int)pcs));
	DPRINTF(("rec control size : %d\n", (unsigned int)rcs));
	DPRINTF(("eff control size : %d\n", (unsigned int)ecs));
#ifndef AUDIO_DEBUG
	__USE(ecs);
#endif
	DPRINTF(("work size : %d\n", (unsigned int)ws));
#ifdef DIAGNOSTIC
	if (pcs != sizeof(struct play_slot_ctrl_bank)) {
		aprint_error_dev(sc->sc_dev, "invalid play slot ctrldata %d != %d\n",
		       (unsigned int)pcs,
		       (unsigned int)sizeof(struct play_slot_ctrl_bank));
	if (rcs != sizeof(struct rec_slot_ctrl_bank))
		aprint_error_dev(sc->sc_dev, "invalid rec slot ctrldata %d != %d\n",
		       (unsigned int)rcs,
		       (unsigned int)sizeof(struct rec_slot_ctrl_bank));
	}
#endif

	memsize = N_PLAY_SLOTS*N_PLAY_SLOT_CTRL_BANK*pcs +
		  N_REC_SLOT_CTRL*N_REC_SLOT_CTRL_BANK*rcs + ws;
	memsize += (N_PLAY_SLOTS+1)*sizeof(uint32_t);

	p = &sc->sc_ctrldata;
	if (KERNADDR(p) == NULL) {
		i = yds_allocmem(sc, memsize, 16, p);
		if (i) {
			aprint_error_dev(sc->sc_dev, "couldn't alloc/map DSP DMA buffer, reason %d\n", i);
			return 1;
		}
	}
	mp = KERNADDR(p);
	da = DMAADDR(p);

	DPRINTF(("mp:%p, DMA addr:%#" PRIxPADDR "\n",
		 mp, sc->sc_ctrldata.map->dm_segs[0].ds_addr));

	memset(mp, 0, memsize);

	/* Work space */
	cb = 0;
	va = (uint8_t *)mp;
	YWRITE4(sc, YDS_WORK_BASE, da + cb);
	cb += ws;

	/* Play control data table */
	sc->ptbl = (uint32_t *)(va + cb);
	sc->ptbloff = cb;
	YWRITE4(sc, YDS_PLAY_CTRLBASE, da + cb);
	cb += (N_PLAY_SLOT_CTRL + 1) * sizeof(uint32_t);

	/* Record slot control data */
	sc->rbank = (struct rec_slot_ctrl_bank *)(va + cb);
	YWRITE4(sc, YDS_REC_CTRLBASE, da + cb);
	sc->rbankoff = cb;
	cb += N_REC_SLOT_CTRL * N_REC_SLOT_CTRL_BANK * rcs;

#if 0
	/* Effect slot control data -- unused */
	YWRITE4(sc, YDS_EFFECT_CTRLBASE, da + cb);
	cb += N_EFFECT_SLOT_CTRL * N_EFFECT_SLOT_CTRL_BANK * ecs;
#endif

	/* Play slot control data */
	sc->pbankoff = cb;
	for (i=0; i < N_PLAY_SLOT_CTRL; i++) {
		sc->pbankp[i*2] = (struct play_slot_ctrl_bank *)(va + cb);
		*(sc->ptbl + i+1) = htole32(da + cb);
		cb += pcs;

		sc->pbankp[i*2+1] = (struct play_slot_ctrl_bank *)(va + cb);
		cb += pcs;
	}
	/* Sync play control data table */
	bus_dmamap_sync(sc->sc_dmatag, p->map,
			sc->ptbloff, (N_PLAY_SLOT_CTRL+1) * sizeof(uint32_t),
			BUS_DMASYNC_PREWRITE);

	return 0;
}

static void
yds_enable_dsp(struct yds_softc *sc)
{

	YWRITE4(sc, YDS_CONFIG, YDS_DSP_SETUP);
}

static int
yds_disable_dsp(struct yds_softc *sc)
{
	int to;
	uint32_t data;

	data = YREAD4(sc, YDS_CONFIG);
	if (data)
		YWRITE4(sc, YDS_CONFIG, YDS_DSP_DISABLE);

	for (to = 0; to < YDS_WORK_TIMEOUT; to++) {
		if ((YREAD4(sc, YDS_STATUS) & YDS_STAT_WORK) == 0)
			return 0;
		delay(1);
	}

	return 1;
}

static int
yds_match(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa;

	pa = (struct pci_attach_args *)aux;
	switch (PCI_VENDOR(pa->pa_id)) {
	case PCI_VENDOR_YAMAHA:
		switch (PCI_PRODUCT(pa->pa_id)) {
		case PCI_PRODUCT_YAMAHA_YMF724:
		case PCI_PRODUCT_YAMAHA_YMF740:
		case PCI_PRODUCT_YAMAHA_YMF740C:
		case PCI_PRODUCT_YAMAHA_YMF724F:
		case PCI_PRODUCT_YAMAHA_YMF744B:
		case PCI_PRODUCT_YAMAHA_YMF754:
			return 1;
		}
		break;
	}

	return 0;
}

/*
 * This routine is called after all the ISA devices are configured,
 * to avoid conflict.
 */
static void
yds_configure_legacy(device_t self)
#define FLEXIBLE	(sc->sc_flags & YDS_CAP_LEGACY_FLEXIBLE)
#define SELECTABLE	(sc->sc_flags & YDS_CAP_LEGACY_SELECTABLE)
{
	static const bus_addr_t opl_addrs[] = {0x388, 0x398, 0x3A0, 0x3A8};
	static const bus_addr_t mpu_addrs[] = {0x330, 0x300, 0x332, 0x334};
	struct yds_softc *sc;
	pcireg_t reg;
	device_t dev;
	int i;

	sc = device_private(self);
	if (!FLEXIBLE && !SELECTABLE)
		return;

	reg = pci_conf_read(sc->sc_pc, sc->sc_pcitag, YDS_PCI_LEGACY);
	reg &= ~0x8133c03f;	/* these bits are out of interest */
	reg |= ((YDS_PCI_EX_LEGACY_IMOD) |
		(YDS_PCI_LEGACY_FMEN |
		 YDS_PCI_LEGACY_MEN /*| YDS_PCI_LEGACY_MIEN*/));
	reg |= YDS_PCI_EX_LEGACY_SMOD_DISABLE;
	if (FLEXIBLE) {
		pci_conf_write(sc->sc_pc, sc->sc_pcitag, YDS_PCI_LEGACY, reg);
		delay(100*1000);
	}

	/* Look for OPL */
	dev = 0;
	for (i = 0; i < sizeof(opl_addrs) / sizeof(bus_addr_t); i++) {
		if (SELECTABLE) {
			pci_conf_write(sc->sc_pc, sc->sc_pcitag,
				       YDS_PCI_LEGACY, reg | (i << (0+16)));
			delay(100*1000);	/* wait 100ms */
		} else
			pci_conf_write(sc->sc_pc, sc->sc_pcitag,
				       YDS_PCI_FM_BA, opl_addrs[i]);
		if (bus_space_map(sc->sc_opl_iot,
				  opl_addrs[i], 4, 0, &sc->sc_opl_ioh) == 0) {
			struct audio_attach_args aa;

			aa.type = AUDIODEV_TYPE_OPL;
			aa.hwif = aa.hdl = NULL;
			dev = config_found(self, &aa, audioprint);
			if (dev == 0)
				bus_space_unmap(sc->sc_opl_iot,
						sc->sc_opl_ioh, 4);
			else {
				if (SELECTABLE)
					reg |= (i << (0+16));
				break;
			}
		}
	}
	if (dev == 0) {
		reg &= ~YDS_PCI_LEGACY_FMEN;
		pci_conf_write(sc->sc_pc, sc->sc_pcitag,
			       YDS_PCI_LEGACY, reg);
	} else {
		/* Max. volume */
		YWRITE4(sc, YDS_LEGACY_OUT_VOLUME, 0x3fff3fff);
		YWRITE4(sc, YDS_LEGACY_REC_VOLUME, 0x3fff3fff);
	}

	/* Look for MPU */
	dev = NULL;
	for (i = 0; i < sizeof(mpu_addrs) / sizeof(bus_addr_t); i++) {
		if (SELECTABLE)
			pci_conf_write(sc->sc_pc, sc->sc_pcitag,
				       YDS_PCI_LEGACY, reg | (i << (4+16)));
		else
			pci_conf_write(sc->sc_pc, sc->sc_pcitag,
				       YDS_PCI_MPU_BA, mpu_addrs[i]);
		if (bus_space_map(sc->sc_mpu_iot,
				  mpu_addrs[i], 2, 0, &sc->sc_mpu_ioh) == 0) {
			struct audio_attach_args aa;

			aa.type = AUDIODEV_TYPE_MPU;
			aa.hwif = aa.hdl = NULL;
			dev = config_found(self, &aa, audioprint);
			if (dev == 0)
				bus_space_unmap(sc->sc_mpu_iot,
						sc->sc_mpu_ioh, 2);
			else {
				if (SELECTABLE)
					reg |= (i << (4+16));
				break;
			}
		}
	}
	if (dev == 0) {
		reg &= ~(YDS_PCI_LEGACY_MEN | YDS_PCI_LEGACY_MIEN);
		pci_conf_write(sc->sc_pc, sc->sc_pcitag, YDS_PCI_LEGACY, reg);
	}
	sc->sc_mpu = dev;
}
#undef FLEXIBLE
#undef SELECTABLE

static int
yds_init(struct yds_softc *sc)
{
	uint32_t reg;

	DPRINTF(("yds_init()\n"));

	/* Download microcode */
	if (yds_download_mcode(sc)) {
		aprint_error_dev(sc->sc_dev, "download microcode failed\n");
		return 1;
	}

	/* Allocate DMA buffers */
	if (yds_allocate_slots(sc)) {
		aprint_error_dev(sc->sc_dev, "could not allocate slots\n");
		return 1;
	}

	/* Warm reset */
	reg = pci_conf_read(sc->sc_pc, sc->sc_pcitag, YDS_PCI_DSCTRL);
	pci_conf_write(sc->sc_pc, sc->sc_pcitag, YDS_PCI_DSCTRL,
		reg | YDS_DSCTRL_WRST);
	delay(50000);

	return 0;
}

static bool
yds_suspend(device_t dv, const pmf_qual_t *qual)
{
	struct yds_softc *sc = device_private(dv);
	pci_chipset_tag_t pc = sc->sc_pc;
	pcitag_t tag = sc->sc_pcitag;

	mutex_enter(&sc->sc_lock);
	mutex_spin_enter(&sc->sc_intr_lock);
	sc->sc_dsctrl = pci_conf_read(pc, tag, YDS_PCI_DSCTRL);
	sc->sc_legacy = pci_conf_read(pc, tag, YDS_PCI_LEGACY);
	sc->sc_ba[0] = pci_conf_read(pc, tag, YDS_PCI_FM_BA);
	sc->sc_ba[1] = pci_conf_read(pc, tag, YDS_PCI_MPU_BA);
	mutex_spin_exit(&sc->sc_intr_lock);
	mutex_exit(&sc->sc_lock);

	return true;
}

static bool
yds_resume(device_t dv, const pmf_qual_t *qual)
{
	struct yds_softc *sc = device_private(dv);
	pci_chipset_tag_t pc = sc->sc_pc;
	pcitag_t tag = sc->sc_pcitag;
	pcireg_t reg;

	/* Disable legacy mode */
	mutex_enter(&sc->sc_lock);
	mutex_spin_enter(&sc->sc_intr_lock);
	reg = pci_conf_read(pc, tag, YDS_PCI_LEGACY);
	pci_conf_write(pc, tag, YDS_PCI_LEGACY, reg & YDS_PCI_LEGACY_LAD);

	/* Enable the device. */
	reg = pci_conf_read(pc, tag, PCI_COMMAND_STATUS_REG);
	reg |= (PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MEM_ENABLE |
		PCI_COMMAND_MASTER_ENABLE);
	pci_conf_write(pc, tag, PCI_COMMAND_STATUS_REG, reg);
	reg = pci_conf_read(pc, tag, PCI_COMMAND_STATUS_REG);
	if (yds_init(sc)) {
		aprint_error_dev(dv, "reinitialize failed\n");
		mutex_spin_exit(&sc->sc_intr_lock);
		mutex_exit(&sc->sc_lock);
		return false;
	}

	pci_conf_write(pc, tag, YDS_PCI_DSCTRL, sc->sc_dsctrl);
	mutex_spin_exit(&sc->sc_intr_lock);
	sc->sc_codec[0].codec_if->vtbl->restore_ports(sc->sc_codec[0].codec_if);
	mutex_exit(&sc->sc_lock);

	return true;
}

static void
yds_attach(device_t parent, device_t self, void *aux)
{
	struct yds_softc *sc;
	struct pci_attach_args *pa;
	pci_chipset_tag_t pc;
	char const *intrstr;
	pci_intr_handle_t ih;
	pcireg_t reg;
	struct yds_codec_softc *codec;
	int i, r, to;
	int revision;
	int ac97_id2;
	char intrbuf[PCI_INTRSTR_LEN];

	sc = device_private(self);
	sc->sc_dev = self;
	pa = (struct pci_attach_args *)aux;
	pc = pa->pa_pc;
	revision = PCI_REVISION(pa->pa_class);

	pci_aprint_devinfo(pa, NULL);

	/* Map register to memory */
	if (pci_mapreg_map(pa, YDS_PCI_MBA, PCI_MAPREG_TYPE_MEM, 0,
			   &sc->memt, &sc->memh, NULL, NULL)) {
		aprint_error_dev(self, "can't map memory space\n");
		return;
	}

	/* Map and establish the interrupt. */
	if (pci_intr_map(pa, &ih)) {
		aprint_error_dev(self, "couldn't map interrupt\n");
		return;
	}

	mutex_init(&sc->sc_lock, MUTEX_DEFAULT, IPL_AUDIO); /* XXX IPL_NONE? */
	mutex_init(&sc->sc_intr_lock, MUTEX_DEFAULT, IPL_AUDIO);

	intrstr = pci_intr_string(pc, ih, intrbuf, sizeof(intrbuf));
	sc->sc_ih = pci_intr_establish(pc, ih, IPL_AUDIO, yds_intr, sc);
	if (sc->sc_ih == NULL) {
		aprint_error_dev(self, "couldn't establish interrupt");
		if (intrstr != NULL)
			aprint_error(" at %s", intrstr);
		aprint_error("\n");
		mutex_destroy(&sc->sc_lock);
		mutex_destroy(&sc->sc_intr_lock);
		return;
	}
	aprint_normal_dev(self, "interrupting at %s\n", intrstr);

	sc->sc_dmatag = pa->pa_dmat;
	sc->sc_pc = pc;
	sc->sc_pcitag = pa->pa_tag;
	sc->sc_id = pa->pa_id;
	sc->sc_revision = revision;
	sc->sc_flags = yds_get_dstype(sc->sc_id);
#ifdef AUDIO_DEBUG
	if (ydsdebug) {
		char bits[80];

		snprintb(bits, sizeof(bits), YDS_CAP_BITS, sc->sc_flags);
		printf("%s: chip has %s\n", device_xname(self), bits);
	}
#endif

	/* Disable legacy mode */
	reg = pci_conf_read(pc, pa->pa_tag, YDS_PCI_LEGACY);
	pci_conf_write(pc, pa->pa_tag, YDS_PCI_LEGACY,
		       reg & YDS_PCI_LEGACY_LAD);

	/* Enable the device. */
	reg = pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	reg |= (PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MEM_ENABLE |
		PCI_COMMAND_MASTER_ENABLE);
	pci_conf_write(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG, reg);
	reg = pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);

	/* Mute all volumes */
	for (i = 0x80; i < 0xc0; i += 2)
		YWRITE2(sc, i, 0);

	/* Initialize the device */
	if (yds_init(sc)) {
		aprint_error_dev(self, "initialize failed\n");
		mutex_destroy(&sc->sc_lock);
		mutex_destroy(&sc->sc_intr_lock);
		return;
	}

	/*
	 * Detect primary/secondary AC97
	 *	YMF754 Hardware Specification Rev 1.01 page 24
	 */
	reg = pci_conf_read(pc, pa->pa_tag, YDS_PCI_DSCTRL);
	pci_conf_write(pc, pa->pa_tag, YDS_PCI_DSCTRL, reg & ~YDS_DSCTRL_CRST);
	delay(400000);		/* Needed for 740C. */

	/* Primary */
	for (to = 0; to < AC97_TIMEOUT; to++) {
		if ((YREAD2(sc, AC97_STAT_ADDR1) & AC97_BUSY) == 0)
			break;
		delay(1);
	}
	if (to == AC97_TIMEOUT) {
		aprint_error_dev(self, "no AC97 available\n");
		mutex_destroy(&sc->sc_lock);
		mutex_destroy(&sc->sc_intr_lock);
		return;
	}

	/* Secondary */
	/* Secondary AC97 is used for 4ch audio. Currently unused. */
	ac97_id2 = -1;
	if ((YREAD2(sc, YDS_ACTIVITY) & YDS_ACTIVITY_DOCKA) == 0)
		goto detected;
#if 0				/* reset secondary... */
	YWRITE2(sc, YDS_GPIO_OCTRL,
		YREAD2(sc, YDS_GPIO_OCTRL) & ~YDS_GPIO_GPO2);
	YWRITE2(sc, YDS_GPIO_FUNCE,
		(YREAD2(sc, YDS_GPIO_FUNCE)&(~YDS_GPIO_GPC2))|YDS_GPIO_GPE2);
#endif
	for (to = 0; to < AC97_TIMEOUT; to++) {
		if ((YREAD2(sc, AC97_STAT_ADDR2) & AC97_BUSY) == 0)
			break;
		delay(1);
	}
	if (to < AC97_TIMEOUT) {
		/* detect id */
		for (ac97_id2 = 1; ac97_id2 < 4; ac97_id2++) {
			YWRITE2(sc, AC97_CMD_ADDR,
				AC97_CMD_READ | AC97_ID(ac97_id2) | 0x28);

			for (to = 0; to < AC97_TIMEOUT; to++) {
				if ((YREAD2(sc, AC97_STAT_ADDR2) & AC97_BUSY)
				    == 0)
					goto detected;
				delay(1);
			}
		}
		if (ac97_id2 == 4)
			ac97_id2 = -1;
detected:
		;
	}

	pci_conf_write(pc, pa->pa_tag, YDS_PCI_DSCTRL, reg | YDS_DSCTRL_CRST);
	delay (20);
	pci_conf_write(pc, pa->pa_tag, YDS_PCI_DSCTRL, reg & ~YDS_DSCTRL_CRST);
	delay (400000);
	for (to = 0; to < AC97_TIMEOUT; to++) {
		if ((YREAD2(sc, AC97_STAT_ADDR1) & AC97_BUSY) == 0)
			break;
		delay(1);
	}

	/*
	 * Attach ac97 codec
	 */
	for (i = 0; i < 2; i++) {
		static struct {
			int data;
			int addr;
		} statregs[] = {
			{AC97_STAT_DATA1, AC97_STAT_ADDR1},
			{AC97_STAT_DATA2, AC97_STAT_ADDR2},
		};

		if (i == 1 && ac97_id2 == -1)
			break;		/* secondary ac97 not available */

		codec = &sc->sc_codec[i];
		codec->sc = sc;
		codec->id = i == 1 ? ac97_id2 : 0;
		codec->status_data = statregs[i].data;
		codec->status_addr = statregs[i].addr;
		codec->host_if.arg = codec;
		codec->host_if.attach = yds_attach_codec;
		codec->host_if.read = yds_read_codec;
		codec->host_if.write = yds_write_codec;
		codec->host_if.reset = yds_reset_codec;

		r = ac97_attach(&codec->host_if, self, &sc->sc_lock);
		if (r != 0) {
			aprint_error_dev(self, "can't attach codec (error 0x%X)\n", r);
			mutex_destroy(&sc->sc_lock);
			mutex_destroy(&sc->sc_intr_lock);
			return;
		}
	}

	if (0 != auconv_create_encodings(yds_formats, YDS_NFORMATS,
	    &sc->sc_encodings)) {
		mutex_destroy(&sc->sc_lock);
		mutex_destroy(&sc->sc_intr_lock);
		return;
	}

	audio_attach_mi(&yds_hw_if, sc, self);

	sc->sc_legacy_iot = pa->pa_iot;
	config_defer(self, yds_configure_legacy);

	if (!pmf_device_register(self, yds_suspend, yds_resume))
		aprint_error_dev(self, "couldn't establish power handler\n");
}

static int
yds_attach_codec(void *sc_, struct ac97_codec_if *codec_if)
{
	struct yds_codec_softc *sc;

	sc = sc_;
	sc->codec_if = codec_if;
	return 0;
}

static int
yds_ready_codec(struct yds_codec_softc *sc)
{
	int to;

	for (to = 0; to < AC97_TIMEOUT; to++) {
		if ((YREAD2(sc->sc, sc->status_addr) & AC97_BUSY) == 0)
			return 0;
		delay(1);
	}

	return 1;
}

static int
yds_read_codec(void *sc_, uint8_t reg, uint16_t *data)
{
	struct yds_codec_softc *sc;

	sc = sc_;
	YWRITE2(sc->sc, AC97_CMD_ADDR, AC97_CMD_READ | AC97_ID(sc->id) | reg);

	if (yds_ready_codec(sc)) {
		aprint_error_dev(sc->sc->sc_dev, "yds_read_codec timeout\n");
		return EIO;
	}

	if (PCI_PRODUCT(sc->sc->sc_id) == PCI_PRODUCT_YAMAHA_YMF744B &&
	    sc->sc->sc_revision < 2) {
		int i;
		for (i=0; i<600; i++)
			(void)YREAD2(sc->sc, sc->status_data);
	}

	*data = YREAD2(sc->sc, sc->status_data);

	return 0;
}

static int
yds_write_codec(void *sc_, uint8_t reg, uint16_t data)
{
	struct yds_codec_softc *sc;

	sc = sc_;
	YWRITE2(sc->sc, AC97_CMD_ADDR, AC97_CMD_WRITE | AC97_ID(sc->id) | reg);
	YWRITE2(sc->sc, AC97_CMD_DATA, data);

	if (yds_ready_codec(sc)) {
		aprint_error_dev(sc->sc->sc_dev, "yds_write_codec timeout\n");
		return EIO;
	}

	return 0;
}

/*
 * XXX: Must handle the secondary differntly!!
 */
static int
yds_reset_codec(void *sc_)
{
	struct yds_codec_softc *codec;
	struct yds_softc *sc;
	pcireg_t reg;

	codec = sc_;
	sc = codec->sc;
	/* reset AC97 codec */
	reg = pci_conf_read(sc->sc_pc, sc->sc_pcitag, YDS_PCI_DSCTRL);
	if (reg & 0x03) {
		pci_conf_write(sc->sc_pc, sc->sc_pcitag,
			       YDS_PCI_DSCTRL, reg & ~0x03);
		pci_conf_write(sc->sc_pc, sc->sc_pcitag,
			       YDS_PCI_DSCTRL, reg | 0x03);
		pci_conf_write(sc->sc_pc, sc->sc_pcitag,
			       YDS_PCI_DSCTRL, reg & ~0x03);
		delay(50000);
	}

	yds_ready_codec(sc_);
	return 0;
}

static int
yds_intr(void *p)
{
	struct yds_softc *sc = p;
#if NMPU > 0
	struct mpu_softc *sc_mpu = device_private(sc->sc_mpu);
#endif
	u_int status;

	mutex_spin_enter(&sc->sc_intr_lock);

	status = YREAD4(sc, YDS_STATUS);
	DPRINTFN(1, ("yds_intr: status=%08x\n", status));
	if ((status & (YDS_STAT_INT|YDS_STAT_TINT)) == 0) {
#if NMPU > 0
		if (sc_mpu)
			return mpu_intr(sc_mpu);
#endif
		mutex_spin_exit(&sc->sc_intr_lock);
		return 0;
	}

	if (status & YDS_STAT_TINT) {
		YWRITE4(sc, YDS_STATUS, YDS_STAT_TINT);
		printf ("yds_intr: timeout!\n");
	}

	if (status & YDS_STAT_INT) {
		int nbank;

		nbank = (YREAD4(sc, YDS_CONTROL_SELECT) == 0);
		/* Clear interrupt flag */
		YWRITE4(sc, YDS_STATUS, YDS_STAT_INT);

		/* Buffer for the next frame is always ready. */
		YWRITE4(sc, YDS_MODE, YREAD4(sc, YDS_MODE) | YDS_MODE_ACTV2);

		if (sc->sc_play.intr) {
			u_int dma, ccpu, blk, len;

			/* Sync play slot control data */
			bus_dmamap_sync(sc->sc_dmatag, sc->sc_ctrldata.map,
					sc->pbankoff,
					sizeof(struct play_slot_ctrl_bank)*
					    le32toh(*sc->ptbl)*
					    N_PLAY_SLOT_CTRL_BANK,
					BUS_DMASYNC_POSTWRITE|
					BUS_DMASYNC_POSTREAD);
			dma = le32toh(sc->pbankp[nbank]->pgstart) * sc->sc_play.factor;
			ccpu = sc->sc_play.offset;
			blk = sc->sc_play.blksize;
			len = sc->sc_play.length;

			if (((dma > ccpu) && (dma - ccpu > blk * 2)) ||
			    ((ccpu > dma) && (dma + len - ccpu > blk * 2))) {
				/* We can fill the next block */
				/* Sync ring buffer for previous write */
				bus_dmamap_sync(sc->sc_dmatag,
						sc->sc_play.dma->map,
						ccpu, blk,
						BUS_DMASYNC_POSTWRITE);
				sc->sc_play.intr(sc->sc_play.intr_arg);
				sc->sc_play.offset += blk;
				if (sc->sc_play.offset >= len) {
					sc->sc_play.offset -= len;
#ifdef DIAGNOSTIC
					if (sc->sc_play.offset != 0)
						printf ("Audio ringbuffer botch\n");
#endif
				}
				/* Sync ring buffer for next write */
				bus_dmamap_sync(sc->sc_dmatag,
						sc->sc_play.dma->map,
						ccpu, blk,
						BUS_DMASYNC_PREWRITE);
			}
		}
		if (sc->sc_rec.intr) {
			u_int dma, ccpu, blk, len;

			/* Sync rec slot control data */
			bus_dmamap_sync(sc->sc_dmatag, sc->sc_ctrldata.map,
					sc->rbankoff,
					sizeof(struct rec_slot_ctrl_bank)*
					    N_REC_SLOT_CTRL*
					    N_REC_SLOT_CTRL_BANK,
					BUS_DMASYNC_POSTWRITE|
					BUS_DMASYNC_POSTREAD);
			dma = le32toh(sc->rbank[YDS_INPUT_SLOT*2 + nbank].pgstartadr);
			ccpu = sc->sc_rec.offset;
			blk = sc->sc_rec.blksize;
			len = sc->sc_rec.length;

			if (((dma > ccpu) && (dma - ccpu > blk * 2)) ||
			    ((ccpu > dma) && (dma + len - ccpu > blk * 2))) {
				/* We can drain the current block */
				/* Sync ring buffer first */
				bus_dmamap_sync(sc->sc_dmatag,
						sc->sc_rec.dma->map,
						ccpu, blk,
						BUS_DMASYNC_POSTREAD);
				sc->sc_rec.intr(sc->sc_rec.intr_arg);
				sc->sc_rec.offset += blk;
				if (sc->sc_rec.offset >= len) {
					sc->sc_rec.offset -= len;
#ifdef DIAGNOSTIC
					if (sc->sc_rec.offset != 0)
						printf ("Audio ringbuffer botch\n");
#endif
				}
				/* Sync ring buffer for next read */
				bus_dmamap_sync(sc->sc_dmatag,
						sc->sc_rec.dma->map,
						ccpu, blk,
						BUS_DMASYNC_PREREAD);
			}
		}
	}

	mutex_spin_exit(&sc->sc_intr_lock);
	return 1;
}

static int
yds_allocmem(struct yds_softc *sc, size_t size, size_t align, struct yds_dma *p)
{
	int error;

	p->size = size;
	error = bus_dmamem_alloc(sc->sc_dmatag, p->size, align, 0,
				 p->segs, sizeof(p->segs)/sizeof(p->segs[0]),
				 &p->nsegs, BUS_DMA_WAITOK);
	if (error)
		return error;

	error = bus_dmamem_map(sc->sc_dmatag, p->segs, p->nsegs, p->size,
			       &p->addr, BUS_DMA_WAITOK|BUS_DMA_COHERENT);
	if (error)
		goto free;

	error = bus_dmamap_create(sc->sc_dmatag, p->size, 1, p->size,
				  0, BUS_DMA_WAITOK, &p->map);
	if (error)
		goto unmap;

	error = bus_dmamap_load(sc->sc_dmatag, p->map, p->addr, p->size, NULL,
				BUS_DMA_WAITOK);
	if (error)
		goto destroy;
	return 0;

destroy:
	bus_dmamap_destroy(sc->sc_dmatag, p->map);
unmap:
	bus_dmamem_unmap(sc->sc_dmatag, p->addr, p->size);
free:
	bus_dmamem_free(sc->sc_dmatag, p->segs, p->nsegs);
	return error;
}

static int
yds_freemem(struct yds_softc *sc, struct yds_dma *p)
{

	bus_dmamap_unload(sc->sc_dmatag, p->map);
	bus_dmamap_destroy(sc->sc_dmatag, p->map);
	bus_dmamem_unmap(sc->sc_dmatag, p->addr, p->size);
	bus_dmamem_free(sc->sc_dmatag, p->segs, p->nsegs);
	return 0;
}

static int
yds_open(void *addr, int flags)
{
	struct yds_softc *sc;
	uint32_t mode;

	sc = addr;
	/* Select bank 0. */
	YWRITE4(sc, YDS_CONTROL_SELECT, 0);

	/* Start the DSP operation. */
	mode = YREAD4(sc, YDS_MODE);
	mode |= YDS_MODE_ACTV;
	mode &= ~YDS_MODE_ACTV2;
	YWRITE4(sc, YDS_MODE, mode);

	return 0;
}

static void
yds_close(void *addr)
{

	yds_halt(addr);
}

static int
yds_query_encoding(void *addr, struct audio_encoding *fp)
{
	struct yds_softc *sc;

	sc = addr;
	return auconv_query_encoding(sc->sc_encodings, fp);
}

static int
yds_set_params(void *addr, int setmode, int usemode,
	       audio_params_t *play, audio_params_t* rec,
	       stream_filter_list_t *pfil, stream_filter_list_t *rfil)
{
	if (setmode & AUMODE_RECORD) {
		if (auconv_set_converter(yds_formats, YDS_NFORMATS,
					 AUMODE_RECORD, rec, FALSE, rfil) < 0)
			return EINVAL;
	}
	if (setmode & AUMODE_PLAY) {
		if (auconv_set_converter(yds_formats, YDS_NFORMATS,
					 AUMODE_PLAY, play, FALSE, pfil) < 0)
			return EINVAL;
	}
	return 0;
}

static int
yds_round_blocksize(void *addr, int blk, int mode,
    const audio_params_t *param)
{

	/*
	 * Block size must be bigger than a frame.
	 * That is 1024bytes at most, i.e. for 48000Hz, 16bit, 2ch.
	 */
	if (blk < 1024)
		blk = 1024;

	return blk & ~4;
}

static uint32_t
yds_get_lpfq(u_int sample_rate)
{
	int i;
	static struct lpfqt {
		u_int rate;
		uint32_t lpfq;
	} lpfqt[] = {
		{8000,  0x32020000},
		{11025, 0x31770000},
		{16000, 0x31390000},
		{22050, 0x31c90000},
		{32000, 0x33d00000},
		{48000, 0x40000000},
		{0, 0}
	};

	if (sample_rate == 44100)		/* for P44 slot? */
		return 0x370A0000;

	for (i = 0; lpfqt[i].rate != 0; i++)
		if (sample_rate <= lpfqt[i].rate)
			break;

	return lpfqt[i].lpfq;
}

static uint32_t
yds_get_lpfk(u_int sample_rate)
{
	int i;
	static struct lpfkt {
		u_int rate;
		uint32_t lpfk;
	} lpfkt[] = {
		{8000,  0x18b20000},
		{11025, 0x20930000},
		{16000, 0x2b9a0000},
		{22050, 0x35a10000},
		{32000, 0x3eaa0000},
		{48000, 0x40000000},
		{0, 0}
	};

	if (sample_rate == 44100)		/* for P44 slot? */
		return 0x46460000;

	for (i = 0; lpfkt[i].rate != 0; i++)
		if (sample_rate <= lpfkt[i].rate)
			break;

	return lpfkt[i].lpfk;
}

static int
yds_trigger_output(void *addr, void *start, void *end, int blksize,
		   void (*intr)(void *), void *arg, const audio_params_t *param)
#define P44		(sc->sc_flags & YDS_CAP_HAS_P44)
{
	struct yds_softc *sc;
	struct yds_dma *p;
	struct play_slot_ctrl_bank *psb;
	const u_int gain = 0x40000000;
	bus_addr_t s;
	size_t l;
	int i;
	int p44, channels;
	uint32_t format;

	sc = addr;
#ifdef DIAGNOSTIC
	if (sc->sc_play.intr)
		panic("yds_trigger_output: already running");
#endif

	sc->sc_play.intr = intr;
	sc->sc_play.intr_arg = arg;
	sc->sc_play.offset = 0;
	sc->sc_play.blksize = blksize;

	DPRINTFN(1, ("yds_trigger_output: sc=%p start=%p end=%p "
	    "blksize=%d intr=%p(%p)\n", addr, start, end, blksize, intr, arg));

	p = yds_find_dma(sc, start);
	if (!p) {
		printf("yds_trigger_output: bad addr %p\n", start);
		return EINVAL;
	}
	sc->sc_play.dma = p;

#ifdef YDS_USE_P44
	/* The document says the P44 SRC supports only stereo, 16bit PCM. */
	if (P44)
		p44 = ((param->sample_rate == 44100) &&
		       (param->channels == 2) &&
		       (param->precision == 16));
	else
#endif
		p44 = 0;
	channels = p44 ? 1 : param->channels;

	s = DMAADDR(p);
	l = ((char *)end - (char *)start);
	sc->sc_play.length = l;

	*sc->ptbl = htole32(channels);	/* Num of play */

	sc->sc_play.factor = 1;
	if (param->channels == 2)
		sc->sc_play.factor *= 2;
	if (param->precision != 8)
		sc->sc_play.factor *= 2;
	l /= sc->sc_play.factor;

	format = ((channels == 2 ? PSLT_FORMAT_STEREO : 0) |
		  (param->precision == 8 ? PSLT_FORMAT_8BIT : 0) |
		  (p44 ? PSLT_FORMAT_SRC441 : 0));

	psb = sc->pbankp[0];
	memset(psb, 0, sizeof(*psb));
	psb->format = htole32(format);
	psb->pgbase = htole32(s);
	psb->pgloopend = htole32(l);
	if (!p44) {
		psb->pgdeltaend = htole32((param->sample_rate * 65536 / 48000) << 12);
		psb->lpfkend = htole32(yds_get_lpfk(param->sample_rate));
		psb->eggainend = htole32(gain);
		psb->lpfq = htole32(yds_get_lpfq(param->sample_rate));
		psb->pgdelta = htole32(psb->pgdeltaend);
		psb->lpfk = htole32(yds_get_lpfk(param->sample_rate));
		psb->eggain = htole32(gain);
	}

	for (i = 0; i < channels; i++) {
		/* i == 0: left or mono, i == 1: right */
		psb = sc->pbankp[i*2];
		if (i)
			/* copy from left */
			*psb = *(sc->pbankp[0]);
		if (channels == 2) {
			/* stereo */
			if (i == 0) {
				psb->lchgain = psb->lchgainend = htole32(gain);
			} else {
				psb->lchgain = psb->lchgainend = 0;
				psb->rchgain = psb->rchgainend = htole32(gain);
				psb->format |= htole32(PSLT_FORMAT_RCH);
			}
		} else if (!p44) {
			/* mono */
			psb->lchgain = psb->rchgain = htole32(gain);
			psb->lchgainend = psb->rchgainend = htole32(gain);
		}
		/* copy to the other bank */
		*(sc->pbankp[i*2+1]) = *psb;
	}

	YDS_DUMP_PLAY_SLOT(5, sc, 0);
	YDS_DUMP_PLAY_SLOT(5, sc, 1);

	if (p44)
		YWRITE4(sc, YDS_P44_OUT_VOLUME, 0x3fff3fff);
	else
		YWRITE4(sc, YDS_DAC_OUT_VOLUME, 0x3fff3fff);

	/* Now the play slot for the next frame is set up!! */
	/* Sync play slot control data for both directions */
	bus_dmamap_sync(sc->sc_dmatag, sc->sc_ctrldata.map,
			sc->ptbloff,
			sizeof(struct play_slot_ctrl_bank) *
			    channels * N_PLAY_SLOT_CTRL_BANK,
			BUS_DMASYNC_PREWRITE|BUS_DMASYNC_PREREAD);
	/* Sync ring buffer */
	bus_dmamap_sync(sc->sc_dmatag, p->map, 0, blksize,
			BUS_DMASYNC_PREWRITE);
	/* HERE WE GO!! */
	YWRITE4(sc, YDS_MODE,
		YREAD4(sc, YDS_MODE) | YDS_MODE_ACTV | YDS_MODE_ACTV2);

	return 0;
}
#undef P44

static int
yds_trigger_input(void *addr, void *start, void *end, int blksize,
		  void (*intr)(void *), void *arg, const audio_params_t *param)
{
	struct yds_softc *sc;
	struct yds_dma *p;
	u_int srate, format;
	struct rec_slot_ctrl_bank *rsb;
	bus_addr_t s;
	size_t l;

	sc = addr;
#ifdef DIAGNOSTIC
	if (sc->sc_rec.intr)
		panic("yds_trigger_input: already running");
#endif
	sc->sc_rec.intr = intr;
	sc->sc_rec.intr_arg = arg;
	sc->sc_rec.offset = 0;
	sc->sc_rec.blksize = blksize;

	DPRINTFN(1, ("yds_trigger_input: "
	    "sc=%p start=%p end=%p blksize=%d intr=%p(%p)\n",
	    addr, start, end, blksize, intr, arg));
	DPRINTFN(1, (" parameters: rate=%u, precision=%u, channels=%u\n",
	    param->sample_rate, param->precision, param->channels));

	p = yds_find_dma(sc, start);
	if (!p) {
		printf("yds_trigger_input: bad addr %p\n", start);
		return EINVAL;
	}
	sc->sc_rec.dma = p;

	s = DMAADDR(p);
	l = ((char *)end - (char *)start);
	sc->sc_rec.length = l;

	sc->sc_rec.factor = 1;
	if (param->channels == 2)
		sc->sc_rec.factor *= 2;
	if (param->precision != 8)
		sc->sc_rec.factor *= 2;

	rsb = &sc->rbank[0];
	memset(rsb, 0, sizeof(*rsb));
	rsb->pgbase = htole32(s);
	rsb->pgloopendadr = htole32(l);
	/* Seems all 4 banks must be set up... */
	sc->rbank[1] = *rsb;
	sc->rbank[2] = *rsb;
	sc->rbank[3] = *rsb;

	YWRITE4(sc, YDS_ADC_IN_VOLUME, 0x3fff3fff);
	YWRITE4(sc, YDS_REC_IN_VOLUME, 0x3fff3fff);
	srate = 48000 * 4096 / param->sample_rate - 1;
	format = ((param->precision == 8 ? YDS_FORMAT_8BIT : 0) |
		  (param->channels == 2 ? YDS_FORMAT_STEREO : 0));
	DPRINTF(("srate=%d, format=%08x\n", srate, format));
#ifdef YDS_USE_REC_SLOT
	YWRITE4(sc, YDS_DAC_REC_VOLUME, 0x3fff3fff);
	YWRITE4(sc, YDS_P44_REC_VOLUME, 0x3fff3fff);
	YWRITE4(sc, YDS_MAPOF_REC, YDS_RECSLOT_VALID);
	YWRITE4(sc, YDS_REC_SAMPLE_RATE, srate);
	YWRITE4(sc, YDS_REC_FORMAT, format);
#else
	YWRITE4(sc, YDS_MAPOF_REC, YDS_ADCSLOT_VALID);
	YWRITE4(sc, YDS_ADC_SAMPLE_RATE, srate);
	YWRITE4(sc, YDS_ADC_FORMAT, format);
#endif
	/* Now the rec slot for the next frame is set up!! */
	/* Sync record slot control data */
	bus_dmamap_sync(sc->sc_dmatag, sc->sc_ctrldata.map,
			sc->rbankoff,
			sizeof(struct rec_slot_ctrl_bank)*
			    N_REC_SLOT_CTRL*
			    N_REC_SLOT_CTRL_BANK,
			BUS_DMASYNC_PREWRITE|BUS_DMASYNC_PREREAD);
	/* Sync ring buffer */
	bus_dmamap_sync(sc->sc_dmatag, p->map, 0, blksize,
			BUS_DMASYNC_PREREAD);
	/* HERE WE GO!! */
	YWRITE4(sc, YDS_MODE,
		YREAD4(sc, YDS_MODE) | YDS_MODE_ACTV | YDS_MODE_ACTV2);

	return 0;
}

static int
yds_halt(struct yds_softc *sc)
{
	uint32_t mode;

	/* Stop the DSP operation. */
	mode = YREAD4(sc, YDS_MODE);
	YWRITE4(sc, YDS_MODE, mode & ~(YDS_MODE_ACTV|YDS_MODE_ACTV2));

	/* Paranoia...  mute all */
	YWRITE4(sc, YDS_P44_OUT_VOLUME, 0);
	YWRITE4(sc, YDS_DAC_OUT_VOLUME, 0);
	YWRITE4(sc, YDS_ADC_IN_VOLUME, 0);
	YWRITE4(sc, YDS_REC_IN_VOLUME, 0);
	YWRITE4(sc, YDS_DAC_REC_VOLUME, 0);
	YWRITE4(sc, YDS_P44_REC_VOLUME, 0);

	return 0;
}

static int
yds_halt_output(void *addr)
{
	struct yds_softc *sc;

	DPRINTF(("yds: yds_halt_output\n"));
	sc = addr;
	if (sc->sc_play.intr) {
		sc->sc_play.intr = 0;
		/* Sync play slot control data */
		bus_dmamap_sync(sc->sc_dmatag, sc->sc_ctrldata.map,
				sc->pbankoff,
				sizeof(struct play_slot_ctrl_bank)*
				    (*sc->ptbl)*N_PLAY_SLOT_CTRL_BANK,
				BUS_DMASYNC_POSTWRITE|BUS_DMASYNC_POSTREAD);
		/* Stop the play slot operation */
		sc->pbankp[0]->status =
		sc->pbankp[1]->status =
		sc->pbankp[2]->status =
		sc->pbankp[3]->status = 1;
		/* Sync ring buffer */
		bus_dmamap_sync(sc->sc_dmatag, sc->sc_play.dma->map,
				0, sc->sc_play.length, BUS_DMASYNC_POSTWRITE);
	}

	return 0;
}

static int
yds_halt_input(void *addr)
{
	struct yds_softc *sc;

	DPRINTF(("yds: yds_halt_input\n"));
	sc = addr;
	sc->sc_rec.intr = NULL;
	if (sc->sc_rec.intr) {
		/* Stop the rec slot operation */
		YWRITE4(sc, YDS_MAPOF_REC, 0);
		sc->sc_rec.intr = 0;
		/* Sync rec slot control data */
		bus_dmamap_sync(sc->sc_dmatag, sc->sc_ctrldata.map,
				sc->rbankoff,
				sizeof(struct rec_slot_ctrl_bank)*
				    N_REC_SLOT_CTRL*N_REC_SLOT_CTRL_BANK,
				BUS_DMASYNC_POSTWRITE|BUS_DMASYNC_POSTREAD);
		/* Sync ring buffer */
		bus_dmamap_sync(sc->sc_dmatag, sc->sc_rec.dma->map,
				0, sc->sc_rec.length, BUS_DMASYNC_POSTREAD);
	}

	return 0;
}

static int
yds_getdev(void *addr, struct audio_device *retp)
{

	*retp = yds_device;
	return 0;
}

static int
yds_mixer_set_port(void *addr, mixer_ctrl_t *cp)
{
	struct yds_softc *sc;

	sc = addr;
	return sc->sc_codec[0].codec_if->vtbl->mixer_set_port(
	    sc->sc_codec[0].codec_if, cp);
}

static int
yds_mixer_get_port(void *addr, mixer_ctrl_t *cp)
{
	struct yds_softc *sc;

	sc = addr;
	return sc->sc_codec[0].codec_if->vtbl->mixer_get_port(
	    sc->sc_codec[0].codec_if, cp);
}

static int
yds_query_devinfo(void *addr, mixer_devinfo_t *dip)
{
	struct yds_softc *sc;

	sc = addr;
	return sc->sc_codec[0].codec_if->vtbl->query_devinfo(
	    sc->sc_codec[0].codec_if, dip);
}

static void *
yds_malloc(void *addr, int direction, size_t size)
{
	struct yds_softc *sc;
	struct yds_dma *p;
	int error;

	p = kmem_alloc(sizeof(*p), KM_SLEEP);
	if (p == NULL)
		return NULL;
	sc = addr;
	error = yds_allocmem(sc, size, 16, p);
	if (error) {
		kmem_free(p, sizeof(*p));
		return NULL;
	}
	p->next = sc->sc_dmas;
	sc->sc_dmas = p;
	return KERNADDR(p);
}

static void
yds_free(void *addr, void *ptr, size_t size)
{
	struct yds_softc *sc;
	struct yds_dma **pp, *p;

	sc = addr;
	for (pp = &sc->sc_dmas; (p = *pp) != NULL; pp = &p->next) {
		if (KERNADDR(p) == ptr) {
			yds_freemem(sc, p);
			*pp = p->next;
			kmem_free(p, sizeof(*p));
			return;
		}
	}
}

static struct yds_dma *
yds_find_dma(struct yds_softc *sc, void *addr)
{
	struct yds_dma *p;

	for (p = sc->sc_dmas; p && KERNADDR(p) != addr; p = p->next)
		continue;

	return p;
}

static size_t
yds_round_buffersize(void *addr, int direction, size_t size)
{

	/*
	 * Buffer size should be at least twice as bigger as a frame.
	 */
	if (size < 1024 * 3)
		size = 1024 * 3;
	return size;
}

static paddr_t
yds_mappage(void *addr, void *mem, off_t off, int prot)
{
	struct yds_softc *sc;
	struct yds_dma *p;

	if (off < 0)
		return -1;
	sc = addr;
	p = yds_find_dma(sc, mem);
	if (p == NULL)
		return -1;
	return bus_dmamem_mmap(sc->sc_dmatag, p->segs, p->nsegs,
	    off, prot, BUS_DMA_WAITOK);
}

static int
yds_get_props(void *addr)
{

	return AUDIO_PROP_MMAP | AUDIO_PROP_INDEPENDENT |
	    AUDIO_PROP_FULLDUPLEX;
}

static void
yds_get_locks(void *addr, kmutex_t **intr, kmutex_t **thread)
{
	struct yds_softc *sc;

	sc = addr;
	*intr = &sc->sc_intr_lock;
	*thread = &sc->sc_lock;
}
