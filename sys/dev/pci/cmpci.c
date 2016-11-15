/*	$NetBSD: cmpci.c,v 1.47 2014/03/29 19:28:24 christos Exp $	*/

/*
 * Copyright (c) 2000, 2001, 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Takuya SHIOZAKI <tshiozak@NetBSD.org> .
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by ITOH Yasufumi.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/*
 * C-Media CMI8x38 Audio Chip Support.
 *
 * TODO:
 *   - 4ch / 6ch support.
 *   - Joystick support.
 *
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: cmpci.c,v 1.47 2014/03/29 19:28:24 christos Exp $");

#if defined(AUDIO_DEBUG) || defined(DEBUG)
#define DPRINTF(x) if (cmpcidebug) printf x
int cmpcidebug = 0;
#else
#define DPRINTF(x)
#endif

#include "mpu.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/kmem.h>
#include <sys/device.h>
#include <sys/proc.h>

#include <dev/pci/pcidevs.h>
#include <dev/pci/pcivar.h>

#include <sys/audioio.h>
#include <dev/audio_if.h>
#include <dev/midi_if.h>

#include <dev/mulaw.h>
#include <dev/auconv.h>
#include <dev/pci/cmpcireg.h>
#include <dev/pci/cmpcivar.h>

#include <dev/ic/mpuvar.h>
#include <sys/bus.h>
#include <sys/intr.h>

/*
 * Low-level HW interface
 */
static inline uint8_t cmpci_mixerreg_read(struct cmpci_softc *, uint8_t);
static inline void cmpci_mixerreg_write(struct cmpci_softc *,
	uint8_t, uint8_t);
static inline void cmpci_reg_partial_write_1(struct cmpci_softc *, int, int,
	unsigned, unsigned);
static inline void cmpci_reg_partial_write_4(struct cmpci_softc *, int, int,
	uint32_t, uint32_t);
static inline void cmpci_reg_set_1(struct cmpci_softc *, int, uint8_t);
static inline void cmpci_reg_clear_1(struct cmpci_softc *, int, uint8_t);
static inline void cmpci_reg_set_4(struct cmpci_softc *, int, uint32_t);
static inline void cmpci_reg_clear_4(struct cmpci_softc *, int, uint32_t);
static inline void cmpci_reg_set_reg_misc(struct cmpci_softc *, uint32_t);
static inline void cmpci_reg_clear_reg_misc(struct cmpci_softc *, uint32_t);
static int cmpci_rate_to_index(int);
static inline int cmpci_index_to_rate(int);
static inline int cmpci_index_to_divider(int);

static int cmpci_adjust(int, int);
static void cmpci_set_mixer_gain(struct cmpci_softc *, int);
static void cmpci_set_out_ports(struct cmpci_softc *);
static int cmpci_set_in_ports(struct cmpci_softc *);


/*
 * autoconf interface
 */
static int cmpci_match(device_t, cfdata_t, void *);
static void cmpci_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(cmpci, sizeof (struct cmpci_softc),
    cmpci_match, cmpci_attach, NULL, NULL);

/* interrupt */
static int cmpci_intr(void *);


/*
 * DMA stuffs
 */
static int cmpci_alloc_dmamem(struct cmpci_softc *, size_t, void **);
static int cmpci_free_dmamem(struct cmpci_softc *, void *, size_t);
static struct cmpci_dmanode * cmpci_find_dmamem(struct cmpci_softc *,
	void *);


/*
 * interface to machine independent layer
 */
static int cmpci_query_encoding(void *, struct audio_encoding *);
static int cmpci_set_params(void *, int, int, audio_params_t *,
	audio_params_t *, stream_filter_list_t *, stream_filter_list_t *);
static int cmpci_round_blocksize(void *, int, int, const audio_params_t *);
static int cmpci_halt_output(void *);
static int cmpci_halt_input(void *);
static int cmpci_getdev(void *, struct audio_device *);
static int cmpci_set_port(void *, mixer_ctrl_t *);
static int cmpci_get_port(void *, mixer_ctrl_t *);
static int cmpci_query_devinfo(void *, mixer_devinfo_t *);
static void *cmpci_allocm(void *, int, size_t);
static void cmpci_freem(void *, void *, size_t);
static size_t cmpci_round_buffersize(void *, int, size_t);
static paddr_t cmpci_mappage(void *, void *, off_t, int);
static int cmpci_get_props(void *);
static int cmpci_trigger_output(void *, void *, void *, int,
	void (*)(void *), void *, const audio_params_t *);
static int cmpci_trigger_input(void *, void *, void *, int,
	void (*)(void *), void *, const audio_params_t *);
static void cmpci_get_locks(void *, kmutex_t **, kmutex_t **);

static const struct audio_hw_if cmpci_hw_if = {
	NULL,			/* open */
	NULL,			/* close */
	NULL,			/* drain */
	cmpci_query_encoding,	/* query_encoding */
	cmpci_set_params,	/* set_params */
	cmpci_round_blocksize,	/* round_blocksize */
	NULL,			/* commit_settings */
	NULL,			/* init_output */
	NULL,			/* init_input */
	NULL,			/* start_output */
	NULL,			/* start_input */
	cmpci_halt_output,	/* halt_output */
	cmpci_halt_input,	/* halt_input */
	NULL,			/* speaker_ctl */
	cmpci_getdev,		/* getdev */
	NULL,			/* setfd */
	cmpci_set_port,		/* set_port */
	cmpci_get_port,		/* get_port */
	cmpci_query_devinfo,	/* query_devinfo */
	cmpci_allocm,		/* allocm */
	cmpci_freem,		/* freem */
	cmpci_round_buffersize,/* round_buffersize */
	cmpci_mappage,		/* mappage */
	cmpci_get_props,	/* get_props */
	cmpci_trigger_output,	/* trigger_output */
	cmpci_trigger_input,	/* trigger_input */
	NULL,			/* dev_ioctl */
	cmpci_get_locks,	/* get_locks */
};

#define CMPCI_NFORMATS	4
static const struct audio_format cmpci_formats[CMPCI_NFORMATS] = {
	{NULL, AUMODE_PLAY | AUMODE_RECORD, AUDIO_ENCODING_SLINEAR_LE, 16, 16,
	 2, AUFMT_STEREO, 0, {5512, 48000}},
	{NULL, AUMODE_PLAY | AUMODE_RECORD, AUDIO_ENCODING_SLINEAR_LE, 16, 16,
	 1, AUFMT_MONAURAL, 0, {5512, 48000}},
	{NULL, AUMODE_PLAY | AUMODE_RECORD, AUDIO_ENCODING_ULINEAR_LE, 8, 8,
	 2, AUFMT_STEREO, 0, {5512, 48000}},
	{NULL, AUMODE_PLAY | AUMODE_RECORD, AUDIO_ENCODING_ULINEAR_LE, 8, 8,
	 1, AUFMT_MONAURAL, 0, {5512, 48000}},
};


/*
 * Low-level HW interface
 */

/* mixer register read/write */
static inline uint8_t
cmpci_mixerreg_read(struct cmpci_softc *sc, uint8_t no)
{
	uint8_t ret;

	bus_space_write_1(sc->sc_iot, sc->sc_ioh, CMPCI_REG_SBADDR, no);
	delay(10);
	ret = bus_space_read_1(sc->sc_iot, sc->sc_ioh, CMPCI_REG_SBDATA);
	delay(10);
	return ret;
}

static inline void
cmpci_mixerreg_write(struct cmpci_softc *sc, uint8_t no, uint8_t val)
{

	bus_space_write_1(sc->sc_iot, sc->sc_ioh, CMPCI_REG_SBADDR, no);
	delay(10);
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, CMPCI_REG_SBDATA, val);
	delay(10);
}


/* register partial write */
static inline void
cmpci_reg_partial_write_1(struct cmpci_softc *sc, int no, int shift,
			  unsigned mask, unsigned val)
{

	bus_space_write_1(sc->sc_iot, sc->sc_ioh, no,
	    (val<<shift) |
	    (bus_space_read_1(sc->sc_iot, sc->sc_ioh, no) & ~(mask<<shift)));
	delay(10);
}

static inline void
cmpci_reg_partial_write_4(struct cmpci_softc *sc, int no, int shift,
			  uint32_t mask, uint32_t val)
{

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, no,
	    (val<<shift) |
	    (bus_space_read_4(sc->sc_iot, sc->sc_ioh, no) & ~(mask<<shift)));
	delay(10);
}

/* register set/clear bit */
static inline void
cmpci_reg_set_1(struct cmpci_softc *sc, int no, uint8_t mask)
{

	bus_space_write_1(sc->sc_iot, sc->sc_ioh, no,
	    (bus_space_read_1(sc->sc_iot, sc->sc_ioh, no) | mask));
	delay(10);
}

