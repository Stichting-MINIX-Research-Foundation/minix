/*	$NetBSD: fms.c,v 1.42 2014/03/29 19:28:24 christos Exp $	*/

/*-
 * Copyright (c) 1999, 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Witold J. Wnuk.
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
 * Forte Media FM801 Audio Device Driver
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: fms.c,v 1.42 2014/03/29 19:28:24 christos Exp $");

#include "mpu.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/kmem.h>
#include <sys/device.h>
#include <sys/audioio.h>

#include <sys/bus.h>
#include <sys/cpu.h>

#include <dev/pci/pcidevs.h>
#include <dev/pci/pcivar.h>

#include <dev/audio_if.h>
#include <dev/mulaw.h>
#include <dev/auconv.h>

#include <dev/ic/ac97var.h>
#include <dev/ic/mpuvar.h>

#include <dev/pci/fmsvar.h>


struct fms_dma {
	struct fms_dma *next;
	void *addr;
	size_t size;
	bus_dmamap_t map;
	bus_dma_segment_t seg;
};



static int	fms_match(device_t, cfdata_t, void *);
static void	fms_attach(device_t, device_t, void *);
static int	fms_intr(void *);

static int	fms_query_encoding(void *, struct audio_encoding *);
static int	fms_set_params(void *, int, int, audio_params_t *,
			       audio_params_t *, stream_filter_list_t *,
			       stream_filter_list_t *);
static int	fms_round_blocksize(void *, int, int, const audio_params_t *);
static int	fms_halt_output(void *);
static int	fms_halt_input(void *);
static int	fms_getdev(void *, struct audio_device *);
static int	fms_set_port(void *, mixer_ctrl_t *);
static int	fms_get_port(void *, mixer_ctrl_t *);
static int	fms_query_devinfo(void *, mixer_devinfo_t *);
static void	*fms_malloc(void *, int, size_t);
static void	fms_free(void *, void *, size_t);
static size_t	fms_round_buffersize(void *, int, size_t);
static paddr_t	fms_mappage(void *, void *, off_t, int);
static int	fms_get_props(void *);
static int	fms_trigger_output(void *, void *, void *, int,
				   void (*)(void *), void *,
				   const audio_params_t *);
static int	fms_trigger_input(void *, void *, void *, int,
				  void (*)(void *), void *,
				  const audio_params_t *);
static void	fms_get_locks(void *, kmutex_t **, kmutex_t **);

CFATTACH_DECL_NEW(fms, sizeof (struct fms_softc),
    fms_match, fms_attach, NULL, NULL);

static struct audio_device fms_device = {
	"Forte Media 801",
	"1.0",
	"fms"
};


static const struct audio_hw_if fms_hw_if = {
	NULL,			/* open */
	NULL,			/* close */
	NULL,
	fms_query_encoding,
	fms_set_params,
	fms_round_blocksize,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	fms_halt_output,
	fms_halt_input,
	NULL,
	fms_getdev,
	NULL,
	fms_set_port,
	fms_get_port,
	fms_query_devinfo,
	fms_malloc,
	fms_free,
	fms_round_buffersize,
	fms_mappage,
	fms_get_props,
	fms_trigger_output,
	fms_trigger_input,
	NULL,
	fms_get_locks,
};

static int	fms_attach_codec(void *, struct ac97_codec_if *);
static int	fms_read_codec(void *, uint8_t, uint16_t *);
static int	fms_write_codec(void *, uint8_t, uint16_t);
static int	fms_reset_codec(void *);

#define FM_PCM_VOLUME		0x00
#define FM_FM_VOLUME		0x02
#define FM_I2S_VOLUME		0x04
#define FM_RECORD_SOURCE	0x06

#define FM_PLAY_CTL		0x08
#define  FM_PLAY_RATE_MASK		0x0f00
#define  FM_PLAY_BUF1_LAST		0x0001
#define  FM_PLAY_BUF2_LAST		0x0002
#define  FM_PLAY_START			0x0020
#define  FM_PLAY_PAUSE			0x0040
#define  FM_PLAY_STOPNOW		0x0080
#define  FM_PLAY_16BIT			0x4000
#define  FM_PLAY_STEREO			0x8000

