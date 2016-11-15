/*	$NetBSD: auacer.c,v 1.32 2014/03/29 19:28:24 christos Exp $	*/

/*-
 * Copyright (c) 2004, 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Acer Labs M5455 audio driver
 *
 * Acer provides data sheets after signing an NDA, so this is guess work.
 * The chip behaves somewhat like the Intel i8x0, so this driver
 * is loosely based on the auich driver.  Additional information taken from
 * the ALSA intel8x0.c driver (which handles M5455 as well).
 *
 * As an historical note one can observe that the auich driver borrows
 * lot from the first NetBSD PCI audio driver, the eap driver.  But this
 * is not attributed anywhere.
 */


#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: auacer.c,v 1.32 2014/03/29 19:28:24 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/kmem.h>
#include <sys/device.h>
#include <sys/fcntl.h>
#include <sys/proc.h>

#include <dev/pci/pcidevs.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/auacerreg.h>

#include <sys/audioio.h>
#include <dev/audio_if.h>
#include <dev/mulaw.h>
#include <dev/auconv.h>

#include <sys/bus.h>

#include <dev/ic/ac97reg.h>
#include <dev/ic/ac97var.h>

struct auacer_dma {
	bus_dmamap_t map;
	void *addr;
	bus_dma_segment_t segs[1];
	int nsegs;
	size_t size;
	struct auacer_dma *next;
};

#define	DMAADDR(p)	((p)->map->dm_segs[0].ds_addr)
#define	KERNADDR(p)	((void *)((p)->addr))

struct auacer_cdata {
	struct auacer_dmalist ic_dmalist_pcmo[ALI_DMALIST_MAX];
};

struct auacer_chan {
	uint32_t ptr;
	uint32_t start, p, end;
	uint32_t blksize, fifoe;
	uint32_t ack;
	uint32_t port;
	struct auacer_dmalist *dmalist;
	void (*intr)(void *);
	void *arg;
};

struct auacer_softc {
	device_t sc_dev;
	void *sc_ih;
	kmutex_t sc_lock;
	kmutex_t sc_intr_lock;

	audio_device_t sc_audev;

	bus_space_tag_t iot;
	bus_space_handle_t mix_ioh;
	bus_space_handle_t aud_ioh;
	bus_dma_tag_t dmat;

	struct ac97_codec_if *codec_if;
	struct ac97_host_if host_if;

	/* DMA scatter-gather lists. */
	bus_dmamap_t sc_cddmamap;
#define	sc_cddma	sc_cddmamap->dm_segs[0].ds_addr

	struct auacer_cdata *sc_cdata;

	struct auacer_chan sc_pcmo;

	struct auacer_dma *sc_dmas;

	pci_chipset_tag_t sc_pc;
	pcitag_t sc_pt;

	int  sc_dmamap_flags;

#define AUACER_NFORMATS	3
	struct audio_format sc_formats[AUACER_NFORMATS];
	struct audio_encoding_set *sc_encodings;
};

#define READ1(sc, a) bus_space_read_1(sc->iot, sc->aud_ioh, a)
#define READ2(sc, a) bus_space_read_2(sc->iot, sc->aud_ioh, a)
#define READ4(sc, a) bus_space_read_4(sc->iot, sc->aud_ioh, a)
#define WRITE1(sc, a, v) bus_space_write_1(sc->iot, sc->aud_ioh, a, v)
#define WRITE2(sc, a, v) bus_space_write_2(sc->iot, sc->aud_ioh, a, v)
#define WRITE4(sc, a, v) bus_space_write_4(sc->iot, sc->aud_ioh, a, v)

/* Debug */
#ifdef AUACER_DEBUG
#define	DPRINTF(l,x)	do { if (auacer_debug & (l)) printf x; } while(0)
int auacer_debug = 0;
#define	ALI_DEBUG_CODECIO	0x0001
#define	ALI_DEBUG_DMA		0x0002
#define	ALI_DEBUG_INTR		0x0004
#define ALI_DEBUG_API		0x0008
#define ALI_DEBUG_MIXERAPI	0x0010
#else
#define	DPRINTF(x,y)	/* nothing */
#endif

static int	auacer_intr(void *);

static int	auacer_query_encoding(void *, struct audio_encoding *);
static int	auacer_set_params(void *, int, int, audio_params_t *,
				  audio_params_t *, stream_filter_list_t *,
				  stream_filter_list_t *);
static int	auacer_round_blocksize(void *, int, int,
				       const audio_params_t *);