static inline void
cmpci_reg_clear_1(struct cmpci_softc *sc, int no, uint8_t mask)
{

	bus_space_write_1(sc->sc_iot, sc->sc_ioh, no,
	    (bus_space_read_1(sc->sc_iot, sc->sc_ioh, no) & ~mask));
	delay(10);
}

static inline void
cmpci_reg_set_4(struct cmpci_softc *sc, int no, uint32_t mask)
{

	/* use cmpci_reg_set_reg_misc() for CMPCI_REG_MISC */
	KDASSERT(no != CMPCI_REG_MISC);

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, no,
	    (bus_space_read_4(sc->sc_iot, sc->sc_ioh, no) | mask));
	delay(10);
}

static inline void
cmpci_reg_clear_4(struct cmpci_softc *sc, int no, uint32_t mask)
{

	/* use cmpci_reg_clear_reg_misc() for CMPCI_REG_MISC */
	KDASSERT(no != CMPCI_REG_MISC);

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, no,
	    (bus_space_read_4(sc->sc_iot, sc->sc_ioh, no) & ~mask));
	delay(10);
}

/*
 * The CMPCI_REG_MISC register needs special handling, since one of
 * its bits has different read/write values.
 */
static inline void
cmpci_reg_set_reg_misc(struct cmpci_softc *sc, uint32_t mask)
{

	sc->sc_reg_misc |= mask;
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, CMPCI_REG_MISC,
	    sc->sc_reg_misc);
	delay(10);
}

static inline void
cmpci_reg_clear_reg_misc(struct cmpci_softc *sc, uint32_t mask)
{

	sc->sc_reg_misc &= ~mask;
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, CMPCI_REG_MISC,
	    sc->sc_reg_misc);
	delay(10);
}

/* rate */
static const struct {
	int rate;
	int divider;
} cmpci_rate_table[CMPCI_REG_NUMRATE] = {
#define _RATE(n) { n, CMPCI_REG_RATE_ ## n }
	_RATE(5512),
	_RATE(8000),
	_RATE(11025),
	_RATE(16000),
	_RATE(22050),
	_RATE(32000),
	_RATE(44100),
	_RATE(48000)
#undef	_RATE
};

static int
cmpci_rate_to_index(int rate)
{
	int i;

	for (i = 0; i < CMPCI_REG_NUMRATE - 1; i++)
		if (rate <=
		    (cmpci_rate_table[i].rate+cmpci_rate_table[i+1].rate) / 2)
			return i;
	return i;  /* 48000 */
}

static inline int
cmpci_index_to_rate(int index)
{

	return cmpci_rate_table[index].rate;
}

static inline int
cmpci_index_to_divider(int index)
{

	return cmpci_rate_table[index].divider;
}

/*
 * interface to configure the device.
 */
static int
cmpci_match(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa;

	pa = (struct pci_attach_args *)aux;
	if ( PCI_VENDOR(pa->pa_id) == PCI_VENDOR_CMEDIA &&
	     (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_CMEDIA_CMI8338A ||
	      PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_CMEDIA_CMI8338B ||
	      PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_CMEDIA_CMI8738 ||
	      PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_CMEDIA_CMI8738B) )
		return 1;

	return 0;
}

static void
cmpci_attach(device_t parent, device_t self, void *aux)
{
	struct cmpci_softc *sc;
	struct pci_attach_args *pa;
	struct audio_attach_args aa;
	pci_intr_handle_t ih;
	char const *strintr;
	int i, v;
	char intrbuf[PCI_INTRSTR_LEN];

	sc = device_private(self);
	sc->sc_dev = self;
	pa = (struct pci_attach_args *)aux;

	sc->sc_id = pa->pa_id;
	sc->sc_class = pa->pa_class;
	pci_aprint_devinfo(pa, "Audio controller");
	switch (PCI_PRODUCT(sc->sc_id)) {
	case PCI_PRODUCT_CMEDIA_CMI8338A:
		/*FALLTHROUGH*/
	case PCI_PRODUCT_CMEDIA_CMI8338B:
		sc->sc_capable = CMPCI_CAP_CMI8338;
		break;
	case PCI_PRODUCT_CMEDIA_CMI8738:
		/*FALLTHROUGH*/
	case PCI_PRODUCT_CMEDIA_CMI8738B:
		sc->sc_capable = CMPCI_CAP_CMI8738;
		break;
	}

	/* map I/O space */
	if (pci_mapreg_map(pa, CMPCI_PCI_IOBASEREG, PCI_MAPREG_TYPE_IO, 0,
		&sc->sc_iot, &sc->sc_ioh, NULL, NULL)) {
		aprint_error_dev(sc->sc_dev, "failed to map I/O space\n");
		return;
	}

	mutex_init(&sc->sc_lock, MUTEX_DEFAULT, IPL_NONE);
	mutex_init(&sc->sc_intr_lock, MUTEX_DEFAULT, IPL_AUDIO);

	/* interrupt */
	if (pci_intr_map(pa, &ih)) {
		aprint_error_dev(sc->sc_dev, "failed to map interrupt\n");
		return;
	}
	strintr = pci_intr_string(pa->pa_pc, ih, intrbuf, sizeof(intrbuf));
	sc->sc_ih = pci_intr_establish(pa->pa_pc, ih, IPL_AUDIO, cmpci_intr,
	    sc);
	if (sc->sc_ih == NULL) {
		aprint_error_dev(sc->sc_dev, "failed to establish interrupt");
		if (strintr != NULL)
			aprint_error(" at %s", strintr);
		aprint_error("\n");
		mutex_destroy(&sc->sc_lock);
		mutex_destroy(&sc->sc_intr_lock);
		return;
	}
	aprint_normal_dev(sc->sc_dev, "interrupting at %s\n", strintr);

	sc->sc_dmat = pa->pa_dmat;

	audio_attach_mi(&cmpci_hw_if, sc, sc->sc_dev);

	/* attach OPL device */
	aa.type = AUDIODEV_TYPE_OPL;
	aa.hwif = NULL;
	aa.hdl = NULL;
	(void)config_found(sc->sc_dev, &aa, audioprint);

	/* attach MPU-401 device */
	aa.type = AUDIODEV_TYPE_MPU;
	aa.hwif = NULL;
	aa.hdl = NULL;
	if (bus_space_subregion(sc->sc_iot, sc->sc_ioh,
	    CMPCI_REG_MPU_BASE, CMPCI_REG_MPU_SIZE, &sc->sc_mpu_ioh) == 0)
		sc->sc_mpudev = config_found(sc->sc_dev, &aa, audioprint);

	/* get initial value (this is 0 and may be omitted but just in case) */
	sc->sc_reg_misc = bus_space_read_4(sc->sc_iot, sc->sc_ioh,
	    CMPCI_REG_MISC) & ~CMPCI_REG_SPDIF48K;

	cmpci_mixerreg_write(sc, CMPCI_SB16_MIXER_RESET, 0);
	cmpci_mixerreg_write(sc, CMPCI_SB16_MIXER_ADCMIX_L, 0);
	cmpci_mixerreg_write(sc, CMPCI_SB16_MIXER_ADCMIX_R, 0);
	cmpci_mixerreg_write(sc, CMPCI_SB16_MIXER_OUTMIX,
	    CMPCI_SB16_SW_CD|CMPCI_SB16_SW_MIC | CMPCI_SB16_SW_LINE);
	for (i = 0; i < CMPCI_NDEVS; i++) {
		switch(i) {
		/*
		 * CMI8738 defaults are
		 *  master:	0xe0	(0x00 - 0xf8)
		 *  FM, DAC:	0xc0	(0x00 - 0xf8)
		 *  PC speaker:	0x80	(0x00 - 0xc0)
		 *  others:	0
		 */
		/* volume */
		case CMPCI_MASTER_VOL:
			v = 128;	/* 224 */
			break;
		case CMPCI_FM_VOL:
		case CMPCI_DAC_VOL:
			v = 192;
			break;
		case CMPCI_PCSPEAKER:
			v = 128;
			break;

		/* booleans, set to true */
		case CMPCI_CD_MUTE:
		case CMPCI_MIC_MUTE:
		case CMPCI_LINE_IN_MUTE:
		case CMPCI_AUX_IN_MUTE:
			v = 1;
			break;

		/* volume with inital value 0 */
		case CMPCI_CD_VOL:
		case CMPCI_LINE_IN_VOL:
		case CMPCI_AUX_IN_VOL:
		case CMPCI_MIC_VOL:
		case CMPCI_MIC_RECVOL:
			/* FALLTHROUGH */

		/* others are cleared */
		case CMPCI_MIC_PREAMP:
		case CMPCI_RECORD_SOURCE:
		case CMPCI_PLAYBACK_MODE:
		case CMPCI_SPDIF_IN_SELECT:
		case CMPCI_SPDIF_IN_PHASE:
		case CMPCI_SPDIF_LOOP:
		case CMPCI_SPDIF_OUT_PLAYBACK:
		case CMPCI_SPDIF_OUT_VOLTAGE:
		case CMPCI_MONITOR_DAC:
		case CMPCI_REAR:
		case CMPCI_INDIVIDUAL:
		case CMPCI_REVERSE:
		case CMPCI_SURROUND:
		default:
			v = 0;
			break;
		}
		sc->sc_gain[i][CMPCI_LEFT] = sc->sc_gain[i][CMPCI_RIGHT] = v;
		cmpci_set_mixer_gain(sc, i);
	}
}

