/*	$NetBSD: auvia.c,v 1.77 2014/03/29 19:28:24 christos Exp $	*/

/*-
 * Copyright (c) 2000, 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Tyler C. Sarna.
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
 * VIA Technologies VT82C686A / VT8233 / VT8235 Southbridge Audio Driver
 *
 * Documentation links:
 *
 * ftp://ftp.alsa-project.org/pub/manuals/via/686a.pdf
 * ftp://ftp.alsa-project.org/pub/manuals/general/ac97r21.pdf
 * ftp://ftp.alsa-project.org/pub/manuals/ad/AD1881_0.pdf (example AC'97 codec)
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: auvia.c,v 1.77 2014/03/29 19:28:24 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kmem.h>
#include <sys/device.h>
#include <sys/audioio.h>

#include <dev/pci/pcidevs.h>
#include <dev/pci/pcivar.h>

#include <dev/audio_if.h>
#include <dev/mulaw.h>
#include <dev/auconv.h>

#include <dev/ic/ac97reg.h>
#include <dev/ic/ac97var.h>

#include <dev/pci/auviavar.h>

#define	AUVIA_MINBLKSZ		512

struct auvia_dma {
	struct auvia_dma *next;
	void *addr;
	size_t size;
	bus_dmamap_t map;
	bus_dma_segment_t seg;
};

struct auvia_dma_op {
	uint32_t ptr;
	uint32_t flags;
#define AUVIA_DMAOP_EOL		0x80000000
#define AUVIA_DMAOP_FLAG	0x40000000
#define AUVIA_DMAOP_STOP	0x20000000
#define AUVIA_DMAOP_COUNT(x)	((x)&0x00FFFFFF)
};

static int	auvia_match(device_t, cfdata_t, void *);
static void	auvia_attach(device_t, device_t, void *);
static int	auvia_detach(device_t, int);
static void	auvia_childdet(device_t, device_t);
static int	auvia_open(void *, int);
static void	auvia_close(void *);
static int	auvia_query_encoding(void *, struct audio_encoding *);
static void	auvia_set_params_sub(struct auvia_softc *,
				     struct auvia_softc_chan *,
				     const audio_params_t *);
static int	auvia_set_params(void *, int, int, audio_params_t *,
				 audio_params_t *, stream_filter_list_t *,
				 stream_filter_list_t *);
static int	auvia_round_blocksize(void *, int, int, const audio_params_t *);
static int	auvia_halt_output(void *);
static int	auvia_halt_input(void *);
static int	auvia_getdev(void *, struct audio_device *);
static int	auvia_set_port(void *, mixer_ctrl_t *);
static int	auvia_get_port(void *, mixer_ctrl_t *);
static int	auvia_query_devinfo(void *, mixer_devinfo_t *);
static void *	auvia_malloc(void *, int, size_t);
static void	auvia_free(void *, void *, size_t);
static size_t	auvia_round_buffersize(void *, int, size_t);
static paddr_t	auvia_mappage(void *, void *, off_t, int);
static int	auvia_get_props(void *);
static int	auvia_build_dma_ops(struct auvia_softc *,
				    struct auvia_softc_chan *,
				    struct auvia_dma *, void *, void *, int);
static int	auvia_trigger_output(void *, void *, void *, int,
				     void (*)(void *), void *,
				     const audio_params_t *);
static int	auvia_trigger_input(void *, void *, void *, int,
				    void (*)(void *), void *,
				    const audio_params_t *);
static void	auvia_get_locks(void *, kmutex_t **, kmutex_t **);
static bool	auvia_resume(device_t, const pmf_qual_t *);

static int	auvia_intr(void *);
static void *	auvia_malloc_dmamem(void *, int, size_t);
static int	auvia_malloc_channel(struct auvia_softc *, struct auvia_softc_chan *,
				     size_t);
static int	auvia_attach_codec(void *, struct ac97_codec_if *);
static int	auvia_write_codec(void *, uint8_t, uint16_t);
static int	auvia_read_codec(void *, uint8_t, uint16_t *);
static int	auvia_reset_codec(void *);
static int	auvia_waitready_codec(struct auvia_softc *);
static int	auvia_waitvalid_codec(struct auvia_softc *);
static void	auvia_spdif_event(void *, bool);

CFATTACH_DECL2_NEW(auvia, sizeof (struct auvia_softc),
    auvia_match, auvia_attach, auvia_detach, NULL, NULL, auvia_childdet);

/* VIA VT823xx revision number */
#define VIA_REV_8233PRE	0x10
#define VIA_REV_8233C	0x20
#define VIA_REV_8233	0x30
#define VIA_REV_8233A	0x40
#define VIA_REV_8235	0x50
#define VIA_REV_8237	0x60

#define AUVIA_PCICONF_JUNK	0x40
#define		AUVIA_PCICONF_ENABLES	 0x00FF0000	/* reg 42 mask */
#define		AUVIA_PCICONF_ACLINKENAB 0x00008000	/* ac link enab */
#define		AUVIA_PCICONF_ACNOTRST	 0x00004000	/* ~(ac reset) */
#define		AUVIA_PCICONF_ACSYNC	 0x00002000	/* ac sync */
#define		AUVIA_PCICONF_ACVSR	 0x00000800	/* var. samp. rate */
#define		AUVIA_PCICONF_ACSGD	 0x00000400	/* SGD enab */
#define		AUVIA_PCICONF_ACFM	 0x00000200	/* FM enab */
#define		AUVIA_PCICONF_ACSB	 0x00000100	/* SB enab */
#define		AUVIA_PCICONF_PRIVALID	 0x00000001	/* primary codec rdy */