#define FM_PLAY_DMALEN		0x0a
#define FM_PLAY_DMABUF1		0x0c
#define FM_PLAY_DMABUF2		0x10


#define FM_REC_CTL		0x14
#define  FM_REC_RATE_MASK		0x0f00
#define  FM_REC_BUF1_LAST		0x0001
#define  FM_REC_BUF2_LAST		0x0002
#define  FM_REC_START			0x0020
#define  FM_REC_PAUSE			0x0040
#define  FM_REC_STOPNOW			0x0080
#define  FM_REC_16BIT			0x4000
#define  FM_REC_STEREO			0x8000


#define FM_REC_DMALEN		0x16
#define FM_REC_DMABUF1		0x18
#define FM_REC_DMABUF2		0x1c

#define FM_CODEC_CTL		0x22
#define FM_VOLUME		0x26
#define  FM_VOLUME_MUTE			0x8000

#define FM_CODEC_CMD		0x2a
#define  FM_CODEC_CMD_READ		0x0080
#define  FM_CODEC_CMD_VALID		0x0100
#define  FM_CODEC_CMD_BUSY		0x0200

#define FM_CODEC_DATA		0x2c

#define FM_IO_CTL		0x52
#define FM_CARD_CTL		0x54

#define FM_INTMASK		0x56
#define  FM_INTMASK_PLAY		0x0001
#define  FM_INTMASK_REC			0x0002
#define  FM_INTMASK_VOL			0x0040
#define  FM_INTMASK_MPU			0x0080

#define FM_INTSTATUS		0x5a
#define  FM_INTSTATUS_PLAY		0x0100
#define  FM_INTSTATUS_REC		0x0200
#define  FM_INTSTATUS_VOL		0x4000
#define  FM_INTSTATUS_MPU		0x8000


static int
fms_match(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa;

	pa = (struct pci_attach_args *)aux;
	if (PCI_VENDOR(pa->pa_id) != PCI_VENDOR_FORTEMEDIA)
		return 0;
	if (PCI_PRODUCT(pa->pa_id) != PCI_PRODUCT_FORTEMEDIA_FM801)
		return 0;

	return 1;
}