static int	auacer_halt_output(void *);
static int	auacer_halt_input(void *);
static int	auacer_getdev(void *, struct audio_device *);
static int	auacer_set_port(void *, mixer_ctrl_t *);
static int	auacer_get_port(void *, mixer_ctrl_t *);
static int	auacer_query_devinfo(void *, mixer_devinfo_t *);
static void	*auacer_allocm(void *, int, size_t);
static void	auacer_freem(void *, void *, size_t);
static size_t	auacer_round_buffersize(void *, int, size_t);
static paddr_t	auacer_mappage(void *, void *, off_t, int);
static int	auacer_get_props(void *);
static int	auacer_trigger_output(void *, void *, void *, int,
				      void (*)(void *), void *,
				      const audio_params_t *);
static int	auacer_trigger_input(void *, void *, void *, int,
				     void (*)(void *), void *,
				     const audio_params_t *);

static int	auacer_alloc_cdata(struct auacer_softc *);

static int	auacer_allocmem(struct auacer_softc *, size_t, size_t,
				struct auacer_dma *);
static int	auacer_freemem(struct auacer_softc *, struct auacer_dma *);
static void	auacer_get_locks(void *, kmutex_t **, kmutex_t **);

static bool	auacer_resume(device_t, const pmf_qual_t *);
static int	auacer_set_rate(struct auacer_softc *, int, u_int);

static void auacer_reset(struct auacer_softc *sc);

static struct audio_hw_if auacer_hw_if = {
	NULL,			/* open */
	NULL,			/* close */
	NULL,			/* drain */
	auacer_query_encoding,
	auacer_set_params,
	auacer_round_blocksize,
	NULL,			/* commit_setting */
	NULL,			/* init_output */
	NULL,			/* init_input */
	NULL,			/* start_output */
	NULL,			/* start_input */
	auacer_halt_output,
	auacer_halt_input,
	NULL,			/* speaker_ctl */
	auacer_getdev,
	NULL,			/* getfd */
	auacer_set_port,
	auacer_get_port,
	auacer_query_devinfo,
	auacer_allocm,
	auacer_freem,
	auacer_round_buffersize,
	auacer_mappage,
	auacer_get_props,
	auacer_trigger_output,
	auacer_trigger_input,
	NULL,			/* dev_ioctl */
	auacer_get_locks,
};

#define AUACER_FORMATS_4CH	1
#define AUACER_FORMATS_6CH	2
static const struct audio_format auacer_formats[AUACER_NFORMATS] = {
	{NULL, AUMODE_PLAY | AUMODE_RECORD, AUDIO_ENCODING_SLINEAR_LE, 16, 16,
	 2, AUFMT_STEREO, 0, {8000, 48000}},
	{NULL, AUMODE_PLAY, AUDIO_ENCODING_SLINEAR_LE, 16, 16,
	 4, AUFMT_SURROUND4, 0, {8000, 48000}},
	{NULL, AUMODE_PLAY, AUDIO_ENCODING_SLINEAR_LE, 16, 16,
	 6, AUFMT_DOLBY_5_1, 0, {8000, 48000}},
};

static int	auacer_attach_codec(void *, struct ac97_codec_if *);
static int	auacer_read_codec(void *, uint8_t, uint16_t *);
static int	auacer_write_codec(void *, uint8_t, uint16_t);
static int	auacer_reset_codec(void *);

static int
auacer_match(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa;

	pa = (struct pci_attach_args *)aux;
	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_ALI &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_ALI_M5455)
		return 1;
	return 0;
}