#define	AUVIA_PLAY_BASE			0x00
#define	AUVIA_RECORD_BASE		0x10

/* *_RP_* are offsets from AUVIA_PLAY_BASE or AUVIA_RECORD_BASE */
#define	AUVIA_RP_STAT			0x00
#define		AUVIA_RPSTAT_INTR		0x03
#define	AUVIA_RP_CONTROL		0x01
#define		AUVIA_RPCTRL_START		0x80
#define		AUVIA_RPCTRL_TERMINATE		0x40
#define		AUVIA_RPCTRL_AUTOSTART		0x20
/* The following are 8233 specific */
#define		AUVIA_RPCTRL_STOP		0x04
#define		AUVIA_RPCTRL_EOL		0x02
#define		AUVIA_RPCTRL_FLAG		0x01
#define	AUVIA_RP_MODE			0x02		/* 82c686 specific */
#define		AUVIA_RPMODE_INTR_FLAG		0x01
#define		AUVIA_RPMODE_INTR_EOL		0x02
#define		AUVIA_RPMODE_STEREO		0x10
#define		AUVIA_RPMODE_16BIT		0x20
#define		AUVIA_RPMODE_AUTOSTART		0x80
#define	AUVIA_RP_DMAOPS_BASE		0x04

#define	VIA8233_RP_DXS_LVOL		0x02
#define	VIA8233_RP_DXS_RVOL		0x03
#define	VIA8233_RP_RATEFMT		0x08
#define		VIA8233_RATEFMT_48K		0xfffff
#define		VIA8233_RATEFMT_STEREO		0x00100000
#define		VIA8233_RATEFMT_16BIT		0x00200000

#define	VIA_RP_DMAOPS_COUNT		0x0c

#define VIA8233_MP_BASE			0x40
	/* STAT, CONTROL, DMAOPS_BASE, DMAOPS_COUNT are valid */
#define VIA8233_OFF_MP_FORMAT		0x02
#define		VIA8233_MP_FORMAT_8BIT		0x00
#define		VIA8233_MP_FORMAT_16BIT		0x80
#define		VIA8233_MP_FORMAT_CHANNLE_MASK	0x70 /* 1, 2, 4, 6 */
#define VIA8233_OFF_MP_SCRATCH		0x03
#define VIA8233_OFF_MP_STOP		0x08

#define VIA8233_WR_BASE			0x60

#define	AUVIA_CODEC_CTL			0x80
#define		AUVIA_CODEC_READ		0x00800000
#define		AUVIA_CODEC_BUSY		0x01000000
#define		AUVIA_CODEC_PRIVALID		0x02000000
#define		AUVIA_CODEC_INDEX(x)		((x)<<16)

#define CH_WRITE1(sc, ch, off, v)	\
	bus_space_write_1((sc)->sc_iot,	(sc)->sc_ioh, (ch)->sc_base + (off), v)
#define CH_WRITE4(sc, ch, off, v)	\
	bus_space_write_4((sc)->sc_iot,	(sc)->sc_ioh, (ch)->sc_base + (off), v)
#define CH_READ1(sc, ch, off)		\
	bus_space_read_1((sc)->sc_iot, (sc)->sc_ioh, (ch)->sc_base + (off))
#define CH_READ4(sc, ch, off)		\
	bus_space_read_4((sc)->sc_iot, (sc)->sc_ioh, (ch)->sc_base + (off))

#define TIMEOUT	50

static const struct audio_hw_if auvia_hw_if = {
	auvia_open,
	auvia_close,
	NULL, /* drain */
	auvia_query_encoding,
	auvia_set_params,
	auvia_round_blocksize,
	NULL, /* commit_settings */
	NULL, /* init_output */
	NULL, /* init_input */
	NULL, /* start_output */
	NULL, /* start_input */
	auvia_halt_output,
	auvia_halt_input,
	NULL, /* speaker_ctl */
	auvia_getdev,
	NULL, /* setfd */
	auvia_set_port,
	auvia_get_port,
	auvia_query_devinfo,
	auvia_malloc,
	auvia_free,
	auvia_round_buffersize,
	auvia_mappage,
	auvia_get_props,
	auvia_trigger_output,
	auvia_trigger_input,
	NULL, /* dev_ioctl */
	auvia_get_locks,
};

#define AUVIA_FORMATS_4CH_16	2
#define AUVIA_FORMATS_6CH_16	3
#define AUVIA_FORMATS_4CH_8	6
#define AUVIA_FORMATS_6CH_8	7
static const struct audio_format auvia_formats[AUVIA_NFORMATS] = {
	{NULL, AUMODE_PLAY | AUMODE_RECORD, AUDIO_ENCODING_SLINEAR_LE, 16, 16,
	 1, AUFMT_MONAURAL, 0, {8000, 48000}},
	{NULL, AUMODE_PLAY | AUMODE_RECORD, AUDIO_ENCODING_SLINEAR_LE, 16, 16,
	 2, AUFMT_STEREO, 0, {8000, 48000}},
	{NULL, AUMODE_PLAY, AUDIO_ENCODING_SLINEAR_LE, 16, 16,
	 4, AUFMT_SURROUND4, 0, {8000, 48000}},
	{NULL, AUMODE_PLAY, AUDIO_ENCODING_SLINEAR_LE, 16, 16,
	 6, AUFMT_DOLBY_5_1, 0, {8000, 48000}},
	{NULL, AUMODE_PLAY | AUMODE_RECORD, AUDIO_ENCODING_ULINEAR_LE, 8, 8,
	 1, AUFMT_MONAURAL, 0, {8000, 48000}},
	{NULL, AUMODE_PLAY | AUMODE_RECORD, AUDIO_ENCODING_ULINEAR_LE, 8, 8,
	 2, AUFMT_STEREO, 0, {8000, 48000}},
	{NULL, AUMODE_PLAY, AUDIO_ENCODING_ULINEAR_LE, 8, 8,
	 4, AUFMT_SURROUND4, 0, {8000, 48000}},
	{NULL, AUMODE_PLAY, AUDIO_ENCODING_SLINEAR_LE, 8, 8,
	 6, AUFMT_DOLBY_5_1, 0, {8000, 48000}},
};