static int
cmpci_intr(void *handle)
{
	struct cmpci_softc *sc = handle;
#if NMPU > 0
	struct mpu_softc *sc_mpu = device_private(sc->sc_mpudev);
#endif
	uint32_t intrstat;

	mutex_spin_enter(&sc->sc_intr_lock);

	intrstat = bus_space_read_4(sc->sc_iot, sc->sc_ioh,
	    CMPCI_REG_INTR_STATUS);

	if (!(intrstat & CMPCI_REG_ANY_INTR)) {
		mutex_spin_exit(&sc->sc_intr_lock);
		return 0;
	}

	delay(10);

	/* disable and reset intr */
	if (intrstat & CMPCI_REG_CH0_INTR)
		cmpci_reg_clear_4(sc, CMPCI_REG_INTR_CTRL,
		   CMPCI_REG_CH0_INTR_ENABLE);
	if (intrstat & CMPCI_REG_CH1_INTR)
		cmpci_reg_clear_4(sc, CMPCI_REG_INTR_CTRL,
		    CMPCI_REG_CH1_INTR_ENABLE);

	if (intrstat & CMPCI_REG_CH0_INTR) {
		if (sc->sc_play.intr != NULL)
			(*sc->sc_play.intr)(sc->sc_play.intr_arg);
	}
	if (intrstat & CMPCI_REG_CH1_INTR) {
		if (sc->sc_rec.intr != NULL)
			(*sc->sc_rec.intr)(sc->sc_rec.intr_arg);
	}

	/* enable intr */
	if (intrstat & CMPCI_REG_CH0_INTR)
		cmpci_reg_set_4(sc, CMPCI_REG_INTR_CTRL,
		    CMPCI_REG_CH0_INTR_ENABLE);
	if (intrstat & CMPCI_REG_CH1_INTR)
		cmpci_reg_set_4(sc, CMPCI_REG_INTR_CTRL,
		    CMPCI_REG_CH1_INTR_ENABLE);

#if NMPU > 0
	if (intrstat & CMPCI_REG_UART_INTR && sc_mpu != NULL)
		mpu_intr(sc_mpu);
#endif

	mutex_spin_exit(&sc->sc_intr_lock);
	return 1;
}

static int
cmpci_query_encoding(void *handle, struct audio_encoding *fp)
{

	switch (fp->index) {
	case 0:
		strcpy(fp->name, AudioEulinear);
		fp->encoding = AUDIO_ENCODING_ULINEAR;
		fp->precision = 8;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		break;
	case 1:
		strcpy(fp->name, AudioEmulaw);
		fp->encoding = AUDIO_ENCODING_ULAW;
		fp->precision = 8;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		break;
	case 2:
		strcpy(fp->name, AudioEalaw);
		fp->encoding = AUDIO_ENCODING_ALAW;
		fp->precision = 8;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		break;
	case 3:
		strcpy(fp->name, AudioEslinear);
		fp->encoding = AUDIO_ENCODING_SLINEAR;
		fp->precision = 8;
		fp->flags = 0;
		break;
	case 4:
		strcpy(fp->name, AudioEslinear_le);
		fp->encoding = AUDIO_ENCODING_SLINEAR_LE;
		fp->precision = 16;
		fp->flags = 0;
		break;
	case 5:
		strcpy(fp->name, AudioEulinear_le);
		fp->encoding = AUDIO_ENCODING_ULINEAR_LE;
		fp->precision = 16;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		break;
	case 6:
		strcpy(fp->name, AudioEslinear_be);
		fp->encoding = AUDIO_ENCODING_SLINEAR_BE;
		fp->precision = 16;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		break;
	case 7:
		strcpy(fp->name, AudioEulinear_be);
		fp->encoding = AUDIO_ENCODING_ULINEAR_BE;
		fp->precision = 16;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		break;
	default:
		return EINVAL;
	}
	return 0;
}


static int
cmpci_set_params(void *handle, int setmode, int usemode,
    audio_params_t *play, audio_params_t *rec, stream_filter_list_t *pfil,
    stream_filter_list_t *rfil)
{
	int i;
	struct cmpci_softc *sc;

	sc = handle;
	for (i = 0; i < 2; i++) {
		int md_format;
		int md_divide;
		int md_index;
		int mode;
		audio_params_t *p;
		stream_filter_list_t *fil;
		int ind;

		switch (i) {
		case 0:
			mode = AUMODE_PLAY;
			p = play;
			fil = pfil;
			break;
		case 1:
			mode = AUMODE_RECORD;
			p = rec;
			fil = rfil;
			break;
		default:
			return EINVAL;
		}

		if (!(setmode & mode))
			continue;

		md_index = cmpci_rate_to_index(p->sample_rate);
		md_divide = cmpci_index_to_divider(md_index);
		p->sample_rate = cmpci_index_to_rate(md_index);
		DPRINTF(("%s: sample:%u, divider=%d\n",
			 device_xname(sc->sc_dev), p->sample_rate, md_divide));

		ind = auconv_set_converter(cmpci_formats, CMPCI_NFORMATS,
					   mode, p, FALSE, fil);
		if (ind < 0)
			return EINVAL;
		if (fil->req_size > 0)
			p = &fil->filters[0].param;

		/* format */
		md_format = p->channels == 1
			? CMPCI_REG_FORMAT_MONO : CMPCI_REG_FORMAT_STEREO;
		md_format |= p->precision == 16
			? CMPCI_REG_FORMAT_16BIT : CMPCI_REG_FORMAT_8BIT;
		if (mode & AUMODE_PLAY) {
			cmpci_reg_partial_write_4(sc,
			   CMPCI_REG_CHANNEL_FORMAT,
			   CMPCI_REG_CH0_FORMAT_SHIFT,
			   CMPCI_REG_CH0_FORMAT_MASK, md_format);
			cmpci_reg_partial_write_4(sc,
			    CMPCI_REG_FUNC_1, CMPCI_REG_DAC_FS_SHIFT,
			    CMPCI_REG_DAC_FS_MASK, md_divide);
			sc->sc_play.md_divide = md_divide;
		} else {
			cmpci_reg_partial_write_4(sc,
			   CMPCI_REG_CHANNEL_FORMAT,
			   CMPCI_REG_CH1_FORMAT_SHIFT,
			   CMPCI_REG_CH1_FORMAT_MASK, md_format);
			cmpci_reg_partial_write_4(sc,
			    CMPCI_REG_FUNC_1, CMPCI_REG_ADC_FS_SHIFT,
			    CMPCI_REG_ADC_FS_MASK, md_divide);
			sc->sc_rec.md_divide = md_divide;
		}
		cmpci_set_out_ports(sc);
		cmpci_set_in_ports(sc);
	}
	return 0;
}

/* ARGSUSED */
static int
cmpci_round_blocksize(void *handle, int block,
    int mode, const audio_params_t *param)
{

	return block & -4;
}

static int
cmpci_halt_output(void *handle)
{
	struct cmpci_softc *sc;

	sc = handle;
	sc->sc_play.intr = NULL;
	cmpci_reg_clear_4(sc, CMPCI_REG_INTR_CTRL, CMPCI_REG_CH0_INTR_ENABLE);
	cmpci_reg_clear_4(sc, CMPCI_REG_FUNC_0, CMPCI_REG_CH0_ENABLE);
	/* wait for reset DMA */
	cmpci_reg_set_4(sc, CMPCI_REG_FUNC_0, CMPCI_REG_CH0_RESET);
	delay(10);
	cmpci_reg_clear_4(sc, CMPCI_REG_FUNC_0, CMPCI_REG_CH0_RESET);

	return 0;
}

static int
cmpci_halt_input(void *handle)
{
	struct cmpci_softc *sc;

	sc = handle;
	sc->sc_rec.intr = NULL;
	cmpci_reg_clear_4(sc, CMPCI_REG_INTR_CTRL, CMPCI_REG_CH1_INTR_ENABLE);
	cmpci_reg_clear_4(sc, CMPCI_REG_FUNC_0, CMPCI_REG_CH1_ENABLE);
	/* wait for reset DMA */
	cmpci_reg_set_4(sc, CMPCI_REG_FUNC_0, CMPCI_REG_CH1_RESET);
	delay(10);
	cmpci_reg_clear_4(sc, CMPCI_REG_FUNC_0, CMPCI_REG_CH1_RESET);

	return 0;
}