static void
fms_attach(device_t parent, device_t self, void *aux)
{
	struct pci_attach_args *pa;
	struct fms_softc *sc;
	struct audio_attach_args aa;
	const char *intrstr;
	pci_chipset_tag_t pc;
	pcitag_t pt;
	pci_intr_handle_t ih;
	uint16_t k1;
	char intrbuf[PCI_INTRSTR_LEN];

	pa = aux;
	sc = device_private(self);
	sc->sc_dev = self;
	intrstr = NULL;
	pc = pa->pa_pc;
	pt = pa->pa_tag;
	aprint_naive(": Audio controller\n");
	aprint_normal(": Forte Media FM-801\n");

	if (pci_mapreg_map(pa, 0x10, PCI_MAPREG_TYPE_IO, 0, &sc->sc_iot,
			   &sc->sc_ioh, &sc->sc_ioaddr, &sc->sc_iosize)) {
		aprint_error_dev(sc->sc_dev, "can't map i/o space\n");
		return;
	}
	if (bus_space_subregion(sc->sc_iot, sc->sc_ioh, 0x30, 2,
				&sc->sc_mpu_ioh))
		panic("fms_attach: can't get mpu subregion handle");
	if (bus_space_subregion(sc->sc_iot, sc->sc_ioh, 0x68, 4,
				&sc->sc_opl_ioh))
		panic("fms_attach: can't get opl subregion handle");

	if (pci_intr_map(pa, &ih)) {
		aprint_error_dev(sc->sc_dev, "couldn't map interrupt\n");
		return;
	}
	intrstr = pci_intr_string(pc, ih, intrbuf, sizeof(intrbuf));

	mutex_init(&sc->sc_lock, MUTEX_DEFAULT, IPL_NONE);
	mutex_init(&sc->sc_intr_lock, MUTEX_DEFAULT, IPL_AUDIO);

	sc->sc_ih = pci_intr_establish(pc, ih, IPL_AUDIO, fms_intr, sc);
	if (sc->sc_ih == NULL) {
		aprint_error_dev(sc->sc_dev, "couldn't establish interrupt");
		if (intrstr != NULL)
			aprint_error(" at %s", intrstr);
		aprint_error("\n");
		mutex_destroy(&sc->sc_lock);
		mutex_destroy(&sc->sc_intr_lock);
		return;
	}

	sc->sc_dmat = pa->pa_dmat;

	aprint_normal_dev(sc->sc_dev, "interrupting at %s\n", intrstr);

	/* Disable legacy audio (SBPro compatibility) */
	pci_conf_write(pc, pt, 0x40, 0);

	/* Reset codec and AC'97 */
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, FM_CODEC_CTL, 0x0020);
	delay(2);		/* > 1us according to AC'97 documentation */
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, FM_CODEC_CTL, 0x0000);
	delay(1);		/* > 168.2ns according to AC'97 documentation */

	/* Set up volume */
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, FM_PCM_VOLUME, 0x0808);
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, FM_FM_VOLUME, 0x0808);
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, FM_I2S_VOLUME, 0x0808);

	bus_space_write_2(sc->sc_iot, sc->sc_ioh, FM_RECORD_SOURCE, 0x0000);

	/* Unmask playback, record and mpu interrupts, mask the rest */
	k1 = bus_space_read_2(sc->sc_iot, sc->sc_ioh, FM_INTMASK);
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, FM_INTMASK,
	    (k1 & ~(FM_INTMASK_PLAY | FM_INTMASK_REC | FM_INTMASK_MPU)) |
	     FM_INTMASK_VOL);
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, FM_INTSTATUS,
	    FM_INTSTATUS_PLAY | FM_INTSTATUS_REC | FM_INTSTATUS_MPU |
	    FM_INTSTATUS_VOL);

	sc->host_if.arg = sc;
	sc->host_if.attach = fms_attach_codec;
	sc->host_if.read = fms_read_codec;
	sc->host_if.write = fms_write_codec;
	sc->host_if.reset = fms_reset_codec;

	if (ac97_attach(&sc->host_if, self, &sc->sc_lock) != 0) {
		mutex_destroy(&sc->sc_intr_lock);
		mutex_destroy(&sc->sc_lock);
		return;
	}

	audio_attach_mi(&fms_hw_if, sc, sc->sc_dev);

	aa.type = AUDIODEV_TYPE_OPL;
	aa.hwif = NULL;
	aa.hdl = NULL;
	config_found(sc->sc_dev, &aa, audioprint);

	aa.type = AUDIODEV_TYPE_MPU;
	aa.hwif = NULL;
	aa.hdl = NULL;
	sc->sc_mpu_dev = config_found(sc->sc_dev, &aa, audioprint);
}

/*
 * Each AC-link frame takes 20.8us, data should be ready in next frame,
 * we allow more than two.
 */
#define TIMO 50
static int
fms_read_codec(void *addr, uint8_t reg, uint16_t *val)
{
	struct fms_softc *sc;
	int i;

	sc = addr;
	/* Poll until codec is ready */
	for (i = 0; i < TIMO && bus_space_read_2(sc->sc_iot, sc->sc_ioh,
		 FM_CODEC_CMD) & FM_CODEC_CMD_BUSY; i++)
		delay(1);
	if (i >= TIMO) {
		printf("fms: codec busy\n");
		return 1;
	}

	/* Write register index, read access */
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, FM_CODEC_CMD,
			  reg | FM_CODEC_CMD_READ);

	/* Poll until we have valid data */
	for (i = 0; i < TIMO && !(bus_space_read_2(sc->sc_iot, sc->sc_ioh,
		 FM_CODEC_CMD) & FM_CODEC_CMD_VALID); i++)
		delay(1);
	if (i >= TIMO) {
		printf("fms: no data from codec\n");
		return 1;
	}

	/* Read data */
	*val = bus_space_read_2(sc->sc_iot, sc->sc_ioh, FM_CODEC_DATA);
	return 0;
}