#define	AUVIA_SPDIF_NFORMATS	1
static const struct audio_format auvia_spdif_formats[AUVIA_SPDIF_NFORMATS] = {
	{NULL, AUMODE_PLAY | AUMODE_RECORD, AUDIO_ENCODING_SLINEAR_LE, 16, 16,
	 2, AUFMT_STEREO, 1, {48000}},
};


static int
auvia_match(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa;

	pa = (struct pci_attach_args *) aux;
	if (PCI_VENDOR(pa->pa_id) != PCI_VENDOR_VIATECH)
		return 0;
	switch (PCI_PRODUCT(pa->pa_id)) {
	case PCI_PRODUCT_VIATECH_VT82C686A_AC97:
	case PCI_PRODUCT_VIATECH_VT8233_AC97:
		break;
	default:
		return 0;
	}

	return 1;
}

static void
auvia_childdet(device_t self, device_t child)
{
	/* we hold no child references, so do nothing */
}

static int
auvia_detach(device_t self, int flags)
{
	int rc;
	struct auvia_softc *sc = device_private(self);

	if ((rc = config_detach_children(self, flags)) != 0)
		return rc;
	pmf_device_deregister(self);

	mutex_enter(&sc->sc_lock);
	auconv_delete_encodings(sc->sc_encodings);
	auconv_delete_encodings(sc->sc_spdif_encodings);
	if (sc->codec_if != NULL)
		sc->codec_if->vtbl->detach(sc->codec_if);
	mutex_exit(&sc->sc_lock);

	/* XXX restore compatibility? */
	if (sc->sc_ih != NULL)
		pci_intr_disestablish(sc->sc_pc, sc->sc_ih);

	bus_space_unmap(sc->sc_iot, sc->sc_ioh, sc->sc_iosize);
	if (sc->sc_play.sc_dma_ops != NULL) {
		auvia_free(sc, sc->sc_play.sc_dma_ops,
		    sc->sc_play.sc_dma_op_count *
		    sizeof(struct auvia_dma_op));
	}
	if (sc->sc_record.sc_dma_ops != NULL) {
		auvia_free(sc, sc->sc_record.sc_dma_ops,
		    sc->sc_play.sc_dma_op_count *
		    sizeof(struct auvia_dma_op));
	}
	mutex_destroy(&sc->sc_lock);
	mutex_destroy(&sc->sc_intr_lock);

	return 0;
}