static void
auacer_attach(device_t parent, device_t self, void *aux)
{
	struct auacer_softc *sc;
	struct pci_attach_args *pa;
	pci_intr_handle_t ih;
	bus_size_t aud_size;
	pcireg_t v;
	const char *intrstr;
	int i;
	char intrbuf[PCI_INTRSTR_LEN];

	sc = device_private(self);
	sc->sc_dev = self;
	pa = aux;
	aprint_normal(": Acer Labs M5455 Audio controller\n");

	if (pci_mapreg_map(pa, 0x10, PCI_MAPREG_TYPE_IO, 0, &sc->iot,
		&sc->aud_ioh, NULL, &aud_size)) {
		aprint_error(": can't map i/o space\n");
		return;
	}

	sc->sc_pc = pa->pa_pc;
	sc->sc_pt = pa->pa_tag;
	sc->dmat = pa->pa_dmat;

	sc->sc_dmamap_flags = BUS_DMA_COHERENT;	/* XXX remove */

	mutex_init(&sc->sc_lock, MUTEX_DEFAULT, IPL_NONE);
	mutex_init(&sc->sc_intr_lock, MUTEX_DEFAULT, IPL_AUDIO);

	/* enable bus mastering */
	v = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG,
	    v | PCI_COMMAND_MASTER_ENABLE);

	/* Map and establish the interrupt. */
	if (pci_intr_map(pa, &ih)) {
		aprint_error_dev(sc->sc_dev, "can't map interrupt\n");
		mutex_destroy(&sc->sc_lock);
		mutex_destroy(&sc->sc_intr_lock);
		return;
	}
	intrstr = pci_intr_string(pa->pa_pc, ih, intrbuf, sizeof(intrbuf));
	sc->sc_ih = pci_intr_establish(pa->pa_pc, ih, IPL_AUDIO,
	    auacer_intr, sc);
	if (sc->sc_ih == NULL) {
		aprint_error_dev(sc->sc_dev, "can't establish interrupt");
		if (intrstr != NULL)
			aprint_error(" at %s", intrstr);
		aprint_error("\n");
		mutex_destroy(&sc->sc_lock);
		mutex_destroy(&sc->sc_intr_lock);
		return;
	}
	aprint_normal_dev(sc->sc_dev, "interrupting at %s\n", intrstr);

	strlcpy(sc->sc_audev.name, "M5455 AC97", MAX_AUDIO_DEV_LEN);
	snprintf(sc->sc_audev.version, MAX_AUDIO_DEV_LEN,
		 "0x%02x", PCI_REVISION(pa->pa_class));
	strlcpy(sc->sc_audev.config, device_xname(sc->sc_dev), MAX_AUDIO_DEV_LEN);

	/* Set up DMA lists. */
	auacer_alloc_cdata(sc);
	sc->sc_pcmo.dmalist = sc->sc_cdata->ic_dmalist_pcmo;
	sc->sc_pcmo.ptr = 0;
	sc->sc_pcmo.port = ALI_BASE_PO;

	DPRINTF(ALI_DEBUG_DMA, ("auacer_attach: lists %p\n",
	    sc->sc_pcmo.dmalist));

	sc->host_if.arg = sc;
	sc->host_if.attach = auacer_attach_codec;
	sc->host_if.read = auacer_read_codec;
	sc->host_if.write = auacer_write_codec;
	sc->host_if.reset = auacer_reset_codec;

	if (ac97_attach(&sc->host_if, self, &sc->sc_lock) != 0) {
		mutex_destroy(&sc->sc_lock);
		mutex_destroy(&sc->sc_intr_lock);
		return;
	}

	/* setup audio_format */
	memcpy(sc->sc_formats, auacer_formats, sizeof(auacer_formats));
	mutex_enter(&sc->sc_lock);
	if (!AC97_IS_4CH(sc->codec_if))
		AUFMT_INVALIDATE(&sc->sc_formats[AUACER_FORMATS_4CH]);
	if (!AC97_IS_6CH(sc->codec_if))
		AUFMT_INVALIDATE(&sc->sc_formats[AUACER_FORMATS_6CH]);
	if (AC97_IS_FIXED_RATE(sc->codec_if)) {
		for (i = 0; i < AUACER_NFORMATS; i++) {
			sc->sc_formats[i].frequency_type = 1;
			sc->sc_formats[i].frequency[0] = 48000;
		}
	}
	mutex_exit(&sc->sc_lock);

	if (0 != auconv_create_encodings(sc->sc_formats, AUACER_NFORMATS,
					 &sc->sc_encodings)) {
		mutex_destroy(&sc->sc_lock);
		mutex_destroy(&sc->sc_intr_lock);
		return;
	}

	mutex_enter(&sc->sc_lock);
	mutex_spin_enter(&sc->sc_intr_lock);
	auacer_reset(sc);
	mutex_spin_exit(&sc->sc_intr_lock);
	mutex_exit(&sc->sc_lock);

	audio_attach_mi(&auacer_hw_if, sc, sc->sc_dev);

	if (!pmf_device_register(self, NULL, auacer_resume))
		aprint_error_dev(self, "couldn't establish power handler\n");
}

CFATTACH_DECL_NEW(auacer, sizeof(struct auacer_softc),
    auacer_match, auacer_attach, NULL, NULL);

static int
auacer_ready_codec(struct auacer_softc *sc, int mask)
{
	int count;

	for (count = 0; count < 0x7f; count++) {
		int val = READ1(sc, ALI_CSPSR);
		if (val & mask)
			return 0;
	}

	aprint_normal("auacer_ready_codec: AC97 codec ready timeout.\n");
	return EBUSY;
}