/* get audio device information */
static int
cmpci_getdev(void *handle, struct audio_device *ad)
{
	struct cmpci_softc *sc;

	sc = handle;
	strncpy(ad->name, "CMI PCI Audio", sizeof(ad->name));
	snprintf(ad->version, sizeof(ad->version), "0x%02x",
		 PCI_REVISION(sc->sc_class));
	switch (PCI_PRODUCT(sc->sc_id)) {
	case PCI_PRODUCT_CMEDIA_CMI8338A:
		strncpy(ad->config, "CMI8338A", sizeof(ad->config));
		break;
	case PCI_PRODUCT_CMEDIA_CMI8338B:
		strncpy(ad->config, "CMI8338B", sizeof(ad->config));
		break;
	case PCI_PRODUCT_CMEDIA_CMI8738:
		strncpy(ad->config, "CMI8738", sizeof(ad->config));
		break;
	case PCI_PRODUCT_CMEDIA_CMI8738B:
		strncpy(ad->config, "CMI8738B", sizeof(ad->config));
		break;
	default:
		strncpy(ad->config, "unknown", sizeof(ad->config));
	}

	return 0;
}

/* mixer device information */
int
cmpci_query_devinfo(void *handle, mixer_devinfo_t *dip)
{
	static const char *const mixer_port_names[] = {
		AudioNdac, AudioNfmsynth, AudioNcd, AudioNline, AudioNaux,
		AudioNmicrophone
	};
	static const char *const mixer_classes[] = {
		AudioCinputs, AudioCoutputs, AudioCrecord, CmpciCplayback,
		CmpciCspdif
	};
	struct cmpci_softc *sc;
	int i;

	sc = handle;
	dip->prev = dip->next = AUDIO_MIXER_LAST;

	switch (dip->index) {
	case CMPCI_INPUT_CLASS:
	case CMPCI_OUTPUT_CLASS:
	case CMPCI_RECORD_CLASS:
	case CMPCI_PLAYBACK_CLASS:
	case CMPCI_SPDIF_CLASS:
		dip->type = AUDIO_MIXER_CLASS;
		dip->mixer_class = dip->index;
		strcpy(dip->label.name,
		    mixer_classes[dip->index - CMPCI_INPUT_CLASS]);
		return 0;

	case CMPCI_AUX_IN_VOL:
		dip->un.v.delta = 1 << (8 - CMPCI_REG_AUX_VALBITS);
		goto vol1;
	case CMPCI_DAC_VOL:
	case CMPCI_FM_VOL:
	case CMPCI_CD_VOL:
	case CMPCI_LINE_IN_VOL:
	case CMPCI_MIC_VOL:
		dip->un.v.delta = 1 << (8 - CMPCI_SB16_MIXER_VALBITS);
	vol1:	dip->mixer_class = CMPCI_INPUT_CLASS;
		dip->next = dip->index + 6;	/* CMPCI_xxx_MUTE */
		strcpy(dip->label.name, mixer_port_names[dip->index]);
		dip->un.v.num_channels = (dip->index == CMPCI_MIC_VOL ? 1 : 2);
	vol:
		dip->type = AUDIO_MIXER_VALUE;
		strcpy(dip->un.v.units.name, AudioNvolume);
		return 0;

	case CMPCI_MIC_MUTE:
		dip->next = CMPCI_MIC_PREAMP;
		/* FALLTHROUGH */
	case CMPCI_DAC_MUTE:
	case CMPCI_FM_MUTE:
	case CMPCI_CD_MUTE:
	case CMPCI_LINE_IN_MUTE:
	case CMPCI_AUX_IN_MUTE:
		dip->prev = dip->index - 6;	/* CMPCI_xxx_VOL */
		dip->mixer_class = CMPCI_INPUT_CLASS;
		strcpy(dip->label.name, AudioNmute);
		goto on_off;
	on_off:
		dip->type = AUDIO_MIXER_ENUM;
		dip->un.e.num_mem = 2;
		strcpy(dip->un.e.member[0].label.name, AudioNoff);
		dip->un.e.member[0].ord = 0;
		strcpy(dip->un.e.member[1].label.name, AudioNon);
		dip->un.e.member[1].ord = 1;
		return 0;

	case CMPCI_MIC_PREAMP:
		dip->mixer_class = CMPCI_INPUT_CLASS;
		dip->prev = CMPCI_MIC_MUTE;
		strcpy(dip->label.name, AudioNpreamp);
		goto on_off;
	case CMPCI_PCSPEAKER:
		dip->mixer_class = CMPCI_INPUT_CLASS;
		strcpy(dip->label.name, AudioNspeaker);
		dip->un.v.num_channels = 1;
		dip->un.v.delta = 1 << (8 - CMPCI_SB16_MIXER_SPEAKER_VALBITS);
		goto vol;
	case CMPCI_RECORD_SOURCE:
		dip->mixer_class = CMPCI_RECORD_CLASS;
		strcpy(dip->label.name, AudioNsource);
		dip->type = AUDIO_MIXER_SET;
		dip->un.s.num_mem = 7;
		strcpy(dip->un.s.member[0].label.name, AudioNmicrophone);
		dip->un.s.member[0].mask = CMPCI_RECORD_SOURCE_MIC;
		strcpy(dip->un.s.member[1].label.name, AudioNcd);
		dip->un.s.member[1].mask = CMPCI_RECORD_SOURCE_CD;
		strcpy(dip->un.s.member[2].label.name, AudioNline);
		dip->un.s.member[2].mask = CMPCI_RECORD_SOURCE_LINE_IN;
		strcpy(dip->un.s.member[3].label.name, AudioNaux);
		dip->un.s.member[3].mask = CMPCI_RECORD_SOURCE_AUX_IN;
		strcpy(dip->un.s.member[4].label.name, AudioNwave);
		dip->un.s.member[4].mask = CMPCI_RECORD_SOURCE_WAVE;
		strcpy(dip->un.s.member[5].label.name, AudioNfmsynth);
		dip->un.s.member[5].mask = CMPCI_RECORD_SOURCE_FM;
		strcpy(dip->un.s.member[6].label.name, CmpciNspdif);
		dip->un.s.member[6].mask = CMPCI_RECORD_SOURCE_SPDIF;
		return 0;
	case CMPCI_MIC_RECVOL:
		dip->mixer_class = CMPCI_RECORD_CLASS;
		strcpy(dip->label.name, AudioNmicrophone);
		dip->un.v.num_channels = 1;
		dip->un.v.delta = 1 << (8 - CMPCI_REG_ADMIC_VALBITS);
		goto vol;

	case CMPCI_PLAYBACK_MODE:
		dip->mixer_class = CMPCI_PLAYBACK_CLASS;
		dip->type = AUDIO_MIXER_ENUM;
		strcpy(dip->label.name, AudioNmode);
		dip->un.e.num_mem = 2;
		strcpy(dip->un.e.member[0].label.name, AudioNdac);
		dip->un.e.member[0].ord = CMPCI_PLAYBACK_MODE_WAVE;
		strcpy(dip->un.e.member[1].label.name, CmpciNspdif);
		dip->un.e.member[1].ord = CMPCI_PLAYBACK_MODE_SPDIF;
		return 0;
	case CMPCI_SPDIF_IN_SELECT:
		dip->mixer_class = CMPCI_SPDIF_CLASS;
		dip->type = AUDIO_MIXER_ENUM;
		dip->next = CMPCI_SPDIF_IN_PHASE;
		strcpy(dip->label.name, AudioNinput);
		i = 0;
		strcpy(dip->un.e.member[i].label.name, CmpciNspdin1);
		dip->un.e.member[i++].ord = CMPCI_SPDIF_IN_SPDIN1;
		if (CMPCI_ISCAP(sc, 2ND_SPDIN)) {
			strcpy(dip->un.e.member[i].label.name, CmpciNspdin2);
			dip->un.e.member[i++].ord = CMPCI_SPDIF_IN_SPDIN2;
		}
		strcpy(dip->un.e.member[i].label.name, CmpciNspdout);
		dip->un.e.member[i++].ord = CMPCI_SPDIF_IN_SPDOUT;
		dip->un.e.num_mem = i;
		return 0;
	case CMPCI_SPDIF_IN_PHASE:
		dip->mixer_class = CMPCI_SPDIF_CLASS;
		dip->prev = CMPCI_SPDIF_IN_SELECT;
		strcpy(dip->label.name, CmpciNphase);
		dip->type = AUDIO_MIXER_ENUM;
		dip->un.e.num_mem = 2;
		strcpy(dip->un.e.member[0].label.name, CmpciNpositive);
		dip->un.e.member[0].ord = CMPCI_SPDIF_IN_PHASE_POSITIVE;
		strcpy(dip->un.e.member[1].label.name, CmpciNnegative);
		dip->un.e.member[1].ord = CMPCI_SPDIF_IN_PHASE_NEGATIVE;
		return 0;
	case CMPCI_SPDIF_LOOP:
		dip->mixer_class = CMPCI_SPDIF_CLASS;
		dip->next = CMPCI_SPDIF_OUT_PLAYBACK;
		strcpy(dip->label.name, AudioNoutput);
		dip->type = AUDIO_MIXER_ENUM;
		dip->un.e.num_mem = 2;
		strcpy(dip->un.e.member[0].label.name, CmpciNplayback);
		dip->un.e.member[0].ord = CMPCI_SPDIF_LOOP_OFF;
		strcpy(dip->un.e.member[1].label.name, CmpciNspdin);
		dip->un.e.member[1].ord = CMPCI_SPDIF_LOOP_ON;
		return 0;
	case CMPCI_SPDIF_OUT_PLAYBACK:
		dip->mixer_class = CMPCI_SPDIF_CLASS;
		dip->prev = CMPCI_SPDIF_LOOP;
		dip->next = CMPCI_SPDIF_OUT_VOLTAGE;
		strcpy(dip->label.name, CmpciNplayback);
		dip->type = AUDIO_MIXER_ENUM;
		dip->un.e.num_mem = 2;
		strcpy(dip->un.e.member[0].label.name, AudioNwave);
		dip->un.e.member[0].ord = CMPCI_SPDIF_OUT_PLAYBACK_WAVE;
		strcpy(dip->un.e.member[1].label.name, CmpciNlegacy);
		dip->un.e.member[1].ord = CMPCI_SPDIF_OUT_PLAYBACK_LEGACY;
		return 0;
	case CMPCI_SPDIF_OUT_VOLTAGE:
		dip->mixer_class = CMPCI_SPDIF_CLASS;
		dip->prev = CMPCI_SPDIF_OUT_PLAYBACK;
		strcpy(dip->label.name, CmpciNvoltage);
		dip->type = AUDIO_MIXER_ENUM;
		dip->un.e.num_mem = 2;
		strcpy(dip->un.e.member[0].label.name, CmpciNhigh_v);
		dip->un.e.member[0].ord = CMPCI_SPDIF_OUT_VOLTAGE_HIGH;
		strcpy(dip->un.e.member[1].label.name, CmpciNlow_v);
		dip->un.e.member[1].ord = CMPCI_SPDIF_OUT_VOLTAGE_LOW;
		return 0;
	case CMPCI_MONITOR_DAC:
		dip->mixer_class = CMPCI_SPDIF_CLASS;
		strcpy(dip->label.name, AudioNmonitor);
		dip->type = AUDIO_MIXER_ENUM;
		dip->un.e.num_mem = 3;
		strcpy(dip->un.e.member[0].label.name, AudioNoff);
		dip->un.e.member[0].ord = CMPCI_MONITOR_DAC_OFF;
		strcpy(dip->un.e.member[1].label.name, CmpciNspdin);
		dip->un.e.member[1].ord = CMPCI_MONITOR_DAC_SPDIN;
		strcpy(dip->un.e.member[2].label.name, CmpciNspdout);
		dip->un.e.member[2].ord = CMPCI_MONITOR_DAC_SPDOUT;
		return 0;

	case CMPCI_MASTER_VOL:
		dip->mixer_class = CMPCI_OUTPUT_CLASS;
		strcpy(dip->label.name, AudioNmaster);
		dip->un.v.num_channels = 2;
		dip->un.v.delta = 1 << (8 - CMPCI_SB16_MIXER_VALBITS);
		goto vol;
	case CMPCI_REAR:
		dip->mixer_class = CMPCI_OUTPUT_CLASS;
		dip->next = CMPCI_INDIVIDUAL;
		strcpy(dip->label.name, CmpciNrear);
		goto on_off;
	case CMPCI_INDIVIDUAL:
		dip->mixer_class = CMPCI_OUTPUT_CLASS;
		dip->prev = CMPCI_REAR;
		dip->next = CMPCI_REVERSE;
		strcpy(dip->label.name, CmpciNindividual);
		goto on_off;
	case CMPCI_REVERSE:
		dip->mixer_class = CMPCI_OUTPUT_CLASS;
		dip->prev = CMPCI_INDIVIDUAL;
		strcpy(dip->label.name, CmpciNreverse);
		goto on_off;
	case CMPCI_SURROUND:
		dip->mixer_class = CMPCI_OUTPUT_CLASS;
		strcpy(dip->label.name, CmpciNsurround);
		goto on_off;
	}

	return ENXIO;
}