static void
auvia_attach(device_t parent, device_t self, void *aux)
{
	struct pci_attach_args *pa;
	struct auvia_softc *sc;
	const char *intrstr;
	pci_chipset_tag_t pc;
	pcitag_t pt;
	pci_intr_handle_t ih;
	pcireg_t pr;
	int r;
	const char *revnum;	/* VT823xx revision number */
	char intrbuf[PCI_INTRSTR_LEN];

	pa = aux;
	sc = device_private(self);
	sc->sc_dev = self;
	intrstr = NULL;
	pc = pa->pa_pc;
	pt = pa->pa_tag;
	revnum = NULL;

	aprint_naive(": Audio controller\n");

	sc->sc_play.sc_base = AUVIA_PLAY_BASE;
	sc->sc_record.sc_base = AUVIA_RECORD_BASE;
	if (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_VIATECH_VT8233_AC97) {
		sc->sc_flags |= AUVIA_FLAGS_VT8233;
		sc->sc_play.sc_base = VIA8233_MP_BASE;
		sc->sc_record.sc_base = VIA8233_WR_BASE;
	}

	if (pci_mapreg_map(pa, 0x10, PCI_MAPREG_TYPE_IO, 0, &sc->sc_iot,
		&sc->sc_ioh, NULL, &sc->sc_iosize)) {
		aprint_error(": can't map i/o space\n");
		return;
	}

	sc->sc_dmat = pa->pa_dmat;
	sc->sc_pc = pc;
	sc->sc_pt = pt;

	r = PCI_REVISION(pa->pa_class);
	if (sc->sc_flags & AUVIA_FLAGS_VT8233) {
		snprintf(sc->sc_revision, sizeof(sc->sc_revision), "0x%02X", r);
		switch(r) {
		case VIA_REV_8233PRE:
			/* same as 8233, but should not be in the market */
			revnum = "3-Pre";
			break;
		case VIA_REV_8233C:
			/* 2 rec, 4 pb, 1 multi-pb */
			revnum = "3C";
			break;
		case VIA_REV_8233:
			/* 2 rec, 4 pb, 1 multi-pb, spdif */
			revnum = "3";
			break;
		case VIA_REV_8233A:
			/* 1 rec, 1 multi-pb, spdif */
			revnum = "3A";
			break;
		default:
			break;
		}
		if (r >= VIA_REV_8237)
			revnum = "7";
		else if (r >= VIA_REV_8235) /* 2 rec, 4 pb, 1 multi-pb, spdif */
			revnum = "5";
		aprint_normal(": VIA Technologies VT823%s AC'97 Audio "
		    "(rev %s)\n", revnum, sc->sc_revision);
	} else {
		sc->sc_revision[1] = '\0';
		if (r == 0x20) {
			sc->sc_revision[0] = 'H';
		} else if ((r >= 0x10) && (r <= 0x14)) {
			sc->sc_revision[0] = 'A' + (r - 0x10);
		} else {
			snprintf(sc->sc_revision, sizeof(sc->sc_revision),
			    "0x%02X", r);
		}

		aprint_normal(": VIA Technologies VT82C686A AC'97 Audio "
		    "(rev %s)\n", sc->sc_revision);
	}

	if (pci_intr_map(pa, &ih)) {
		aprint_error(": couldn't map interrupt\n");
		bus_space_unmap(sc->sc_iot, sc->sc_ioh, sc->sc_iosize);
		return;
	}
	intrstr = pci_intr_string(pc, ih, intrbuf, sizeof(intrbuf));

	mutex_init(&sc->sc_lock, MUTEX_DEFAULT, IPL_NONE);
	mutex_init(&sc->sc_intr_lock, MUTEX_DEFAULT, IPL_AUDIO);

	sc->sc_ih = pci_intr_establish(pc, ih, IPL_AUDIO, auvia_intr, sc);
	if (sc->sc_ih == NULL) {
		aprint_error_dev(sc->sc_dev, "couldn't establish interrupt");
		if (intrstr != NULL)
			aprint_error(" at %s", intrstr);
		aprint_error("\n");
		bus_space_unmap(sc->sc_iot, sc->sc_ioh, sc->sc_iosize);
		mutex_destroy(&sc->sc_lock);
		mutex_destroy(&sc->sc_intr_lock);
		return;
	}

	aprint_normal_dev(sc->sc_dev, "interrupting at %s\n", intrstr);

	/* disable SBPro compat & others */
	pr = pci_conf_read(pc, pt, AUVIA_PCICONF_JUNK);

	pr &= ~AUVIA_PCICONF_ENABLES; /* clear compat function enables */
	/* XXX what to do about MIDI, FM, joystick? */

	pr |= (AUVIA_PCICONF_ACLINKENAB | AUVIA_PCICONF_ACNOTRST
		| AUVIA_PCICONF_ACVSR | AUVIA_PCICONF_ACSGD);

	pr &= ~(AUVIA_PCICONF_ACFM | AUVIA_PCICONF_ACSB);

	pci_conf_write(pc, pt, AUVIA_PCICONF_JUNK, pr);

	sc->host_if.arg = sc;
	sc->host_if.attach = auvia_attach_codec;
	sc->host_if.read = auvia_read_codec;
	sc->host_if.write = auvia_write_codec;
	sc->host_if.reset = auvia_reset_codec;
	sc->host_if.spdif_event = auvia_spdif_event;

	if ((r = ac97_attach(&sc->host_if, self, &sc->sc_lock)) != 0) {
		aprint_error_dev(sc->sc_dev, "can't attach codec (error 0x%X)\n", r);
		pci_intr_disestablish(pc, sc->sc_ih);
		bus_space_unmap(sc->sc_iot, sc->sc_ioh, sc->sc_iosize);
		mutex_destroy(&sc->sc_lock);
		mutex_destroy(&sc->sc_intr_lock);
		return;
	}

	/* setup audio_format */
	memcpy(sc->sc_formats, auvia_formats, sizeof(auvia_formats));
	mutex_enter(&sc->sc_lock);
	if (sc->sc_play.sc_base != VIA8233_MP_BASE || !AC97_IS_4CH(sc->codec_if)) {
		AUFMT_INVALIDATE(&sc->sc_formats[AUVIA_FORMATS_4CH_8]);
		AUFMT_INVALIDATE(&sc->sc_formats[AUVIA_FORMATS_4CH_16]);
	}
	if (sc->sc_play.sc_base != VIA8233_MP_BASE || !AC97_IS_6CH(sc->codec_if)) {
		AUFMT_INVALIDATE(&sc->sc_formats[AUVIA_FORMATS_6CH_8]);
		AUFMT_INVALIDATE(&sc->sc_formats[AUVIA_FORMATS_6CH_16]);
	}
	if (AC97_IS_FIXED_RATE(sc->codec_if)) {
		for (r = 0; r < AUVIA_NFORMATS; r++) {
			sc->sc_formats[r].frequency_type = 1;
			sc->sc_formats[r].frequency[0] = 48000;
		}
	}
	mutex_exit(&sc->sc_lock);

	if (0 != auconv_create_encodings(sc->sc_formats, AUVIA_NFORMATS,
					 &sc->sc_encodings)) {
		mutex_enter(&sc->sc_lock);
		sc->codec_if->vtbl->detach(sc->codec_if);
		mutex_exit(&sc->sc_lock);
		pci_intr_disestablish(pc, sc->sc_ih);
		bus_space_unmap(sc->sc_iot, sc->sc_ioh, sc->sc_iosize);
		mutex_destroy(&sc->sc_lock);
		mutex_destroy(&sc->sc_intr_lock);
		aprint_error_dev(sc->sc_dev, "can't create encodings\n");
		return;
	}
	if (0 != auconv_create_encodings(auvia_spdif_formats,
	    AUVIA_SPDIF_NFORMATS, &sc->sc_spdif_encodings)) {
		mutex_enter(&sc->sc_lock);
		sc->codec_if->vtbl->detach(sc->codec_if);
		mutex_exit(&sc->sc_lock);
		pci_intr_disestablish(pc, sc->sc_ih);
		bus_space_unmap(sc->sc_iot, sc->sc_ioh, sc->sc_iosize);
		mutex_destroy(&sc->sc_lock);
		mutex_destroy(&sc->sc_intr_lock);
		aprint_error_dev(sc->sc_dev, "can't create spdif encodings\n");
		return;
	}

	if (!pmf_device_register(self, NULL, auvia_resume))
		aprint_error_dev(self, "couldn't establish power handler\n");

	audio_attach_mi(&auvia_hw_if, sc, sc->sc_dev);
	mutex_enter(&sc->sc_lock);
	sc->codec_if->vtbl->unlock(sc->codec_if);
	mutex_exit(&sc->sc_lock);
	return;
}