static int
auacer_sema_codec(struct auacer_softc *sc)
{
	int ttime;

	ttime = 100;
	while (ttime-- && (READ4(sc, ALI_CAS) & ALI_CAS_SEM_BUSY))
		delay(1);
	if (!ttime)
		aprint_normal("auacer_sema_codec: timeout\n");
	return auacer_ready_codec(sc, ALI_CSPSR_CODEC_READY);
}

static int
auacer_read_codec(void *v, uint8_t reg, uint16_t *val)
{
	struct auacer_softc *sc;

	sc = v;
	if (auacer_sema_codec(sc))
		return EIO;

	reg |= ALI_CPR_ADDR_READ;
#if 0
	if (ac97->num)
		reg |= ALI_CPR_ADDR_SECONDARY;
#endif
	WRITE2(sc, ALI_CPR_ADDR, reg);
	if (auacer_ready_codec(sc, ALI_CSPSR_READ_OK))
		return EIO;
	*val = READ2(sc, ALI_SPR);

	DPRINTF(ALI_DEBUG_CODECIO, ("auacer_read_codec: reg=0x%x val=0x%x\n",
				    reg, *val));

	return 0;
}

int
auacer_write_codec(void *v, uint8_t reg, uint16_t val)
{
	struct auacer_softc *sc;

	DPRINTF(ALI_DEBUG_CODECIO, ("auacer_write_codec: reg=0x%x val=0x%x\n",
				    reg, val));
	sc = v;
	if (auacer_sema_codec(sc))
		return EIO;
	WRITE2(sc, ALI_CPR, val);
#if 0
	if (ac97->num)
		reg |= ALI_CPR_ADDR_SECONDARY;
#endif
	WRITE2(sc, ALI_CPR_ADDR, reg);
	auacer_ready_codec(sc, ALI_CSPSR_WRITE_OK);
	return 0;
}

static int
auacer_attach_codec(void *v, struct ac97_codec_if *cif)
{
	struct auacer_softc *sc;

	sc = v;
	sc->codec_if = cif;
	return 0;
}

static int
auacer_reset_codec(void *v)
{
	struct auacer_softc *sc;
	uint32_t reg;
	int i;

	sc = v;
	i = 0;
	reg = READ4(sc, ALI_SCR);
	if ((reg & 2) == 0)	/* Cold required */
		reg |= 2;
	else
		reg |= 1;	/* Warm */
	reg &= ~0x80000000;	/* ACLink on */
	WRITE4(sc, ALI_SCR, reg);

	while (i < 10) {
		if ((READ4(sc, ALI_INTERRUPTSR) & ALI_INT_GPIO) == 0)
			break;
		delay(50000);	/* XXX */
		i++;
	}
	if (i == 10) {
		return EIO;
	}

	for (i = 0; i < 10; i++) {
		reg = READ4(sc, ALI_RTSR);
		if (reg & 0x80) /* primary codec */
			break;
		WRITE4(sc, ALI_RTSR, reg | 0x80);
		delay(50000);	/* XXX */
	}

	return 0;
}

static void
auacer_reset(struct auacer_softc *sc)
{
	WRITE4(sc, ALI_SCR, ALI_SCR_RESET);
	WRITE4(sc, ALI_FIFOCR1, 0x83838383);
	WRITE4(sc, ALI_FIFOCR2, 0x83838383);
	WRITE4(sc, ALI_FIFOCR3, 0x83838383);
	WRITE4(sc, ALI_INTERFACECR, ALI_IF_PO); /* XXX pcm out only */
	WRITE4(sc, ALI_INTERRUPTCR, 0x00000000);
	WRITE4(sc, ALI_INTERRUPTSR, 0x00000000);
}

static int
auacer_query_encoding(void *v, struct audio_encoding *aep)
{
	struct auacer_softc *sc;

	DPRINTF(ALI_DEBUG_API, ("auacer_query_encoding\n"));
	sc = v;
	return auconv_query_encoding(sc->sc_encodings, aep);
}