static int
cmpci_alloc_dmamem(struct cmpci_softc *sc, size_t size, void **r_addr)
{
	int error;
	struct cmpci_dmanode *n;

	error = 0;
	n = kmem_alloc(sizeof(struct cmpci_dmanode), KM_SLEEP);
	if (n == NULL) {
		error = ENOMEM;
		goto quit;
	}

#define CMPCI_DMABUF_ALIGN    0x4
#define CMPCI_DMABUF_BOUNDARY 0x0
	n->cd_tag = sc->sc_dmat;
	n->cd_size = size;
	error = bus_dmamem_alloc(n->cd_tag, n->cd_size,
	    CMPCI_DMABUF_ALIGN, CMPCI_DMABUF_BOUNDARY, n->cd_segs,
	    sizeof(n->cd_segs)/sizeof(n->cd_segs[0]), &n->cd_nsegs,
	    BUS_DMA_WAITOK);
	if (error)
		goto mfree;
	error = bus_dmamem_map(n->cd_tag, n->cd_segs, n->cd_nsegs, n->cd_size,
	    &n->cd_addr, BUS_DMA_WAITOK | BUS_DMA_COHERENT);
	if (error)
		goto dmafree;
	error = bus_dmamap_create(n->cd_tag, n->cd_size, 1, n->cd_size, 0,
	    BUS_DMA_WAITOK, &n->cd_map);
	if (error)
		goto unmap;
	error = bus_dmamap_load(n->cd_tag, n->cd_map, n->cd_addr, n->cd_size,
	    NULL, BUS_DMA_WAITOK);
	if (error)
		goto destroy;

	n->cd_next = sc->sc_dmap;
	sc->sc_dmap = n;
	*r_addr = KVADDR(n);
	return 0;

 destroy:
	bus_dmamap_destroy(n->cd_tag, n->cd_map);
 unmap:
	bus_dmamem_unmap(n->cd_tag, n->cd_addr, n->cd_size);
 dmafree:
	bus_dmamem_free(n->cd_tag,
			n->cd_segs, sizeof(n->cd_segs)/sizeof(n->cd_segs[0]));
 mfree:
	kmem_free(n, sizeof(*n));
 quit:
	return error;
}

static int
cmpci_free_dmamem(struct cmpci_softc *sc, void *addr, size_t size)
{
	struct cmpci_dmanode **nnp;

	for (nnp = &sc->sc_dmap; *nnp; nnp= &(*nnp)->cd_next) {
		if ((*nnp)->cd_addr == addr) {
			struct cmpci_dmanode *n = *nnp;
			bus_dmamap_unload(n->cd_tag, n->cd_map);
			bus_dmamap_destroy(n->cd_tag, n->cd_map);
			bus_dmamem_unmap(n->cd_tag, n->cd_addr, n->cd_size);
			bus_dmamem_free(n->cd_tag, n->cd_segs,
			    sizeof(n->cd_segs)/sizeof(n->cd_segs[0]));
			kmem_free(n, sizeof(*n));
			return 0;
		}
	}
	return -1;
}

static struct cmpci_dmanode *
cmpci_find_dmamem(struct cmpci_softc *sc, void *addr)
{
	struct cmpci_dmanode *p;

	for (p = sc->sc_dmap; p; p = p->cd_next)
		if (KVADDR(p) == (void *)addr)
			break;
	return p;
}

#if 0
static void
cmpci_print_dmamem(struct cmpci_dmanode *);
static void
cmpci_print_dmamem(struct cmpci_dmanode *p)
{

	DPRINTF(("DMA at virt:%p, dmaseg:%p, mapseg:%p, size:%p\n",
		 (void *)p->cd_addr, (void *)p->cd_segs[0].ds_addr,
		 (void *)DMAADDR(p), (void *)p->cd_size));
}
#endif /* DEBUG */

static void *
cmpci_allocm(void *handle, int direction, size_t size)
{
	void *addr;

	addr = NULL;	/* XXX gcc */

	if (cmpci_alloc_dmamem(handle, size, &addr))
		return NULL;
	return addr;
}

static void
cmpci_freem(void *handle, void *addr, size_t size)
{

	cmpci_free_dmamem(handle, addr, size);
}

#define MAXVAL 256
static int
cmpci_adjust(int val, int mask)
{

	val += (MAXVAL - mask) >> 1;
	if (val >= MAXVAL)
		val = MAXVAL-1;
	return val & mask;
}