static int
auvia_attach_codec(void *addr, struct ac97_codec_if *cif)
{
	struct auvia_softc *sc;

	sc = addr;
	sc->codec_if = cif;
	return 0;
}

static int
auvia_reset_codec(void *addr)
{
	struct auvia_softc *sc;
	pcireg_t r;
	int i;

	/* perform a codec cold reset */
	sc = addr;
	r = pci_conf_read(sc->sc_pc, sc->sc_pt, AUVIA_PCICONF_JUNK);

	r &= ~AUVIA_PCICONF_ACNOTRST;	/* enable RESET (active low) */
	pci_conf_write(sc->sc_pc, sc->sc_pt, AUVIA_PCICONF_JUNK, r);
	delay(2);

	r |= AUVIA_PCICONF_ACNOTRST;	/* disable RESET (inactive high) */
	pci_conf_write(sc->sc_pc, sc->sc_pt, AUVIA_PCICONF_JUNK, r);
	delay(200);

	for (i = 500000; i != 0 && !(pci_conf_read(sc->sc_pc, sc->sc_pt,
		AUVIA_PCICONF_JUNK) & AUVIA_PCICONF_PRIVALID); i--)
		DELAY(1);
	if (i == 0) {
		printf("%s: codec reset timed out\n", device_xname(sc->sc_dev));
		return ETIMEDOUT;
	}
	return 0;
}

static int
auvia_waitready_codec(struct auvia_softc *sc)
{
	int i;

	/* poll until codec not busy */
	for (i = 0; (i < TIMEOUT) && (bus_space_read_4(sc->sc_iot, sc->sc_ioh,
		AUVIA_CODEC_CTL) & AUVIA_CODEC_BUSY); i++)
		delay(1);
	if (i >= TIMEOUT) {
		printf("%s: codec busy\n", device_xname(sc->sc_dev));
		return 1;
	}

	return 0;
}

static int
auvia_waitvalid_codec(struct auvia_softc *sc)
{
	int i;

	/* poll until codec valid */
	for (i = 0; (i < TIMEOUT) && !(bus_space_read_4(sc->sc_iot, sc->sc_ioh,
		AUVIA_CODEC_CTL) & AUVIA_CODEC_PRIVALID); i++)
			delay(1);
	if (i >= TIMEOUT) {
		printf("%s: codec invalid\n", device_xname(sc->sc_dev));
		return 1;
	}

	return 0;
}

static int
auvia_write_codec(void *addr, u_int8_t reg, u_int16_t val)
{
	struct auvia_softc *sc;

	sc = addr;
	if (auvia_waitready_codec(sc))
		return 1;

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, AUVIA_CODEC_CTL,
		AUVIA_CODEC_PRIVALID | AUVIA_CODEC_INDEX(reg) | val);

	return 0;
}

static int
auvia_read_codec(void *addr, u_int8_t reg, u_int16_t *val)
{
	struct auvia_softc *sc;

	sc = addr;
	if (auvia_waitready_codec(sc))
		return 1;

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, AUVIA_CODEC_CTL,
		AUVIA_CODEC_PRIVALID | AUVIA_CODEC_READ | AUVIA_CODEC_INDEX(reg));

	if (auvia_waitready_codec(sc))
		return 1;

	if (auvia_waitvalid_codec(sc))
		return 1;

	*val = bus_space_read_2(sc->sc_iot, sc->sc_ioh, AUVIA_CODEC_CTL);

	return 0;
}

static void
auvia_spdif_event(void *addr, bool flag)
{
	struct auvia_softc *sc;

	sc = addr;
	sc->sc_spdif = flag;
}

static int
auvia_open(void *addr, int flags)
{
	struct auvia_softc *sc;

	sc = (struct auvia_softc *)addr;
	mutex_spin_exit(&sc->sc_intr_lock);
	sc->codec_if->vtbl->lock(sc->codec_if);
	mutex_spin_enter(&sc->sc_intr_lock);
	return 0;
}

static void
auvia_close(void *addr)
{
	struct auvia_softc *sc;

	sc = (struct auvia_softc *)addr;
	mutex_spin_exit(&sc->sc_intr_lock);
	sc->codec_if->vtbl->unlock(sc->codec_if);
	mutex_spin_enter(&sc->sc_intr_lock);
}