static int
auacer_set_rate(struct auacer_softc *sc, int mode, u_int srate)
{
	int ret;
	u_int ratetmp;

	DPRINTF(ALI_DEBUG_API, ("auacer_set_rate: srate=%u\n", srate));

	ratetmp = srate;
	if (mode == AUMODE_RECORD)
		return sc->codec_if->vtbl->set_rate(sc->codec_if,
		    AC97_REG_PCM_LR_ADC_RATE, &ratetmp);
	ret = sc->codec_if->vtbl->set_rate(sc->codec_if,
	    AC97_REG_PCM_FRONT_DAC_RATE, &ratetmp);
	if (ret)
		return ret;
	ratetmp = srate;
	ret = sc->codec_if->vtbl->set_rate(sc->codec_if,
	    AC97_REG_PCM_SURR_DAC_RATE, &ratetmp);
	if (ret)
		return ret;
	ratetmp = srate;
	ret = sc->codec_if->vtbl->set_rate(sc->codec_if,
	    AC97_REG_PCM_LFE_DAC_RATE, &ratetmp);
	return ret;
}

static int
auacer_set_params(void *v, int setmode, int usemode,
    audio_params_t *play, audio_params_t *rec, stream_filter_list_t *pfil,
    stream_filter_list_t *rfil)
{
	struct auacer_softc *sc;
	struct audio_params *p;
	stream_filter_list_t *fil;
	uint32_t control;
	int mode, index;

	DPRINTF(ALI_DEBUG_API, ("auacer_set_params\n"));
	sc = v;
	for (mode = AUMODE_RECORD; mode != -1;
	     mode = mode == AUMODE_RECORD ? AUMODE_PLAY : -1) {
		if ((setmode & mode) == 0)
			continue;

		p = mode == AUMODE_PLAY ? play : rec;
		if (p == NULL)
			continue;

		if ((p->sample_rate !=  8000) &&
		    (p->sample_rate != 11025) &&
		    (p->sample_rate != 12000) &&
		    (p->sample_rate != 16000) &&
		    (p->sample_rate != 22050) &&
		    (p->sample_rate != 24000) &&
		    (p->sample_rate != 32000) &&
		    (p->sample_rate != 44100) &&
		    (p->sample_rate != 48000))
			return (EINVAL);

		fil = mode == AUMODE_PLAY ? pfil : rfil;
		index = auconv_set_converter(sc->sc_formats, AUACER_NFORMATS,
					     mode, p, TRUE, fil);
		if (index < 0)
			return EINVAL;
		if (fil->req_size > 0)
			p = &fil->filters[0].param;
		/* p points HW encoding */
		if (sc->sc_formats[index].frequency_type != 1
		    && auacer_set_rate(sc, mode, p->sample_rate))
			return EINVAL;
		if (mode == AUMODE_PLAY) {
			control = READ4(sc, ALI_SCR);
			control &= ~ALI_SCR_PCM_246_MASK;
			if (p->channels == 4)
				control |= ALI_SCR_PCM_4;
			else if (p->channels == 6)
				control |= ALI_SCR_PCM_6;
			WRITE4(sc, ALI_SCR, control);
		}
	}

	return (0);
}

static int
auacer_round_blocksize(void *v, int blk, int mode,
    const audio_params_t *param)
{

	return blk & ~0x3f;		/* keep good alignment */
}

static void
auacer_halt(struct auacer_softc *sc, struct auacer_chan *chan)
{
	uint32_t val;
	uint8_t port;
	uint32_t slot;

	port = chan->port;
	DPRINTF(ALI_DEBUG_API, ("auacer_halt: port=0x%x\n", port));
	chan->intr = 0;

	slot = ALI_PORT2SLOT(port);

	val = READ4(sc, ALI_DMACR);
	val |= 1 << (slot+16); /* pause */
	val &= ~(1 << slot); /* no start */
	WRITE4(sc, ALI_DMACR, val);
	WRITE1(sc, port + ALI_OFF_CR, 0);
	while (READ1(sc, port + ALI_OFF_CR))
		;
	/* reset whole DMA things */
	WRITE1(sc, port + ALI_OFF_CR, ALI_CR_RR);
	/* clear interrupts */
	WRITE1(sc, port + ALI_OFF_SR, READ1(sc, port+ALI_OFF_SR) | ALI_SR_W1TC);
	WRITE4(sc, ALI_INTERRUPTSR, ALI_PORT2INTR(port));
}

static int
auacer_halt_output(void *v)
{
	struct auacer_softc *sc;

	DPRINTF(ALI_DEBUG_DMA, ("auacer_halt_output\n"));
	sc = v;
	auacer_halt(sc, &sc->sc_pcmo);

	return 0;
}

static int
auacer_halt_input(void *v)
{
	DPRINTF(ALI_DEBUG_DMA, ("auacer_halt_input\n"));

	return 0;
}