static int
fms_write_codec(void *addr, uint8_t reg, uint16_t val)
{
	struct fms_softc *sc = addr;
	int i;

	/* Poll until codec is ready */
	for (i = 0; i < TIMO && bus_space_read_2(sc->sc_iot, sc->sc_ioh,
		 FM_CODEC_CMD) & FM_CODEC_CMD_BUSY; i++)
		delay(1);
	if (i >= TIMO) {
		printf("fms: codec busy\n");
		return 1;
	}

	/* Write data */
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, FM_CODEC_DATA, val);
	/* Write index register, write access */
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, FM_CODEC_CMD, reg);
	return 0;
}
#undef TIMO

static int
fms_attach_codec(void *addr, struct ac97_codec_if *cif)
{
	struct fms_softc *sc;

	sc = addr;
	sc->codec_if = cif;
	return 0;
}

/* Cold Reset */
static int
fms_reset_codec(void *addr)
{
	struct fms_softc *sc;

	sc = addr;
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, FM_CODEC_CTL, 0x0020);
	delay(2);
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, FM_CODEC_CTL, 0x0000);
	delay(1);
	return 0;
}

static int
fms_intr(void *arg)
{
	struct fms_softc *sc = arg;
#if NMPU > 0
	struct mpu_softc *sc_mpu = device_private(sc->sc_mpu_dev);
#endif
	uint16_t istat;

	mutex_spin_enter(&sc->sc_intr_lock);

	istat = bus_space_read_2(sc->sc_iot, sc->sc_ioh, FM_INTSTATUS);

	if (istat & FM_INTSTATUS_PLAY) {
		if ((sc->sc_play_nextblk += sc->sc_play_blksize) >=
		     sc->sc_play_end)
			sc->sc_play_nextblk = sc->sc_play_start;

		bus_space_write_4(sc->sc_iot, sc->sc_ioh,
		    sc->sc_play_flip++ & 1 ?
		    FM_PLAY_DMABUF2 : FM_PLAY_DMABUF1, sc->sc_play_nextblk);

		if (sc->sc_pintr)
			sc->sc_pintr(sc->sc_parg);
		else
			printf("unexpected play intr\n");
	}

	if (istat & FM_INTSTATUS_REC) {
		if ((sc->sc_rec_nextblk += sc->sc_rec_blksize) >=
		     sc->sc_rec_end)
			sc->sc_rec_nextblk = sc->sc_rec_start;

		bus_space_write_4(sc->sc_iot, sc->sc_ioh,
		    sc->sc_rec_flip++ & 1 ?
		    FM_REC_DMABUF2 : FM_REC_DMABUF1, sc->sc_rec_nextblk);

		if (sc->sc_rintr)
			sc->sc_rintr(sc->sc_rarg);
		else
			printf("unexpected rec intr\n");
	}

#if NMPU > 0
	if (istat & FM_INTSTATUS_MPU)
		mpu_intr(sc_mpu);
#endif

	bus_space_write_2(sc->sc_iot, sc->sc_ioh, FM_INTSTATUS,
			  istat & (FM_INTSTATUS_PLAY | FM_INTSTATUS_REC));

	mutex_spin_exit(&sc->sc_intr_lock);

	return 1;
}

static int
fms_query_encoding(void *addr, struct audio_encoding *fp)
{

	switch (fp->index) {
	case 0:
		strcpy(fp->name, AudioEmulaw);
		fp->encoding = AUDIO_ENCODING_ULAW;
		fp->precision = 8;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		return 0;
	case 1:
		strcpy(fp->name, AudioEslinear_le);
		fp->encoding = AUDIO_ENCODING_SLINEAR_LE;
		fp->precision = 16;
		fp->flags = 0;
		return 0;
	case 2:
		strcpy(fp->name, AudioEulinear);
		fp->encoding = AUDIO_ENCODING_ULINEAR;
		fp->precision = 8;
		fp->flags = 0;
		return 0;
	case 3:
		strcpy(fp->name, AudioEalaw);
		fp->encoding = AUDIO_ENCODING_ALAW;
		fp->precision = 8;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		return 0;
	case 4:
		strcpy(fp->name, AudioEulinear_le);
		fp->encoding = AUDIO_ENCODING_ULINEAR_LE;
		fp->precision = 16;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		return 0;
	case 5:
		strcpy(fp->name, AudioEslinear);
		fp->encoding = AUDIO_ENCODING_SLINEAR;
		fp->precision = 8;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		return 0;
	case 6:
		strcpy(fp->name, AudioEulinear_be);
		fp->encoding = AUDIO_ENCODING_ULINEAR_BE;
		fp->precision = 16;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		return 0;
	case 7:
		strcpy(fp->name, AudioEslinear_be);
		fp->encoding = AUDIO_ENCODING_SLINEAR_BE;
		fp->precision = 16;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		return 0;
	default:
		return EINVAL;
	}
}