static void
cmpci_set_mixer_gain(struct cmpci_softc *sc, int port)
{
	int src;
	int bits, mask;

	switch (port) {
	case CMPCI_MIC_VOL:
		cmpci_mixerreg_write(sc, CMPCI_SB16_MIXER_MIC,
		    CMPCI_ADJUST_MIC_GAIN(sc, sc->sc_gain[port][CMPCI_LR]));
		return;
	case CMPCI_MASTER_VOL:
		src = CMPCI_SB16_MIXER_MASTER_L;
		break;
	case CMPCI_LINE_IN_VOL:
		src = CMPCI_SB16_MIXER_LINE_L;
		break;
	case CMPCI_AUX_IN_VOL:
		bus_space_write_1(sc->sc_iot, sc->sc_ioh, CMPCI_REG_MIXER_AUX,
		    CMPCI_ADJUST_AUX_GAIN(sc, sc->sc_gain[port][CMPCI_LEFT],
					      sc->sc_gain[port][CMPCI_RIGHT]));
		return;
	case CMPCI_MIC_RECVOL:
		cmpci_reg_partial_write_1(sc, CMPCI_REG_MIXER25,
		    CMPCI_REG_ADMIC_SHIFT, CMPCI_REG_ADMIC_MASK,
		    CMPCI_ADJUST_ADMIC_GAIN(sc, sc->sc_gain[port][CMPCI_LR]));
		return;
	case CMPCI_DAC_VOL:
		src = CMPCI_SB16_MIXER_VOICE_L;
		break;
	case CMPCI_FM_VOL:
		src = CMPCI_SB16_MIXER_FM_L;
		break;
	case CMPCI_CD_VOL:
		src = CMPCI_SB16_MIXER_CDDA_L;
		break;
	case CMPCI_PCSPEAKER:
		cmpci_mixerreg_write(sc, CMPCI_SB16_MIXER_SPEAKER,
		    CMPCI_ADJUST_2_GAIN(sc, sc->sc_gain[port][CMPCI_LR]));
		return;
	case CMPCI_MIC_PREAMP:
		if (sc->sc_gain[port][CMPCI_LR])
			cmpci_reg_clear_1(sc, CMPCI_REG_MIXER25,
			    CMPCI_REG_MICGAINZ);
		else
			cmpci_reg_set_1(sc, CMPCI_REG_MIXER25,
			    CMPCI_REG_MICGAINZ);
		return;

	case CMPCI_DAC_MUTE:
		if (sc->sc_gain[port][CMPCI_LR])
			cmpci_reg_set_1(sc, CMPCI_REG_MIXER24,
			    CMPCI_REG_WSMUTE);
		else
			cmpci_reg_clear_1(sc, CMPCI_REG_MIXER24,
			    CMPCI_REG_WSMUTE);
		return;
	case CMPCI_FM_MUTE:
		if (sc->sc_gain[port][CMPCI_LR])
			cmpci_reg_set_1(sc, CMPCI_REG_MIXER24,
			    CMPCI_REG_FMMUTE);
		else
			cmpci_reg_clear_1(sc, CMPCI_REG_MIXER24,
			    CMPCI_REG_FMMUTE);
		return;
	case CMPCI_AUX_IN_MUTE:
		if (sc->sc_gain[port][CMPCI_LR])
			cmpci_reg_clear_1(sc, CMPCI_REG_MIXER25,
			    CMPCI_REG_VAUXRM|CMPCI_REG_VAUXLM);
		else
			cmpci_reg_set_1(sc, CMPCI_REG_MIXER25,
			    CMPCI_REG_VAUXRM|CMPCI_REG_VAUXLM);
		return;
	case CMPCI_CD_MUTE:
		mask = CMPCI_SB16_SW_CD;
		goto sbmute;
	case CMPCI_MIC_MUTE:
		mask = CMPCI_SB16_SW_MIC;
		goto sbmute;
	case CMPCI_LINE_IN_MUTE:
		mask = CMPCI_SB16_SW_LINE;
	sbmute:
		bits = cmpci_mixerreg_read(sc, CMPCI_SB16_MIXER_OUTMIX);
		if (sc->sc_gain[port][CMPCI_LR])
			bits = bits & ~mask;
		else
			bits = bits | mask;
		cmpci_mixerreg_write(sc, CMPCI_SB16_MIXER_OUTMIX, bits);
		return;

	case CMPCI_SPDIF_IN_SELECT:
	case CMPCI_MONITOR_DAC:
	case CMPCI_PLAYBACK_MODE:
	case CMPCI_SPDIF_LOOP:
	case CMPCI_SPDIF_OUT_PLAYBACK:
		cmpci_set_out_ports(sc);
		return;
	case CMPCI_SPDIF_OUT_VOLTAGE:
		if (CMPCI_ISCAP(sc, SPDOUT_VOLTAGE)) {
			if (sc->sc_gain[CMPCI_SPDIF_OUT_VOLTAGE][CMPCI_LR]
			    == CMPCI_SPDIF_OUT_VOLTAGE_HIGH)
				cmpci_reg_clear_reg_misc(sc, CMPCI_REG_5V);
			else
				cmpci_reg_set_reg_misc(sc, CMPCI_REG_5V);
		}
		return;
	case CMPCI_SURROUND:
		if (CMPCI_ISCAP(sc, SURROUND)) {
			if (sc->sc_gain[CMPCI_SURROUND][CMPCI_LR])
				cmpci_reg_set_1(sc, CMPCI_REG_MIXER24,
						CMPCI_REG_SURROUND);
			else
				cmpci_reg_clear_1(sc, CMPCI_REG_MIXER24,
						  CMPCI_REG_SURROUND);
		}
		return;
	case CMPCI_REAR:
		if (CMPCI_ISCAP(sc, REAR)) {
			if (sc->sc_gain[CMPCI_REAR][CMPCI_LR])
				cmpci_reg_set_reg_misc(sc, CMPCI_REG_N4SPK3D);
			else
				cmpci_reg_clear_reg_misc(sc, CMPCI_REG_N4SPK3D);
		}
		return;
	case CMPCI_INDIVIDUAL:
		if (CMPCI_ISCAP(sc, INDIVIDUAL_REAR)) {
			if (sc->sc_gain[CMPCI_REAR][CMPCI_LR])
				cmpci_reg_set_1(sc, CMPCI_REG_MIXER24,
						CMPCI_REG_INDIVIDUAL);
			else
				cmpci_reg_clear_1(sc, CMPCI_REG_MIXER24,
						  CMPCI_REG_INDIVIDUAL);
		}
		return;
	case CMPCI_REVERSE:
		if (CMPCI_ISCAP(sc, REVERSE_FR)) {
			if (sc->sc_gain[CMPCI_REVERSE][CMPCI_LR])
				cmpci_reg_set_1(sc, CMPCI_REG_MIXER24,
						CMPCI_REG_REVERSE_FR);
			else
				cmpci_reg_clear_1(sc, CMPCI_REG_MIXER24,
						  CMPCI_REG_REVERSE_FR);
		}
		return;
	case CMPCI_SPDIF_IN_PHASE:
		if (CMPCI_ISCAP(sc, SPDIN_PHASE)) {
			if (sc->sc_gain[CMPCI_SPDIF_IN_PHASE][CMPCI_LR]
			    == CMPCI_SPDIF_IN_PHASE_POSITIVE)
				cmpci_reg_clear_1(sc, CMPCI_REG_CHANNEL_FORMAT,
						  CMPCI_REG_SPDIN_PHASE);
			else
				cmpci_reg_set_1(sc, CMPCI_REG_CHANNEL_FORMAT,
						CMPCI_REG_SPDIN_PHASE);
		}
		return;
	default:
		return;
	}

	cmpci_mixerreg_write(sc, src,
	    CMPCI_ADJUST_GAIN(sc, sc->sc_gain[port][CMPCI_LEFT]));
	cmpci_mixerreg_write(sc, CMPCI_SB16_MIXER_L_TO_R(src),
	    CMPCI_ADJUST_GAIN(sc, sc->sc_gain[port][CMPCI_RIGHT]));
}