static int
auacer_getdev(void *v, struct audio_device *adp)
{
	struct auacer_softc *sc;

	DPRINTF(ALI_DEBUG_API, ("auacer_getdev\n"));
	sc = v;
	*adp = sc->sc_audev;
	return 0;
}

static int
auacer_set_port(void *v, mixer_ctrl_t *cp)
{
	struct auacer_softc *sc;

	DPRINTF(ALI_DEBUG_MIXERAPI, ("auacer_set_port\n"));
	sc = v;
	return sc->codec_if->vtbl->mixer_set_port(sc->codec_if, cp);
}

static int
auacer_get_port(void *v, mixer_ctrl_t *cp)
{
	struct auacer_softc *sc;

	DPRINTF(ALI_DEBUG_MIXERAPI, ("auacer_get_port\n"));
	sc = v;
	return sc->codec_if->vtbl->mixer_get_port(sc->codec_if, cp);
}

static int
auacer_query_devinfo(void *v, mixer_devinfo_t *dp)
{
	struct auacer_softc *sc;

	DPRINTF(ALI_DEBUG_MIXERAPI, ("auacer_query_devinfo\n"));
	sc = v;
	return sc->codec_if->vtbl->query_devinfo(sc->codec_if, dp);
}

static void *
auacer_allocm(void *v, int direction, size_t size)
{
	struct auacer_softc *sc;
	struct auacer_dma *p;
	int error;

	if (size > (ALI_DMALIST_MAX * ALI_DMASEG_MAX))
		return NULL;

	p = kmem_zalloc(sizeof(*p), KM_SLEEP);
	if (p == NULL)
		return NULL;
	sc = v;
	error = auacer_allocmem(sc, size, 0, p);
	if (error) {
		kmem_free(p, sizeof(*p));
		return NULL;
	}

	p->next = sc->sc_dmas;
	sc->sc_dmas = p;

	return KERNADDR(p);
}

static void
auacer_freem(void *v, void *ptr, size_t size)
{
	struct auacer_softc *sc;
	struct auacer_dma *p, **pp;

	sc = v;
	for (pp = &sc->sc_dmas; (p = *pp) != NULL; pp = &p->next) {
		if (KERNADDR(p) == ptr) {
			auacer_freemem(sc, p);
			*pp = p->next;
			kmem_free(p, sizeof(*p));
			return;
		}
	}
}

static size_t
auacer_round_buffersize(void *v, int direction, size_t size)
{

	if (size > (ALI_DMALIST_MAX * ALI_DMASEG_MAX))
		size = ALI_DMALIST_MAX * ALI_DMASEG_MAX;

	return size;
}

static paddr_t
auacer_mappage(void *v, void *mem, off_t off, int prot)
{
	struct auacer_softc *sc;
	struct auacer_dma *p;

	if (off < 0)
		return -1;
	sc = v;
	for (p = sc->sc_dmas; p && KERNADDR(p) != mem; p = p->next)
		continue;
	if (p == NULL)
		return -1;
	return bus_dmamem_mmap(sc->dmat, p->segs, p->nsegs,
	    off, prot, BUS_DMA_WAITOK);
}

static int
auacer_get_props(void *v)
{
	struct auacer_softc *sc;
	int props;

	sc = v;
	props = AUDIO_PROP_INDEPENDENT | AUDIO_PROP_FULLDUPLEX;
	/*
	 * Even if the codec is fixed-rate, set_param() succeeds for any sample
	 * rate because of aurateconv.  Applications can't know what rate the
	 * device can process in the case of mmap().
	 */
	if (!AC97_IS_FIXED_RATE(sc->codec_if))
		props |= AUDIO_PROP_MMAP;
	return props;
}

static void
auacer_get_locks(void *v, kmutex_t **intr, kmutex_t **proc)
{
	struct auacer_softc *sc;

	sc = v;
	*intr = &sc->sc_intr_lock;
	*proc = &sc->sc_lock;
}

static void
auacer_add_entry(struct auacer_chan *chan)
{
	struct auacer_dmalist *q;

	q = &chan->dmalist[chan->ptr];

	DPRINTF(ALI_DEBUG_INTR,
		("auacer_add_entry: %p = %x @ 0x%x\n",
		 q, chan->blksize / 2, chan->p));

	q->base = htole32(chan->p);
	q->len = htole32((chan->blksize / ALI_SAMPLE_SIZE) | ALI_DMAF_IOC);
	chan->p += chan->blksize;
	if (chan->p >= chan->end)
		chan->p = chan->start;

	if (++chan->ptr >= ALI_DMALIST_MAX)
		chan->ptr = 0;
}