/*
 * Range below -limit- is set to -rate-
 * What a pity FM801 does not have 24000
 * 24000 -> 22050 sounds rather poor
 */
static struct {
	int limit;
	int rate;
} const fms_rates[11] = {
	{  6600,  5500 },
	{  8750,  8000 },
	{ 10250,  9600 },
	{ 13200, 11025 },
	{ 17500, 16000 },
	{ 20500, 19200 },
	{ 26500, 22050 },
	{ 35000, 32000 },
	{ 41000, 38400 },
	{ 46000, 44100 },
	{ 48000, 48000 },
	/* anything above -> 48000 */
};

#define FMS_NFORMATS	4
static const struct audio_format fms_formats[FMS_NFORMATS] = {
	{NULL, AUMODE_PLAY | AUMODE_RECORD, AUDIO_ENCODING_SLINEAR_LE, 16, 16,
	 2, AUFMT_STEREO, 11, {5500, 8000, 9600, 11025, 16000, 19200, 22050,
			       32000, 38400, 44100, 48000}},
	{NULL, AUMODE_PLAY | AUMODE_RECORD, AUDIO_ENCODING_SLINEAR_LE, 16, 16,
	 1, AUFMT_MONAURAL, 11, {5500, 8000, 9600, 11025, 16000, 19200, 22050,
				 32000, 38400, 44100, 48000}},
	{NULL, AUMODE_PLAY | AUMODE_RECORD, AUDIO_ENCODING_ULINEAR_LE, 8, 8,
	 2, AUFMT_STEREO, 11, {5500, 8000, 9600, 11025, 16000, 19200, 22050,
			       32000, 38400, 44100, 48000}},
	{NULL, AUMODE_PLAY | AUMODE_RECORD, AUDIO_ENCODING_ULINEAR_LE, 8, 8,
	 1, AUFMT_MONAURAL, 11, {5500, 8000, 9600, 11025, 16000, 19200, 22050,
				 32000, 38400, 44100, 48000}},
};

static int
fms_set_params(void *addr, int setmode, int usemode,
    audio_params_t *play, audio_params_t *rec, stream_filter_list_t *pfil,
    stream_filter_list_t *rfil)
{
	struct fms_softc *sc;
	int i, index;

	sc = addr;
	if (setmode & AUMODE_PLAY) {
		for (i = 0; i < 10 && play->sample_rate > fms_rates[i].limit;
		     i++)
			continue;
		play->sample_rate = fms_rates[i].rate;
		index = auconv_set_converter(fms_formats, FMS_NFORMATS,
					     AUMODE_PLAY, play, FALSE, pfil);
		if (index < 0)
			return EINVAL;
		sc->sc_play_reg = i << 8;
		if (fms_formats[index].channels == 2)
			sc->sc_play_reg |= FM_PLAY_STEREO;
		if (fms_formats[index].precision == 16)
			sc->sc_play_reg |= FM_PLAY_16BIT;
	}

	if (setmode & AUMODE_RECORD) {
		for (i = 0; i < 10 && rec->sample_rate > fms_rates[i].limit;
		     i++)
			continue;
		rec->sample_rate = fms_rates[i].rate;
		index = auconv_set_converter(fms_formats, FMS_NFORMATS,
					     AUMODE_RECORD, rec, FALSE, rfil);
		if (index < 0)
			return EINVAL;
		sc->sc_rec_reg = i << 8;
		if (fms_formats[index].channels == 2)
			sc->sc_rec_reg |= FM_REC_STEREO;
		if (fms_formats[index].precision == 16)
			sc->sc_rec_reg |= FM_REC_16BIT;
	}

	return 0;
}