static void
cmpci_set_out_ports(struct cmpci_softc *sc)
{
	uint8_t v;
	int enspdout;

	if (!CMPCI_ISCAP(sc, SPDLOOP))
		return;

	/* SPDIF/out select */
	if (sc->sc_gain[CMPCI_SPDIF_LOOP][CMPCI_LR] == CMPCI_SPDIF_LOOP_OFF) {
		/* playback */
		cmpci_reg_clear_4(sc, CMPCI_REG_FUNC_1, CMPCI_REG_SPDIF_LOOP);
	} else {
		/* monitor SPDIF/in */
		cmpci_reg_set_4(sc, CMPCI_REG_FUNC_1, CMPCI_REG_SPDIF_LOOP);
	}

	/* SPDIF in select */
	v = sc->sc_gain[CMPCI_SPDIF_IN_SELECT][CMPCI_LR];
	if (v & CMPCI_SPDIFIN_SPDIFIN2)
		cmpci_reg_set_reg_misc(sc, CMPCI_REG_2ND_SPDIFIN);
	else
		cmpci_reg_clear_reg_misc(sc, CMPCI_REG_2ND_SPDIFIN);
	if (v & CMPCI_SPDIFIN_SPDIFOUT)
		cmpci_reg_set_reg_misc(sc, CMPCI_REG_SPDFLOOPI);
	else
		cmpci_reg_clear_reg_misc(sc, CMPCI_REG_SPDFLOOPI);

	enspdout = 0;
	/* playback to ... */
	if (CMPCI_ISCAP(sc, SPDOUT) &&
	    sc->sc_gain[CMPCI_PLAYBACK_MODE][CMPCI_LR]
		== CMPCI_PLAYBACK_MODE_SPDIF &&
	    (sc->sc_play.md_divide == CMPCI_REG_RATE_44100 ||
		(CMPCI_ISCAP(sc, SPDOUT_48K) &&
		    sc->sc_play.md_divide==CMPCI_REG_RATE_48000))) {
		/* playback to SPDIF */
		cmpci_reg_set_4(sc, CMPCI_REG_FUNC_1, CMPCI_REG_SPDIF0_ENABLE);
		enspdout = 1;
		if (sc->sc_play.md_divide==CMPCI_REG_RATE_48000)
			cmpci_reg_set_reg_misc(sc,
				CMPCI_REG_SPDIFOUT_48K | CMPCI_REG_SPDIF48K);
		else
			cmpci_reg_clear_reg_misc(sc,
				CMPCI_REG_SPDIFOUT_48K | CMPCI_REG_SPDIF48K);
	} else {
		/* playback to DAC */
		cmpci_reg_clear_4(sc, CMPCI_REG_FUNC_1,
				  CMPCI_REG_SPDIF0_ENABLE);
		if (CMPCI_ISCAP(sc, SPDOUT_48K))
			cmpci_reg_clear_reg_misc(sc,
				CMPCI_REG_SPDIFOUT_48K | CMPCI_REG_SPDIF48K);
	}

	/* legacy to SPDIF/out or not */
	if (CMPCI_ISCAP(sc, SPDLEGACY)) {
		if (sc->sc_gain[CMPCI_SPDIF_OUT_PLAYBACK][CMPCI_LR]
		    == CMPCI_SPDIF_OUT_PLAYBACK_WAVE)
			cmpci_reg_clear_4(sc, CMPCI_REG_LEGACY_CTRL,
					CMPCI_REG_LEGACY_SPDIF_ENABLE);
		else {
			cmpci_reg_set_4(sc, CMPCI_REG_LEGACY_CTRL,
					CMPCI_REG_LEGACY_SPDIF_ENABLE);
			enspdout = 1;
		}
	}

	/* enable/disable SPDIF/out */
	if (CMPCI_ISCAP(sc, XSPDOUT) && enspdout)
		cmpci_reg_set_4(sc, CMPCI_REG_LEGACY_CTRL,
				CMPCI_REG_XSPDIF_ENABLE);
	else
		cmpci_reg_clear_4(sc, CMPCI_REG_LEGACY_CTRL,
				CMPCI_REG_XSPDIF_ENABLE);

	/* SPDIF monitor (digital to analog output) */
	if (CMPCI_ISCAP(sc, SPDIN_MONITOR)) {
		v = sc->sc_gain[CMPCI_MONITOR_DAC][CMPCI_LR];
		if (!(v & CMPCI_MONDAC_ENABLE))
			cmpci_reg_clear_1(sc, CMPCI_REG_MIXER24,
					CMPCI_REG_SPDIN_MONITOR);
		if (v & CMPCI_MONDAC_SPDOUT)
			cmpci_reg_set_4(sc, CMPCI_REG_FUNC_1,
					CMPCI_REG_SPDIFOUT_DAC);
		else
			cmpci_reg_clear_4(sc, CMPCI_REG_FUNC_1,
					CMPCI_REG_SPDIFOUT_DAC);
		if (v & CMPCI_MONDAC_ENABLE)
			cmpci_reg_set_1(sc, CMPCI_REG_MIXER24,
					CMPCI_REG_SPDIN_MONITOR);
	}
}

static int
cmpci_set_in_ports(struct cmpci_softc *sc)
{
	int mask;
	int bitsl, bitsr;

	mask = sc->sc_in_mask;

	/*
	 * Note CMPCI_RECORD_SOURCE_CD, CMPCI_RECORD_SOURCE_LINE_IN and
	 * CMPCI_RECORD_SOURCE_FM are defined to the corresponding bit
	 * of the mixer register.
	 */
	bitsr = mask & (CMPCI_RECORD_SOURCE_CD | CMPCI_RECORD_SOURCE_LINE_IN |
	    CMPCI_RECORD_SOURCE_FM);

	bitsl = CMPCI_SB16_MIXER_SRC_R_TO_L(bitsr);
	if (mask & CMPCI_RECORD_SOURCE_MIC) {
		bitsl |= CMPCI_SB16_MIXER_MIC_SRC;
		bitsr |= CMPCI_SB16_MIXER_MIC_SRC;
	}
	cmpci_mixerreg_write(sc, CMPCI_SB16_MIXER_ADCMIX_L, bitsl);
	cmpci_mixerreg_write(sc, CMPCI_SB16_MIXER_ADCMIX_R, bitsr);

	if (mask & CMPCI_RECORD_SOURCE_AUX_IN)
		cmpci_reg_set_1(sc, CMPCI_REG_MIXER25,
		    CMPCI_REG_RAUXREN | CMPCI_REG_RAUXLEN);
	else
		cmpci_reg_clear_1(sc, CMPCI_REG_MIXER25,
		    CMPCI_REG_RAUXREN | CMPCI_REG_RAUXLEN);

	if (mask & CMPCI_RECORD_SOURCE_WAVE)
		cmpci_reg_set_1(sc, CMPCI_REG_MIXER24,
		    CMPCI_REG_WAVEINL | CMPCI_REG_WAVEINR);
	else
		cmpci_reg_clear_1(sc, CMPCI_REG_MIXER24,
		    CMPCI_REG_WAVEINL | CMPCI_REG_WAVEINR);

	if (CMPCI_ISCAP(sc, SPDIN) &&
	    (sc->sc_rec.md_divide == CMPCI_REG_RATE_44100 ||
		(CMPCI_ISCAP(sc, SPDOUT_48K) &&
		    sc->sc_rec.md_divide == CMPCI_REG_RATE_48000/* XXX? */))) {
		if (mask & CMPCI_RECORD_SOURCE_SPDIF) {
			/* enable SPDIF/in */
			cmpci_reg_set_4(sc,
					CMPCI_REG_FUNC_1,
					CMPCI_REG_SPDIF1_ENABLE);
		} else {
			cmpci_reg_clear_4(sc,
					CMPCI_REG_FUNC_1,
					CMPCI_REG_SPDIF1_ENABLE);
		}
	}

	return 0;
}