static void
auacer_upd_chan(struct auacer_softc *sc, struct auacer_chan *chan)
{
	uint32_t sts;
	uint32_t civ;

	sts = READ2(sc, chan->port + ALI_OFF_SR);
	/* intr ack */
	WRITE2(sc, chan->port + ALI_OFF_SR, sts & ALI_SR_W1TC);
	WRITE4(sc, ALI_INTERRUPTSR, ALI_PORT2INTR(chan->port));

	DPRINTF(ALI_DEBUG_INTR, ("auacer_upd_chan: sts=0x%x\n", sts));

	if (sts & ALI_SR_DMA_INT_FIFO) {
		printf("%s: fifo underrun # %u\n",
		       device_xname(sc->sc_dev), ++chan->fifoe);
	}

	civ = READ1(sc, chan->port + ALI_OFF_CIV);

	DPRINTF(ALI_DEBUG_INTR,("auacer_intr: civ=%u ptr=%u\n",civ,chan->ptr));

	/* XXX */
	while (chan->ptr != civ) {
		auacer_add_entry(chan);
	}

	WRITE1(sc, chan->port + ALI_OFF_LVI, (chan->ptr - 1) & ALI_LVI_MASK);

	while (chan->ack != civ) {
		if (chan->intr) {
			DPRINTF(ALI_DEBUG_INTR,("auacer_upd_chan: callback\n"));
			chan->intr(chan->arg);
		}
		chan->ack++;
		if (chan->ack >= ALI_DMALIST_MAX)
			chan->ack = 0;
	}
}

static int
auacer_intr(void *v)
{
	struct auacer_softc *sc;
	int ret, intrs;

	sc = v;

	DPRINTF(ALI_DEBUG_INTR, ("auacer_intr: intrs=0x%x\n",
	    READ4(sc, ALI_INTERRUPTSR)));

	mutex_spin_enter(&sc->sc_intr_lock);
	intrs = READ4(sc, ALI_INTERRUPTSR);
	ret = 0;
	if (intrs & ALI_INT_PCMOUT) {
		auacer_upd_chan(sc, &sc->sc_pcmo);
		ret++;
	}
	mutex_spin_exit(&sc->sc_intr_lock);

	return ret != 0;
}

static void
auacer_setup_chan(struct auacer_softc *sc, struct auacer_chan *chan,
		  uint32_t start, uint32_t size, uint32_t blksize,
		  void (*intr)(void *), void *arg)
{
	uint32_t port, slot;
	uint32_t offs, val;

	chan->start = start;
	chan->ptr = 0;
	chan->p = chan->start;
	chan->end = chan->start + size;
	chan->blksize = blksize;
	chan->ack = 0;
	chan->intr = intr;
	chan->arg = arg;

	auacer_add_entry(chan);
	auacer_add_entry(chan);

	port = chan->port;
	slot = ALI_PORT2SLOT(port);

	WRITE1(sc, port + ALI_OFF_CIV, 0);
	WRITE1(sc, port + ALI_OFF_LVI, (chan->ptr - 1) & ALI_LVI_MASK);
	offs = (char *)chan->dmalist - (char *)sc->sc_cdata;
	WRITE4(sc, port + ALI_OFF_BDBAR, sc->sc_cddma + offs);
	WRITE1(sc, port + ALI_OFF_CR,
	       ALI_CR_IOCE | ALI_CR_FEIE | ALI_CR_LVBIE | ALI_CR_RPBM);
	val = READ4(sc, ALI_DMACR);
	val &= ~(1 << (slot+16)); /* no pause */
	val |= 1 << slot;	/* start */
	WRITE4(sc, ALI_DMACR, val);
}

static int
auacer_trigger_output(void *v, void *start, void *end, int blksize,
    void (*intr)(void *), void *arg, const audio_params_t *param)
{
	struct auacer_softc *sc;
	struct auacer_dma *p;
	uint32_t size;

	DPRINTF(ALI_DEBUG_DMA,
		("auacer_trigger_output(%p, %p, %d, %p, %p, %p)\n",
		 start, end, blksize, intr, arg, param));
	sc = v;
	for (p = sc->sc_dmas; p && KERNADDR(p) != start; p = p->next)
		continue;
	if (!p) {
		printf("auacer_trigger_output: bad addr %p\n", start);
		return (EINVAL);
	}

	size = (char *)end - (char *)start;
	auacer_setup_chan(sc, &sc->sc_pcmo, DMAADDR(p), size, blksize,
			  intr, arg);

	return 0;
}