static int
auvia_query_encoding(void *addr, struct audio_encoding *fp)
{
	struct auvia_softc *sc;

	sc = (struct auvia_softc *)addr;
	return auconv_query_encoding(
	    sc->sc_spdif ? sc->sc_spdif_encodings : sc->sc_encodings, fp);
}

static void
auvia_set_params_sub(struct auvia_softc *sc, struct auvia_softc_chan *ch,
		     const audio_params_t *p)
{
	uint32_t v;
	uint16_t regval;

	if (!(sc->sc_flags & AUVIA_FLAGS_VT8233)) {
		regval = (p->channels == 2 ? AUVIA_RPMODE_STEREO : 0)
			| (p->precision  == 16 ?
				AUVIA_RPMODE_16BIT : 0)
			| AUVIA_RPMODE_INTR_FLAG | AUVIA_RPMODE_INTR_EOL
			| AUVIA_RPMODE_AUTOSTART;
		ch->sc_reg = regval;
	} else if (ch->sc_base != VIA8233_MP_BASE) {
		v = CH_READ4(sc, ch, VIA8233_RP_RATEFMT);
		v &= ~(VIA8233_RATEFMT_48K | VIA8233_RATEFMT_STEREO
			| VIA8233_RATEFMT_16BIT);

		v |= VIA8233_RATEFMT_48K * (p->sample_rate / 20)
			/ (48000 / 20);
		if (p->channels == 2)
			v |= VIA8233_RATEFMT_STEREO;
		if (p->precision == 16)
			v |= VIA8233_RATEFMT_16BIT;

		CH_WRITE4(sc, ch, VIA8233_RP_RATEFMT, v);
	} else {
		static const u_int32_t slottab[7] =
			{ 0, 0xff000011, 0xff000021, 0,
			  0xff004321, 0, 0xff436521};

		regval = (p->precision == 16
			? VIA8233_MP_FORMAT_16BIT : VIA8233_MP_FORMAT_8BIT)
			| (p->channels << 4);
		CH_WRITE1(sc, ch, VIA8233_OFF_MP_FORMAT, regval);
		CH_WRITE4(sc, ch, VIA8233_OFF_MP_STOP, slottab[p->channels]);
	}
}

static int
auvia_set_params(void *addr, int setmode, int usemode,
    audio_params_t *play, audio_params_t *rec, stream_filter_list_t *pfil,
    stream_filter_list_t *rfil)
{
	struct auvia_softc *sc;
	struct auvia_softc_chan *ch;
	struct audio_params *p;
	struct ac97_codec_if* codec;
	stream_filter_list_t *fil;
	int reg, mode;
	int index;

	sc = addr;
	codec = sc->codec_if;
	/* for mode in (RECORD, PLAY) */
	for (mode = AUMODE_RECORD; mode != -1;
	     mode = mode == AUMODE_RECORD ? AUMODE_PLAY : -1) {
		if ((setmode & mode) == 0)
			continue;

		if (mode == AUMODE_PLAY ) {
			p = play;
			ch = &sc->sc_play;
			reg = AC97_REG_PCM_FRONT_DAC_RATE;
			fil = pfil;
		} else {
			p = rec;
			ch = &sc->sc_record;
			reg = AC97_REG_PCM_LR_ADC_RATE;
			fil = rfil;
		}

		if (p->sample_rate < 4000 || p->sample_rate > 48000 ||
		    (p->precision != 8 && p->precision != 16))
			return (EINVAL);
		if (sc->sc_spdif)
			index = auconv_set_converter(auvia_spdif_formats,
			    AUVIA_SPDIF_NFORMATS, mode, p, TRUE, fil);
		else
			index = auconv_set_converter(sc->sc_formats,
			    AUVIA_NFORMATS, mode, p, TRUE, fil);
		if (index < 0)
			return EINVAL;
		if (fil->req_size > 0)
			p = &fil->filters[0].param;
		if (!AC97_IS_FIXED_RATE(codec)) {
			if (codec->vtbl->set_rate(codec, reg, &p->sample_rate))
				return EINVAL;
			reg = AC97_REG_PCM_SURR_DAC_RATE;
			if (p->channels >= 4
			    && codec->vtbl->set_rate(codec, reg,
						     &p->sample_rate))
				return EINVAL;
			reg = AC97_REG_PCM_LFE_DAC_RATE;
			if (p->channels == 6
			    && codec->vtbl->set_rate(codec, reg,
						     &p->sample_rate))
				return EINVAL;
		}
		auvia_set_params_sub(sc, ch, p);
	}

	return 0;
}

static int
auvia_round_blocksize(void *addr, int blk,
    int mode, const audio_params_t *param)
{
	struct auvia_softc *sc;

	sc = addr;

	/* XXX VT823x might have the limitation of dma_ops size */
	if (sc->sc_flags & AUVIA_FLAGS_VT8233 && blk < 288)
		blk = 288;

	/* Avoid too many dma_ops. */
	return min((blk & -32), AUVIA_MINBLKSZ);
}

static int
auvia_halt_output(void *addr)
{
	struct auvia_softc *sc;
	struct auvia_softc_chan *ch;

	sc = addr;
	ch = &(sc->sc_play);
	CH_WRITE1(sc, ch, AUVIA_RP_CONTROL, AUVIA_RPCTRL_TERMINATE);
	ch->sc_intr = NULL;
	return 0;
}

static int
auvia_halt_input(void *addr)
{
	struct auvia_softc *sc;
	struct auvia_softc_chan *ch;

	sc = addr;
	ch = &(sc->sc_record);
	CH_WRITE1(sc, ch, AUVIA_RP_CONTROL, AUVIA_RPCTRL_TERMINATE);
	ch->sc_intr = NULL;
	return 0;
}