static int
cmpci_set_port(void *handle, mixer_ctrl_t *cp)
{
	struct cmpci_softc *sc;
	int lgain, rgain;

	sc = handle;
	switch (cp->dev) {
	case CMPCI_MIC_VOL:
	case CMPCI_PCSPEAKER:
	case CMPCI_MIC_RECVOL:
		if (cp->un.value.num_channels != 1)
			return EINVAL;
		/* FALLTHROUGH */
	case CMPCI_DAC_VOL:
	case CMPCI_FM_VOL:
	case CMPCI_CD_VOL:
	case CMPCI_LINE_IN_VOL:
	case CMPCI_AUX_IN_VOL:
	case CMPCI_MASTER_VOL:
		if (cp->type != AUDIO_MIXER_VALUE)
			return EINVAL;
		switch (cp->un.value.num_channels) {
		case 1:
			lgain = rgain =
			    cp->un.value.level[AUDIO_MIXER_LEVEL_MONO];
			break;
		case 2:
			lgain = cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT];
			rgain = cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT];
			break;
		default:
			return EINVAL;
		}
		sc->sc_gain[cp->dev][CMPCI_LEFT]  = lgain;
		sc->sc_gain[cp->dev][CMPCI_RIGHT] = rgain;

		cmpci_set_mixer_gain(sc, cp->dev);
		break;

	case CMPCI_RECORD_SOURCE:
		if (cp->type != AUDIO_MIXER_SET)
			return EINVAL;

		if (cp->un.mask & ~(CMPCI_RECORD_SOURCE_MIC |
		    CMPCI_RECORD_SOURCE_CD | CMPCI_RECORD_SOURCE_LINE_IN |
		    CMPCI_RECORD_SOURCE_AUX_IN | CMPCI_RECORD_SOURCE_WAVE |
		    CMPCI_RECORD_SOURCE_FM | CMPCI_RECORD_SOURCE_SPDIF))
			return EINVAL;

		if (cp->un.mask & CMPCI_RECORD_SOURCE_SPDIF)
			cp->un.mask = CMPCI_RECORD_SOURCE_SPDIF;

		sc->sc_in_mask = cp->un.mask;
		return cmpci_set_in_ports(sc);

	/* boolean */
	case CMPCI_DAC_MUTE:
	case CMPCI_FM_MUTE:
	case CMPCI_CD_MUTE:
	case CMPCI_LINE_IN_MUTE:
	case CMPCI_AUX_IN_MUTE:
	case CMPCI_MIC_MUTE:
	case CMPCI_MIC_PREAMP:
	case CMPCI_PLAYBACK_MODE:
	case CMPCI_SPDIF_IN_PHASE:
	case CMPCI_SPDIF_LOOP:
	case CMPCI_SPDIF_OUT_PLAYBACK:
	case CMPCI_SPDIF_OUT_VOLTAGE:
	case CMPCI_REAR:
	case CMPCI_INDIVIDUAL:
	case CMPCI_REVERSE:
	case CMPCI_SURROUND:
		if (cp->type != AUDIO_MIXER_ENUM)
			return EINVAL;
		sc->sc_gain[cp->dev][CMPCI_LR] = cp->un.ord != 0;
		cmpci_set_mixer_gain(sc, cp->dev);
		break;

	case CMPCI_SPDIF_IN_SELECT:
		switch (cp->un.ord) {
		case CMPCI_SPDIF_IN_SPDIN1:
		case CMPCI_SPDIF_IN_SPDIN2:
		case CMPCI_SPDIF_IN_SPDOUT:
			break;
		default:
			return EINVAL;
		}
		goto xenum;
	case CMPCI_MONITOR_DAC:
		switch (cp->un.ord) {
		case CMPCI_MONITOR_DAC_OFF:
		case CMPCI_MONITOR_DAC_SPDIN:
		case CMPCI_MONITOR_DAC_SPDOUT:
			break;
		default:
			return EINVAL;
		}
	xenum:
		if (cp->type != AUDIO_MIXER_ENUM)
			return EINVAL;
		sc->sc_gain[cp->dev][CMPCI_LR] = cp->un.ord;
		cmpci_set_mixer_gain(sc, cp->dev);
		break;

	default:
	    return EINVAL;
	}

	return 0;
}

static int
cmpci_get_port(void *handle, mixer_ctrl_t *cp)
{
	struct cmpci_softc *sc;

	sc = handle;
	switch (cp->dev) {
	case CMPCI_MIC_VOL:
	case CMPCI_PCSPEAKER:
	case CMPCI_MIC_RECVOL:
		if (cp->un.value.num_channels != 1)
			return EINVAL;
		/*FALLTHROUGH*/
	case CMPCI_DAC_VOL:
	case CMPCI_FM_VOL:
	case CMPCI_CD_VOL:
	case CMPCI_LINE_IN_VOL:
	case CMPCI_AUX_IN_VOL:
	case CMPCI_MASTER_VOL:
		switch (cp->un.value.num_channels) {
		case 1:
			cp->un.value.level[AUDIO_MIXER_LEVEL_MONO] =
				sc->sc_gain[cp->dev][CMPCI_LEFT];
			break;
		case 2:
			cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT] =
				sc->sc_gain[cp->dev][CMPCI_LEFT];
			cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT] =
				sc->sc_gain[cp->dev][CMPCI_RIGHT];
			break;
		default:
			return EINVAL;
		}
		break;

	case CMPCI_RECORD_SOURCE:
		cp->un.mask = sc->sc_in_mask;
		break;

	case CMPCI_DAC_MUTE:
	case CMPCI_FM_MUTE:
	case CMPCI_CD_MUTE:
	case CMPCI_LINE_IN_MUTE:
	case CMPCI_AUX_IN_MUTE:
	case CMPCI_MIC_MUTE:
	case CMPCI_MIC_PREAMP:
	case CMPCI_PLAYBACK_MODE:
	case CMPCI_SPDIF_IN_SELECT:
	case CMPCI_SPDIF_IN_PHASE:
	case CMPCI_SPDIF_LOOP:
	case CMPCI_SPDIF_OUT_PLAYBACK:
	case CMPCI_SPDIF_OUT_VOLTAGE:
	case CMPCI_MONITOR_DAC:
	case CMPCI_REAR:
	case CMPCI_INDIVIDUAL:
	case CMPCI_REVERSE:
	case CMPCI_SURROUND:
		cp->un.ord = sc->sc_gain[cp->dev][CMPCI_LR];
		break;

	default:
		return EINVAL;
	}

	return 0;
}

/* ARGSUSED */
static size_t
cmpci_round_buffersize(void *handle, int direction,
    size_t bufsize)
{

	if (bufsize > 0x10000)
		bufsize = 0x10000;

	return bufsize;
}

static paddr_t
cmpci_mappage(void *handle, void *addr, off_t offset, int prot)
{
	struct cmpci_dmanode *p;

	if (offset < 0 || NULL == (p = cmpci_find_dmamem(handle, addr)))
		return -1;

	return bus_dmamem_mmap(p->cd_tag, p->cd_segs,
		   sizeof(p->cd_segs)/sizeof(p->cd_segs[0]),
		   offset, prot, BUS_DMA_WAITOK);
}

/* ARGSUSED */
static int
cmpci_get_props(void *handle)
{

	return AUDIO_PROP_MMAP | AUDIO_PROP_INDEPENDENT | AUDIO_PROP_FULLDUPLEX;
}

static int
cmpci_trigger_output(void *handle, void *start, void *end, int blksize,
		     void (*intr)(void *), void *arg,
		     const audio_params_t *param)
{
	struct cmpci_softc *sc;
	struct cmpci_dmanode *p;
	int bps;

	sc = handle;
	sc->sc_play.intr = intr;
	sc->sc_play.intr_arg = arg;
	bps = param->channels * param->precision / 8;
	if (!bps)
		return EINVAL;

	/* set DMA frame */
	if (!(p = cmpci_find_dmamem(sc, start)))
		return EINVAL;
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, CMPCI_REG_DMA0_BASE,
	    DMAADDR(p));
	delay(10);
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, CMPCI_REG_DMA0_BYTES,
	    ((char *)end - (char *)start + 1) / bps - 1);
	delay(10);

	/* set interrupt count */
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, CMPCI_REG_DMA0_SAMPLES,
			  (blksize + bps - 1) / bps - 1);
	delay(10);

	/* start DMA */
	cmpci_reg_clear_4(sc, CMPCI_REG_FUNC_0, CMPCI_REG_CH0_DIR); /* PLAY */
	cmpci_reg_set_4(sc, CMPCI_REG_INTR_CTRL, CMPCI_REG_CH0_INTR_ENABLE);
	cmpci_reg_set_4(sc, CMPCI_REG_FUNC_0, CMPCI_REG_CH0_ENABLE);

	return 0;
}

static int
cmpci_trigger_input(void *handle, void *start, void *end, int blksize,
		    void (*intr)(void *), void *arg,
		    const audio_params_t *param)
{
	struct cmpci_softc *sc;
	struct cmpci_dmanode *p;
	int bps;

	sc = handle;
	sc->sc_rec.intr = intr;
	sc->sc_rec.intr_arg = arg;
	bps = param->channels * param->precision / 8;
	if (!bps)
		return EINVAL;

	/* set DMA frame */
	if (!(p=cmpci_find_dmamem(sc, start)))
		return EINVAL;
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, CMPCI_REG_DMA1_BASE,
	    DMAADDR(p));
	delay(10);
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, CMPCI_REG_DMA1_BYTES,
	    ((char *)end - (char *)start + 1) / bps - 1);
	delay(10);

	/* set interrupt count */
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, CMPCI_REG_DMA1_SAMPLES,
	    (blksize + bps - 1) / bps - 1);
	delay(10);

	/* start DMA */
	cmpci_reg_set_4(sc, CMPCI_REG_FUNC_0, CMPCI_REG_CH1_DIR); /* REC */
	cmpci_reg_set_4(sc, CMPCI_REG_INTR_CTRL, CMPCI_REG_CH1_INTR_ENABLE);
	cmpci_reg_set_4(sc, CMPCI_REG_FUNC_0, CMPCI_REG_CH1_ENABLE);

	return 0;
}

static void
cmpci_get_locks(void *addr, kmutex_t **intr, kmutex_t **thread)
{
	struct cmpci_softc *sc;

	sc = addr;
	*intr = &sc->sc_intr_lock;
	*thread = &sc->sc_lock;
}

/* end of file */