static int
auacer_trigger_input(void *v, void *start, void *end,
    int blksize, void (*intr)(void *), void *arg,
    const audio_params_t *param)
{
	return EINVAL;
}

static int
auacer_allocmem(struct auacer_softc *sc, size_t size, size_t align,
    struct auacer_dma *p)
{
	int error;

	p->size = size;
	error = bus_dmamem_alloc(sc->dmat, p->size, align, 0,
				 p->segs, sizeof(p->segs)/sizeof(p->segs[0]),
				 &p->nsegs, BUS_DMA_WAITOK);
	if (error)
		return error;

	error = bus_dmamem_map(sc->dmat, p->segs, p->nsegs, p->size,
			       &p->addr, BUS_DMA_WAITOK|sc->sc_dmamap_flags);
	if (error)
		goto free;

	error = bus_dmamap_create(sc->dmat, p->size, 1, p->size,
				  0, BUS_DMA_WAITOK, &p->map);
	if (error)
		goto unmap;

	error = bus_dmamap_load(sc->dmat, p->map, p->addr, p->size, NULL,
				BUS_DMA_WAITOK);
	if (error)
		goto destroy;
	return (0);

 destroy:
	bus_dmamap_destroy(sc->dmat, p->map);
 unmap:
	bus_dmamem_unmap(sc->dmat, p->addr, p->size);
 free:
	bus_dmamem_free(sc->dmat, p->segs, p->nsegs);
	return error;
}

static int
auacer_freemem(struct auacer_softc *sc, struct auacer_dma *p)
{

	bus_dmamap_unload(sc->dmat, p->map);
	bus_dmamap_destroy(sc->dmat, p->map);
	bus_dmamem_unmap(sc->dmat, p->addr, p->size);
	bus_dmamem_free(sc->dmat, p->segs, p->nsegs);
	return 0;
}

static int
auacer_alloc_cdata(struct auacer_softc *sc)
{
	bus_dma_segment_t seg;
	int error, rseg;

	/*
	 * Allocate the control data structure, and create and load the
	 * DMA map for it.
	 */
	if ((error = bus_dmamem_alloc(sc->dmat,
				      sizeof(struct auacer_cdata),
				      PAGE_SIZE, 0, &seg, 1, &rseg, 0)) != 0) {
		aprint_error_dev(sc->sc_dev, "unable to allocate control data, error = %d\n",
		    error);
		goto fail_0;
	}

	if ((error = bus_dmamem_map(sc->dmat, &seg, rseg,
				    sizeof(struct auacer_cdata),
				    (void **) &sc->sc_cdata,
				    sc->sc_dmamap_flags)) != 0) {
		aprint_error_dev(sc->sc_dev, "unable to map control data, error = %d\n",
		    error);
		goto fail_1;
	}

	if ((error = bus_dmamap_create(sc->dmat, sizeof(struct auacer_cdata), 1,
				       sizeof(struct auacer_cdata), 0, 0,
				       &sc->sc_cddmamap)) != 0) {
		aprint_error_dev(sc->sc_dev, "unable to create control data DMA map, "
		    "error = %d\n", error);
		goto fail_2;
	}

	if ((error = bus_dmamap_load(sc->dmat, sc->sc_cddmamap,
				     sc->sc_cdata, sizeof(struct auacer_cdata),
				     NULL, 0)) != 0) {
		aprint_error_dev(sc->sc_dev, "unable to load control data DMA map, "
		    "error = %d\n", error);
		goto fail_3;
	}

	return 0;

 fail_3:
	bus_dmamap_destroy(sc->dmat, sc->sc_cddmamap);
 fail_2:
	bus_dmamem_unmap(sc->dmat, (void *) sc->sc_cdata,
	    sizeof(struct auacer_cdata));
 fail_1:
	bus_dmamem_free(sc->dmat, &seg, rseg);
 fail_0:
	return error;
}

static bool
auacer_resume(device_t dv, const pmf_qual_t *qual)
{
	struct auacer_softc *sc = device_private(dv);

	mutex_enter(&sc->sc_lock);
	mutex_spin_enter(&sc->sc_intr_lock);
	auacer_reset_codec(sc);
	mutex_spin_exit(&sc->sc_intr_lock);
	delay(1000);
	sc->codec_if->vtbl->restore_ports(sc->codec_if);
	mutex_exit(&sc->sc_lock);

	return true;
}