static int
auvia_getdev(void *addr, struct audio_device *retp)
{
	struct auvia_softc *sc;

	if (retp) {
		sc = addr;
		if (sc->sc_flags & AUVIA_FLAGS_VT8233) {
			strncpy(retp->name, "VIA VT823x",
				sizeof(retp->name));
		} else {
			strncpy(retp->name, "VIA VT82C686A",
				sizeof(retp->name));
		}
		strncpy(retp->version, sc->sc_revision, sizeof(retp->version));
		strncpy(retp->config, "auvia", sizeof(retp->config));
	}

	return 0;
}

static int
auvia_set_port(void *addr, mixer_ctrl_t *cp)
{
	struct auvia_softc *sc;

	sc = addr;
	return sc->codec_if->vtbl->mixer_set_port(sc->codec_if, cp);
}

static int
auvia_get_port(void *addr, mixer_ctrl_t *cp)
{
	struct auvia_softc *sc;

	sc = addr;
	return sc->codec_if->vtbl->mixer_get_port(sc->codec_if, cp);
}

static int
auvia_query_devinfo(void *addr, mixer_devinfo_t *dip)
{
	struct auvia_softc *sc;

	sc = addr;
	return sc->codec_if->vtbl->query_devinfo(sc->codec_if, dip);
}

static int
auvia_malloc_channel(struct auvia_softc *sc, struct auvia_softc_chan *ch,
		     size_t size)
{
	struct auvia_dma *dp;
	int segs;

	/* if old list is large enough, nothing to do */
	segs = (size + AUVIA_MINBLKSZ - 1) / AUVIA_MINBLKSZ;
	if (segs <= ch->sc_dma_op_count) {
		return 0;
	}

	if (ch->sc_dma_ops) {
		auvia_free(sc, ch->sc_dma_ops,
		    ch->sc_dma_op_count * sizeof(*dp));
	}

	ch->sc_dma_ops = auvia_malloc_dmamem(sc, 0,
	    sizeof(struct auvia_dma_op) * segs);

	if (ch->sc_dma_ops == NULL) {
		aprint_error_dev(sc->sc_dev, "couldn't build dmaops\n");
		return ENOMEM;
	}

	for (dp = sc->sc_dmas;
	    dp && dp->addr != (void *)(ch->sc_dma_ops);
	    dp = dp->next)
		continue;

	if (!dp)
		panic("%s: build_dma_ops: where'd my memory go??? "
			"address (%p)\n", device_xname(sc->sc_dev),
			ch->sc_dma_ops);

	ch->sc_dma_op_count = segs;
	ch->sc_dma_ops_dma = dp;

	return 0;
}