static int
fms_round_blocksize(void *addr, int blk, int mode,
    const audio_params_t *param)
{

	return blk & ~0xf;
}

static int
fms_halt_output(void *addr)
{
	struct fms_softc *sc;
	uint16_t k1;

	sc = addr;
	k1 = bus_space_read_2(sc->sc_iot, sc->sc_ioh, FM_PLAY_CTL);
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, FM_PLAY_CTL,
			  (k1 & ~(FM_PLAY_STOPNOW | FM_PLAY_START)) |
			  FM_PLAY_BUF1_LAST | FM_PLAY_BUF2_LAST);

	return 0;
}

static int
fms_halt_input(void *addr)
{
	struct fms_softc *sc;
	uint16_t k1;

	sc = addr;
	k1 = bus_space_read_2(sc->sc_iot, sc->sc_ioh, FM_REC_CTL);
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, FM_REC_CTL,
			  (k1 & ~(FM_REC_STOPNOW | FM_REC_START)) |
			  FM_REC_BUF1_LAST | FM_REC_BUF2_LAST);

	return 0;
}

static int
fms_getdev(void *addr, struct audio_device *retp)
{

	*retp = fms_device;
	return 0;
}

static int
fms_set_port(void *addr, mixer_ctrl_t *cp)
{
	struct fms_softc *sc;

	sc = addr;
	return sc->codec_if->vtbl->mixer_set_port(sc->codec_if, cp);
}

static int
fms_get_port(void *addr, mixer_ctrl_t *cp)
{
	struct fms_softc *sc;

	sc = addr;
	return sc->codec_if->vtbl->mixer_get_port(sc->codec_if, cp);
}

static void *
fms_malloc(void *addr, int direction, size_t size)
{
	struct fms_softc *sc;
	struct fms_dma *p;
	int error;
	int rseg;

	sc = addr;
	p = kmem_alloc(sizeof(*p), KM_SLEEP);
	if (p == NULL)
		return NULL;

	p->size = size;
	if ((error = bus_dmamem_alloc(sc->sc_dmat, size, PAGE_SIZE, 0, &p->seg,
				      1, &rseg, BUS_DMA_WAITOK)) != 0) {
		aprint_error_dev(sc->sc_dev, "unable to allocate DMA, error = %d\n", error);
		goto fail_alloc;
	}

	if ((error = bus_dmamem_map(sc->sc_dmat, &p->seg, rseg, size, &p->addr,
				    BUS_DMA_WAITOK | BUS_DMA_COHERENT)) != 0) {
		aprint_error_dev(sc->sc_dev, "unable to map DMA, error = %d\n",
		       error);
		goto fail_map;
	}

	if ((error = bus_dmamap_create(sc->sc_dmat, size, 1, size, 0,
				       BUS_DMA_WAITOK, &p->map)) != 0) {
		aprint_error_dev(sc->sc_dev, "unable to create DMA map, error = %d\n",
		       error);
		goto fail_create;
	}

	if ((error = bus_dmamap_load(sc->sc_dmat, p->map, p->addr, size, NULL,
				     BUS_DMA_WAITOK)) != 0) {
		aprint_error_dev(sc->sc_dev, "unable to load DMA map, error = %d\n",
		       error);
		goto fail_load;
	}

	p->next = sc->sc_dmas;
	sc->sc_dmas = p;

	return p->addr;


fail_load:
	bus_dmamap_destroy(sc->sc_dmat, p->map);
fail_create:
	bus_dmamem_unmap(sc->sc_dmat, p->addr, size);
fail_map:
	bus_dmamem_free(sc->sc_dmat, &p->seg, 1);
fail_alloc:
	kmem_free(p, sizeof(*p));
	return NULL;
}

static void
fms_free(void *addr, void *ptr, size_t size)
{
	struct fms_softc *sc;
	struct fms_dma **pp, *p;

	sc = addr;
	for (pp = &(sc->sc_dmas); (p = *pp) != NULL; pp = &p->next)
		if (p->addr == ptr) {
			bus_dmamap_unload(sc->sc_dmat, p->map);
			bus_dmamap_destroy(sc->sc_dmat, p->map);
			bus_dmamem_unmap(sc->sc_dmat, p->addr, p->size);
			bus_dmamem_free(sc->sc_dmat, &p->seg, 1);

			*pp = p->next;
			kmem_free(p, sizeof(*p));
			return;
		}

	panic("fms_free: trying to free unallocated memory");
}

static size_t
fms_round_buffersize(void *addr, int direction, size_t size)
{

	return size;
}

static paddr_t
fms_mappage(void *addr, void *mem, off_t off, int prot)
{
	struct fms_softc *sc;
	struct fms_dma *p;

	sc = addr;
	if (off < 0)
		return -1;

	for (p = sc->sc_dmas; p && p->addr != mem; p = p->next)
		continue;
	if (p == NULL)
		return -1;

	return bus_dmamem_mmap(sc->sc_dmat, &p->seg, 1, off, prot,
			       BUS_DMA_WAITOK);
}

static int
fms_get_props(void *addr)
{
	return AUDIO_PROP_MMAP | AUDIO_PROP_INDEPENDENT |
	       AUDIO_PROP_FULLDUPLEX;
}

static int
fms_query_devinfo(void *addr, mixer_devinfo_t *dip)
{
	struct fms_softc *sc;

	sc = addr;
	return sc->codec_if->vtbl->query_devinfo(sc->codec_if, dip);
}

static int
fms_trigger_output(void *addr, void *start, void *end, int blksize,
    void (*intr)(void *), void *arg, const audio_params_t *param)
{
	struct fms_softc *sc;
	struct fms_dma *p;

	sc = addr;
	sc->sc_pintr = intr;
	sc->sc_parg = arg;

	for (p = sc->sc_dmas; p && p->addr != start; p = p->next)
		continue;

	if (p == NULL)
		panic("fms_trigger_output: request with bad start "
		      "address (%p)", start);

	sc->sc_play_start = p->map->dm_segs[0].ds_addr;
	sc->sc_play_end = sc->sc_play_start + ((char *)end - (char *)start);
	sc->sc_play_blksize = blksize;
	sc->sc_play_nextblk = sc->sc_play_start + sc->sc_play_blksize;
	sc->sc_play_flip = 0;
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, FM_PLAY_DMALEN, blksize - 1);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, FM_PLAY_DMABUF1,
			  sc->sc_play_start);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, FM_PLAY_DMABUF2,
			  sc->sc_play_nextblk);
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, FM_PLAY_CTL,
			  FM_PLAY_START | FM_PLAY_STOPNOW | sc->sc_play_reg);
	return 0;
}


static int
fms_trigger_input(void *addr, void *start, void *end, int blksize,
    void (*intr)(void *), void *arg, const audio_params_t *param)
{
	struct fms_softc *sc;
	struct fms_dma *p;

	sc = addr;
	sc->sc_rintr = intr;
	sc->sc_rarg = arg;

	for (p = sc->sc_dmas; p && p->addr != start; p = p->next)
		continue;

	if (p == NULL)
		panic("fms_trigger_input: request with bad start "
		      "address (%p)", start);

	sc->sc_rec_start = p->map->dm_segs[0].ds_addr;
	sc->sc_rec_end = sc->sc_rec_start + ((char *)end - (char *)start);
	sc->sc_rec_blksize = blksize;
	sc->sc_rec_nextblk = sc->sc_rec_start + sc->sc_rec_blksize;
	sc->sc_rec_flip = 0;
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, FM_REC_DMALEN, blksize - 1);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, FM_REC_DMABUF1,
			  sc->sc_rec_start);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, FM_REC_DMABUF2,
			  sc->sc_rec_nextblk);
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, FM_REC_CTL,
			  FM_REC_START | FM_REC_STOPNOW | sc->sc_rec_reg);
	return 0;
}

static void
fms_get_locks(void *addr, kmutex_t **intr, kmutex_t **thread)
{
	struct fms_softc *sc;

	sc = addr;
	*intr = &sc->sc_intr_lock;
	*thread = &sc->sc_lock;
}