static void *
auvia_malloc_dmamem(void *addr, int direction, size_t size)
{
	struct auvia_softc *sc;
	struct auvia_dma *p;
	int error;
	int rseg;

	p = kmem_alloc(sizeof(*p), KM_SLEEP);
	if (p == NULL)
		return NULL;
	sc = addr;
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

static void *
auvia_malloc(void *addr, int direction, size_t size)
{
	struct auvia_softc *sc;
	void *p;

	sc = addr;

	p = auvia_malloc_dmamem(addr, direction, size);
	if (p == NULL) {
		return NULL;
	}
	if (auvia_malloc_channel(sc, &sc->sc_play, size) != 0) {
		auvia_free(addr, p, size);
		return NULL;
	}
	if (auvia_malloc_channel(sc, &sc->sc_record, size) != 0) {
		auvia_free(addr, p, size);
		return NULL;
	}
	return p;
}

static void
auvia_free(void *addr, void *ptr, size_t size)
{
	struct auvia_softc *sc;
	struct auvia_dma **pp, *p;

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

	panic("auvia_free: trying to free unallocated memory");
}

static size_t
auvia_round_buffersize(void *addr, int direction, size_t size)
{

	return size;
}

static paddr_t
auvia_mappage(void *addr, void *mem, off_t off, int prot)
{
	struct auvia_softc *sc;
	struct auvia_dma *p;

	if (off < 0)
		return -1;
	sc = addr;
	for (p = sc->sc_dmas; p && p->addr != mem; p = p->next)
		continue;

	if (!p)
		return -1;

	return bus_dmamem_mmap(sc->sc_dmat, &p->seg, 1, off, prot,
	    BUS_DMA_WAITOK);
}

static int
auvia_get_props(void *addr)
{
	struct auvia_softc *sc;
	int props;

	props = AUDIO_PROP_INDEPENDENT | AUDIO_PROP_FULLDUPLEX;
	sc = addr;
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
auvia_get_locks(void *addr, kmutex_t **intr, kmutex_t **thread)
{
	struct auvia_softc *sc;

	sc = addr;
	*intr = &sc->sc_intr_lock;
	*thread = &sc->sc_lock;
}

static int
auvia_build_dma_ops(struct auvia_softc *sc, struct auvia_softc_chan *ch,
	struct auvia_dma *p, void *start, void *end, int blksize)
{
	struct auvia_dma_op *op;
	bus_addr_t s;
	size_t l;

	op = ch->sc_dma_ops;
	s = p->map->dm_segs[0].ds_addr;
	l = ((char *)end - (char *)start);

	while (l) {
		op->ptr = htole32(s);
		l = l - blksize;
		if (!l) {
			/* if last block */
			op->flags = htole32(AUVIA_DMAOP_EOL | blksize);
		} else {
			op->flags = htole32(AUVIA_DMAOP_FLAG | blksize);
		}
		s += blksize;
		op++;
	}

	return 0;
}


static int
auvia_trigger_output(void *addr, void *start, void *end, int blksize,
    void (*intr)(void *), void *arg, const audio_params_t *param)
{
	struct auvia_softc *sc;
	struct auvia_softc_chan *ch;
	struct auvia_dma *p;

	sc = addr;
	ch = &(sc->sc_play);
	for (p = sc->sc_dmas; p && p->addr != start; p = p->next)
		continue;

	if (!p)
		panic("auvia_trigger_output: request with bad start "
			"address (%p)", start);

	if (auvia_build_dma_ops(sc, ch, p, start, end, blksize)) {
		return 1;
	}

	ch->sc_intr = intr;
	ch->sc_arg = arg;

	CH_WRITE4(sc, ch, AUVIA_RP_DMAOPS_BASE,
		ch->sc_dma_ops_dma->map->dm_segs[0].ds_addr);

	if (sc->sc_flags & AUVIA_FLAGS_VT8233) {
		if (ch->sc_base != VIA8233_MP_BASE) {
			CH_WRITE1(sc, ch, VIA8233_RP_DXS_LVOL, 0);
			CH_WRITE1(sc, ch, VIA8233_RP_DXS_RVOL, 0);
		}
		CH_WRITE1(sc, ch, AUVIA_RP_CONTROL,
			AUVIA_RPCTRL_START | AUVIA_RPCTRL_AUTOSTART |
			AUVIA_RPCTRL_STOP  | AUVIA_RPCTRL_EOL | AUVIA_RPCTRL_FLAG);
	} else {
		CH_WRITE1(sc, ch, AUVIA_RP_MODE, ch->sc_reg);
		CH_WRITE1(sc, ch, AUVIA_RP_CONTROL, AUVIA_RPCTRL_START);
	}

	return 0;
}

static int
auvia_trigger_input(void *addr, void *start, void *end, int blksize,
    void (*intr)(void *), void *arg, const audio_params_t *param)
{
	struct auvia_softc *sc;
	struct auvia_softc_chan *ch;
	struct auvia_dma *p;

	sc = addr;
	ch = &(sc->sc_record);
	for (p = sc->sc_dmas; p && p->addr != start; p = p->next)
		continue;

	if (!p)
		panic("auvia_trigger_input: request with bad start "
			"address (%p)", start);

	if (auvia_build_dma_ops(sc, ch, p, start, end, blksize)) {
		return 1;
	}

	ch->sc_intr = intr;
	ch->sc_arg = arg;

	CH_WRITE4(sc, ch, AUVIA_RP_DMAOPS_BASE,
		  ch->sc_dma_ops_dma->map->dm_segs[0].ds_addr);

	if (sc->sc_flags & AUVIA_FLAGS_VT8233) {
		CH_WRITE1(sc, ch, VIA8233_RP_DXS_LVOL, 0);
		CH_WRITE1(sc, ch, VIA8233_RP_DXS_RVOL, 0);
		CH_WRITE1(sc, ch, AUVIA_RP_CONTROL,
			AUVIA_RPCTRL_START | AUVIA_RPCTRL_AUTOSTART |
			AUVIA_RPCTRL_STOP  | AUVIA_RPCTRL_EOL | AUVIA_RPCTRL_FLAG);
	} else {
		CH_WRITE1(sc, ch, AUVIA_RP_MODE, ch->sc_reg);
		CH_WRITE1(sc, ch, AUVIA_RP_CONTROL, AUVIA_RPCTRL_START);
	}

	return 0;
}

static int
auvia_intr(void *arg)
{
	struct auvia_softc *sc;
	struct auvia_softc_chan *ch;
	u_int8_t r;
	int rval;

	sc = arg;
	rval = 0;
	ch = &sc->sc_record;

	mutex_spin_enter(&sc->sc_intr_lock);
	r = CH_READ1(sc, ch, AUVIA_RP_STAT);
	if (r & AUVIA_RPSTAT_INTR) {
		if (sc->sc_record.sc_intr)
			sc->sc_record.sc_intr(sc->sc_record.sc_arg);

		/* clear interrupts */
		CH_WRITE1(sc, ch, AUVIA_RP_STAT, AUVIA_RPSTAT_INTR);
		rval = 1;
	}

	ch = &sc->sc_play;
	r = CH_READ1(sc, ch, AUVIA_RP_STAT);
	if (r & AUVIA_RPSTAT_INTR) {
		if (sc->sc_play.sc_intr)
			sc->sc_play.sc_intr(sc->sc_play.sc_arg);

		/* clear interrupts */
		CH_WRITE1(sc, ch, AUVIA_RP_STAT, AUVIA_RPSTAT_INTR);
		rval = 1;
	}
	mutex_spin_exit(&sc->sc_intr_lock);

	return rval;
}

static bool
auvia_resume(device_t dv, const pmf_qual_t *qual)
{
	struct auvia_softc *sc = device_private(dv);

	mutex_enter(&sc->sc_lock);
	mutex_spin_enter(&sc->sc_intr_lock);
	auvia_reset_codec(sc);
	DELAY(1000);
	mutex_spin_exit(&sc->sc_intr_lock);
	(sc->codec_if->vtbl->restore_ports)(sc->codec_if);
	mutex_exit(&sc->sc_lock);

	return true;
}
