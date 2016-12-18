/*	$NetBSD: azalia.c,v 1.83 2014/11/09 19:57:53 nonaka Exp $	*/

/*-
 * Copyright (c) 2005, 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by TAMURA Kent
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
 * High Definition Audio Specification
 *	ftp://download.intel.com/standards/hdaudio/pdf/HDAudio_03.pdf
 *
 *
 * TO DO:
 *  - power hook
 *  - multiple codecs (needed?)
 *  - multiple streams (needed?)
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: azalia.c,v 1.83 2014/11/09 19:57:53 nonaka Exp $");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/fcntl.h>
#include <sys/kmem.h>
#include <sys/systm.h>
#include <sys/module.h>

#include <dev/audio_if.h>
#include <dev/auconv.h>

#include <dev/pci/pcidevs.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/azalia.h>

/* ----------------------------------------------------------------
 * ICH6/ICH7 constant values
 * ---------------------------------------------------------------- */

/* PCI registers */
#define ICH_PCI_HDBARL	0x10
#define ICH_PCI_HDBARU	0x14
#define ICH_PCI_HDCTL	0x40
#define		ICH_PCI_HDCTL_CLKDETCLR		0x08
#define		ICH_PCI_HDCTL_CLKDETEN		0x04
#define		ICH_PCI_HDCTL_CLKDETINV		0x02
#define		ICH_PCI_HDCTL_SIGNALMODE	0x01

/* internal types */

typedef struct {
	bus_dmamap_t map;
	void *addr;		/* kernel virtual address */
	bus_dma_segment_t segments[1];
	size_t size;
} azalia_dma_t;

#define AZALIA_DMA_DMAADDR(p)	((p)->map->dm_segs[0].ds_addr)

typedef struct {
	struct azalia_t *az;
	int regbase;
	int number;
	int dir;		/* AUMODE_PLAY or AUMODE_RECORD */
	uint32_t intr_bit;
	azalia_dma_t bdlist;
	azalia_dma_t buffer;
	void (*intr)(void*);
	void *intr_arg;
	bus_addr_t dmaend, dmanext; /* XXX needed? */
} stream_t;

/* XXXfreza use bus_space_subregion() instead of adding 'regbase' offset */
#define STR_READ_1(s, r)	\
	bus_space_read_1((s)->az->iot, (s)->az->ioh, (s)->regbase + HDA_SD_##r)
#define STR_READ_2(s, r)	\
	bus_space_read_2((s)->az->iot, (s)->az->ioh, (s)->regbase + HDA_SD_##r)
#define STR_READ_4(s, r)	\
	bus_space_read_4((s)->az->iot, (s)->az->ioh, (s)->regbase + HDA_SD_##r)
#define STR_WRITE_1(s, r, v)	\
	bus_space_write_1((s)->az->iot, (s)->az->ioh, (s)->regbase + HDA_SD_##r, v)
#define STR_WRITE_2(s, r, v)	\
	bus_space_write_2((s)->az->iot, (s)->az->ioh, (s)->regbase + HDA_SD_##r, v)
#define STR_WRITE_4(s, r, v)	\
	bus_space_write_4((s)->az->iot, (s)->az->ioh, (s)->regbase + HDA_SD_##r, v)

typedef struct azalia_t {
	device_t dev;
	device_t audiodev;
	kmutex_t lock;
	kmutex_t intr_lock;

	pci_chipset_tag_t pc;
	pcitag_t tag;
	void *ih;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	bus_size_t map_size;
	bus_dma_tag_t dmat;
	pcireg_t pciid;
	uint32_t subid;

	codec_t codecs[15];
	int ncodecs;		/* number of codecs */
	int codecno;		/* index of the using codec */

	azalia_dma_t corb_dma;
	int corb_size;
	azalia_dma_t rirb_dma;
	int rirb_size;
	int rirb_rp;
#define UNSOLQ_SIZE	256
	rirb_entry_t *unsolq;
	int unsolq_wp;
	int unsolq_rp;
	bool unsolq_kick;

	bool ok64;
	int nistreams, nostreams, nbstreams;
	stream_t pstream;
	stream_t rstream;

	int mode_cap;
} azalia_t;

#define AZ_READ_1(z, r)		bus_space_read_1((z)->iot, (z)->ioh, HDA_##r)
#define AZ_READ_2(z, r)		bus_space_read_2((z)->iot, (z)->ioh, HDA_##r)
#define AZ_READ_4(z, r)		bus_space_read_4((z)->iot, (z)->ioh, HDA_##r)
#define AZ_WRITE_1(z, r, v)	bus_space_write_1((z)->iot, (z)->ioh, HDA_##r, v)
#define AZ_WRITE_2(z, r, v)	bus_space_write_2((z)->iot, (z)->ioh, HDA_##r, v)
#define AZ_WRITE_4(z, r, v)	bus_space_write_4((z)->iot, (z)->ioh, HDA_##r, v)


/* prototypes */
static int	azalia_pci_match(device_t, cfdata_t, void *);
static void	azalia_pci_attach(device_t, device_t, void *);
static int	azalia_pci_detach(device_t, int);
static bool	azalia_pci_resume(device_t, const pmf_qual_t *);
static void	azalia_childdet(device_t, device_t);
static int	azalia_intr(void *);
static int	azalia_attach(azalia_t *);
static void	azalia_attach_intr(device_t);
static int	azalia_init_corb(azalia_t *, int);
static int	azalia_delete_corb(azalia_t *);
static int	azalia_init_rirb(azalia_t *, int);
static int	azalia_delete_rirb(azalia_t *);
static int	azalia_set_command(const azalia_t *, nid_t, int, uint32_t,
	uint32_t);
static int	azalia_get_response(azalia_t *, uint32_t *);
static void	azalia_rirb_kick_unsol_events(azalia_t *);
static void	azalia_rirb_intr(azalia_t *);
static int	azalia_alloc_dmamem(azalia_t *, size_t, size_t, azalia_dma_t *);
static int	azalia_free_dmamem(const azalia_t *, azalia_dma_t*);

static int	azalia_codec_init(codec_t *, int, uint32_t);
static int	azalia_codec_delete(codec_t *);
static void	azalia_codec_add_bits(codec_t *, int, uint32_t, int);
static void	azalia_codec_add_format(codec_t *, int, int, int, uint32_t,
	int32_t);
static int	azalia_codec_comresp(const codec_t *, nid_t, uint32_t,
	uint32_t, uint32_t *);
static int	azalia_codec_connect_stream(codec_t *, int, uint16_t, int);
static int	azalia_codec_disconnect_stream(codec_t *, int);

static int	azalia_widget_init(widget_t *, const codec_t *, int, const char *);
static int	azalia_widget_init_audio(widget_t *, const codec_t *, const char *);
#ifdef AZALIA_DEBUG
static int	azalia_widget_print_audio(const widget_t *, const char *, int);
#endif
static int	azalia_widget_init_pin(widget_t *, const codec_t *);
static int	azalia_widget_print_pin(const widget_t *, const char *);
static int	azalia_widget_init_connection(widget_t *, const codec_t *, const char *);

static int	azalia_stream_init(stream_t *, azalia_t *, int, int, int);
static int	azalia_stream_delete(stream_t *, azalia_t *);
static int	azalia_stream_reset(stream_t *);
static int	azalia_stream_start(stream_t *, void *, void *, int,
	void (*)(void *), void *, uint16_t);
static int	azalia_stream_halt(stream_t *);
static int	azalia_stream_intr(stream_t *, uint32_t);

static int	azalia_open(void *, int);
static void	azalia_close(void *);
static int	azalia_query_encoding(void *, audio_encoding_t *);
static int	azalia_set_params(void *, int, int, audio_params_t *,
	audio_params_t *, stream_filter_list_t *, stream_filter_list_t *);
static int	azalia_round_blocksize(void *, int, int, const audio_params_t *);
static int	azalia_halt_output(void *);
static int	azalia_halt_input(void *);
static int	azalia_getdev(void *, struct audio_device *);
static int	azalia_set_port(void *, mixer_ctrl_t *);
static int	azalia_get_port(void *, mixer_ctrl_t *);
static int	azalia_query_devinfo(void *, mixer_devinfo_t *);
static void	*azalia_allocm(void *, int, size_t);
static void	azalia_freem(void *, void *, size_t);
static size_t	azalia_round_buffersize(void *, int, size_t);
static int	azalia_get_props(void *);
static int	azalia_trigger_output(void *, void *, void *, int,
	void (*)(void *), void *, const audio_params_t *);
static int	azalia_trigger_input(void *, void *, void *, int,
	void (*)(void *), void *, const audio_params_t *);
static void	azalia_get_locks(void *, kmutex_t **, kmutex_t **);

static int	azalia_params2fmt(const audio_params_t *, uint16_t *);

/* variables */
CFATTACH_DECL2_NEW(azalia, sizeof(azalia_t),
    azalia_pci_match, azalia_pci_attach, azalia_pci_detach, NULL,
    NULL, azalia_childdet);

static const struct audio_hw_if azalia_hw_if = {
	azalia_open,
	azalia_close,
	NULL,			/* drain */
	azalia_query_encoding,
	azalia_set_params,
	azalia_round_blocksize,
	NULL,			/* commit_settings */
	NULL,			/* init_output */
	NULL,			/* init_input */
	NULL,			/* start_output */
	NULL,			/* satart_inpu */
	azalia_halt_output,
	azalia_halt_input,
	NULL,			/* speaker_ctl */
	azalia_getdev,
	NULL,			/* setfd */
	azalia_set_port,
	azalia_get_port,
	azalia_query_devinfo,
	azalia_allocm,
	azalia_freem,
	azalia_round_buffersize,
	NULL,			/* mappage */
	azalia_get_props,
	azalia_trigger_output,
	azalia_trigger_input,
	NULL,			/* dev_ioctl */
	azalia_get_locks,
};

static const char *pin_colors[16] = {
	"unknown", "black", "gray", "blue",
	"green", "red", "orange", "yellow",
	"purple", "pink", "col0a", "col0b",
	"col0c", "col0d", "white", "other"
};

#ifdef AZALIA_DEBUG
static const char *pin_devices[16] = {
	"line-out", AudioNspeaker, AudioNheadphone, AudioNcd,
	"SPDIF-out", "digital-out", "modem-line", "modem-handset",
	"line-in", AudioNaux, AudioNmicrophone, "telephony",
	"SPDIF-in", "digital-in", "dev0e", "other"
};
#endif

/* ================================================================
 * PCI functions
 * ================================================================ */

static int
azalia_pci_match(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa;

	pa = aux;
	if (PCI_CLASS(pa->pa_class) == PCI_CLASS_MULTIMEDIA
	    && PCI_SUBCLASS(pa->pa_class) == PCI_SUBCLASS_MULTIMEDIA_HDAUDIO)
		return 1;
	return 0;
}

static void
azalia_pci_attach(device_t parent, device_t self, void *aux)
{
	azalia_t *sc = device_private(self);
	struct pci_attach_args *pa = aux;
	pcireg_t v;
	pci_intr_handle_t ih;
	const char *intrrupt_str;
	char vendor[PCI_VENDORSTR_LEN];
	char product[PCI_PRODUCTSTR_LEN];
	char intrbuf[PCI_INTRSTR_LEN];

	sc->dev = self;
	sc->dmat = pa->pa_dmat;

	aprint_normal(": Generic High Definition Audio Controller\n");

	v = pci_conf_read(pa->pa_pc, pa->pa_tag, ICH_PCI_HDBARL);
	v &= PCI_MAPREG_TYPE_MASK | PCI_MAPREG_MEM_TYPE_MASK;
	if (pci_mapreg_map(pa, ICH_PCI_HDBARL, v, 0,
			   &sc->iot, &sc->ioh, NULL, &sc->map_size)) {
		aprint_error_dev(self, "can't map device i/o space\n");
		return;
	}

	/* enable bus mastering */
	v = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG,
	    v | PCI_COMMAND_MASTER_ENABLE | PCI_COMMAND_BACKTOBACK_ENABLE);

	/* interrupt */
	if (pci_intr_map(pa, &ih)) {
		aprint_error_dev(self, "can't map interrupt\n");
		return;
	}

	mutex_init(&sc->lock, MUTEX_DEFAULT, IPL_NONE);
	mutex_init(&sc->intr_lock, MUTEX_DEFAULT, IPL_AUDIO);

	sc->pc = pa->pa_pc;
	sc->tag = pa->pa_tag;
	intrrupt_str = pci_intr_string(pa->pa_pc, ih, intrbuf, sizeof(intrbuf));
	sc->ih = pci_intr_establish(pa->pa_pc, ih, IPL_AUDIO, azalia_intr, sc);
	if (sc->ih == NULL) {
		aprint_error_dev(self, "can't establish interrupt");
		if (intrrupt_str != NULL)
			aprint_error(" at %s", intrrupt_str);
		aprint_error("\n");
		mutex_destroy(&sc->lock);
		mutex_destroy(&sc->intr_lock);
		return;
	}
	aprint_normal_dev(self, "interrupting at %s\n", intrrupt_str);

	if (!pmf_device_register(self, NULL, azalia_pci_resume))
		aprint_error_dev(self, "couldn't establish power handler\n");

	sc->pciid = pa->pa_id;
	pci_findvendor(vendor, sizeof(vendor), PCI_VENDOR(pa->pa_id));
	pci_findproduct(product, sizeof(product), PCI_VENDOR(pa->pa_id),
	    PCI_PRODUCT(pa->pa_id));
	aprint_normal_dev(self, "host: %s %s (rev. %d)",
	    vendor, product, PCI_REVISION(pa->pa_class));

	if (azalia_attach(sc)) {
		aprint_error_dev(self, "initialization failure\n");
		azalia_pci_detach(self, 0);
		mutex_destroy(&sc->lock);
		mutex_destroy(&sc->intr_lock);
		return;
	}
	sc->subid = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_SUBSYS_ID_REG);

	config_interrupts(self, azalia_attach_intr);
}

static void
azalia_childdet(device_t self, device_t child)
{
	azalia_t *az = device_private(self);

	KASSERT(az->audiodev == child);
	az->audiodev = NULL;
}

static int
azalia_pci_detach(device_t self, int flags)
{
	azalia_t *az;
	int i;

	DPRINTF(("%s\n", __func__));
	az = device_private(self);
	if (az->audiodev != NULL)
		config_detach(az->audiodev, flags);

	mutex_enter(&az->lock);

#if notyet
	DPRINTF(("%s: halt streams\n", __func__));
	azalia_stream_halt(&az->rstream);
	azalia_stream_halt(&az->pstream);
#endif


	DPRINTF(("%s: delete streams\n", __func__));
	azalia_stream_delete(&az->rstream, az);
	azalia_stream_delete(&az->pstream, az);

	DPRINTF(("%s: delete codecs\n", __func__));
	for (i = 0; i < az->ncodecs; i++) {
		azalia_codec_delete(&az->codecs[i]);
	}
	az->ncodecs = 0;

	DPRINTF(("%s: delete CORB and RIRB\n", __func__));
	azalia_delete_corb(az);
	azalia_delete_rirb(az);

	mutex_exit(&az->lock);
	mutex_destroy(&az->lock);
	mutex_destroy(&az->intr_lock);

	DPRINTF(("%s: delete PCI resources\n", __func__));
	if (az->ih != NULL) {
		pci_intr_disestablish(az->pc, az->ih);
		az->ih = NULL;
	}
	if (az->map_size != 0) {
		bus_space_unmap(az->iot, az->ioh, az->map_size);
		az->map_size = 0;
	}
	return 0;
}

static bool
azalia_pci_resume(device_t dv, const pmf_qual_t *qual)
{
	azalia_t *az = device_private(dv);

	mutex_enter(&az->lock);
	mutex_spin_enter(&az->intr_lock);
	azalia_attach(az);
	azalia_attach_intr(az->dev);
	mutex_spin_exit(&az->intr_lock);
	mutex_exit(&az->lock);

	return true;
}

static int
azalia_intr(void *v)
{
	azalia_t *az;
	int ret;
	uint32_t intsts;
	uint8_t rirbsts;

	az = v;

	if (!device_has_power(az->dev))
		return 0;

	mutex_spin_enter(&az->intr_lock);

	intsts = AZ_READ_4(az, INTSTS);
	if (intsts == 0) {
		mutex_spin_exit(&az->intr_lock);
		return 0;
	}

	ret = azalia_stream_intr(&az->pstream, intsts) +
	      azalia_stream_intr(&az->rstream, intsts);

	rirbsts = AZ_READ_1(az, RIRBSTS);
	if (rirbsts & (HDA_RIRBSTS_RIRBOIS | HDA_RIRBSTS_RINTFL)) {
		if (rirbsts & HDA_RIRBSTS_RINTFL) {
			azalia_rirb_intr(az);
		} else {
			/*printf("[Overflow!]");*/
		}
		AZ_WRITE_1(az, RIRBSTS,
		    rirbsts | HDA_RIRBSTS_RIRBOIS | HDA_RIRBSTS_RINTFL);
		ret++;
	}

	mutex_spin_exit(&az->intr_lock);
	return ret;
}

/* ================================================================
 * HDA controller functions
 * ================================================================ */

static int
azalia_attach(azalia_t *az)
{
	int i, n;
	uint32_t gctl;
	uint16_t gcap;
	uint16_t statests;

	if (az->audiodev == NULL)
		aprint_normal(", HDA rev. %d.%d\n",
		    AZ_READ_1(az, VMAJ), AZ_READ_1(az, VMIN));

	gcap = AZ_READ_2(az, GCAP);
	az->nistreams = HDA_GCAP_ISS(gcap);
	az->nostreams = HDA_GCAP_OSS(gcap);
	az->nbstreams = HDA_GCAP_BSS(gcap);
	az->ok64 = (gcap & HDA_GCAP_64OK) != 0;
	DPRINTF(("%s: host: %d output, %d input, and %d bidi streams\n",
	    device_xname(az->dev), az->nostreams, az->nistreams, az->nbstreams));
	if (az->nistreams > 0)
		az->mode_cap |= AUMODE_RECORD;
	if (az->nostreams > 0)
		az->mode_cap |= AUMODE_PLAY;

	/* 4.2.2 Starting the High Definition Audio Controller */
	DPRINTF(("%s: resetting\n", __func__));
	gctl = AZ_READ_4(az, GCTL);
	AZ_WRITE_4(az, GCTL, gctl & ~HDA_GCTL_CRST);
	for (i = 5000; i >= 0; i--) {
		DELAY(10);
		if ((AZ_READ_4(az, GCTL) & HDA_GCTL_CRST) == 0)
			break;
	}
	DPRINTF(("%s: reset counter = %d\n", __func__, i));
	if (i <= 0) {
		aprint_error_dev(az->dev, "reset failure\n");
		return ETIMEDOUT;
	}
	DELAY(1000);
	gctl = AZ_READ_4(az, GCTL);
	AZ_WRITE_4(az, GCTL, gctl | HDA_GCTL_CRST);
	for (i = 5000; i >= 0; i--) {
		DELAY(10);
		if (AZ_READ_4(az, GCTL) & HDA_GCTL_CRST)
			break;
	}
	DPRINTF(("%s: reset counter = %d\n", __func__, i));
	if (i <= 0) {
		aprint_error_dev(az->dev, "reset-exit failure\n");
		return ETIMEDOUT;
	}

	/* enable unsolicited response */
	gctl = AZ_READ_4(az, GCTL);
	AZ_WRITE_4(az, GCTL, gctl | HDA_GCTL_UNSOL);

	/* 4.3 Codec discovery */
	DELAY(1000);
	statests = AZ_READ_2(az, STATESTS);
	for (i = 0, n = 0; i < 15; i++) {
		if ((statests >> i) & 1) {
			DPRINTF(("%s: found a codec at #%d\n",
				device_xname(az->dev), i));
			az->codecs[n].address = i;
			az->codecs[n++].dev = az->dev;
		}
	}
	az->ncodecs = n;
	if (az->ncodecs < 1) {
		aprint_error_dev(az->dev, "No HD-Audio codecs\n");
		return -1;
	}
	return 0;
}

static void
azalia_attach_intr(device_t self)
{
	azalia_t *az;
	int err, i, c, reinit;

	az = device_private(self);
	reinit = az->audiodev == NULL ? 0 : 1;

	AZ_WRITE_2(az, STATESTS, HDA_STATESTS_SDIWAKE);
	AZ_WRITE_1(az, RIRBSTS, HDA_RIRBSTS_RINTFL | HDA_RIRBSTS_RIRBOIS);
	AZ_WRITE_4(az, INTSTS, HDA_INTSTS_CIS | HDA_INTSTS_GIS);
	AZ_WRITE_4(az, DPLBASE, 0);
	AZ_WRITE_4(az, DPUBASE, 0);

	/* 4.4.1 Command Outbound Ring Buffer */
	if (azalia_init_corb(az, reinit))
		goto err_exit;
	/* 4.4.2 Response Inbound Ring Buffer */
	if (azalia_init_rirb(az, reinit))
		goto err_exit;

	AZ_WRITE_4(az, INTCTL,
	    AZ_READ_4(az, INTCTL) | HDA_INTCTL_CIE | HDA_INTCTL_GIE);

	c = -1;
	for (i = 0; i < az->ncodecs; i++) {
		err = azalia_codec_init(&az->codecs[i], reinit, az->subid);
		if (!err && c < 0)
			c = i;
	}
	if (c < 0)
		goto err_exit;
	/* Use the first audio codec */
	az->codecno = c;
	DPRINTF(("%s: using the #%d codec\n",
		device_xname(az->dev), az->codecno));
	if (az->codecs[c].dacs.ngroups <= 0)
		az->mode_cap &= ~AUMODE_PLAY;
	if (az->codecs[c].adcs.ngroups <= 0)
		az->mode_cap &= ~AUMODE_RECORD;

	/* Use stream#1 and #2.  Don't use stream#0. */
	if (reinit == 0) {
		if (azalia_stream_init(&az->pstream, az, az->nistreams + 0,
		    1, AUMODE_PLAY))
			goto err_exit;
		if (azalia_stream_init(&az->rstream, az, 0, 2, AUMODE_RECORD))
			goto err_exit;

		az->audiodev = audio_attach_mi(&azalia_hw_if, az, az->dev);
	}
	return;
err_exit:
	azalia_pci_detach(self, 0);
	return;
}

static int
azalia_init_corb(azalia_t *az, int reinit)
{
	int entries, err, i;
	uint16_t corbrp, corbwp;
	uint8_t corbsize, cap, corbctl;

	/* stop the CORB */
	corbctl = AZ_READ_1(az, CORBCTL);
	if (corbctl & HDA_CORBCTL_CORBRUN) { /* running? */
		AZ_WRITE_1(az, CORBCTL, corbctl & ~HDA_CORBCTL_CORBRUN);
		for (i = 5000; i >= 0; i--) {
			DELAY(10);
			corbctl = AZ_READ_1(az, CORBCTL);
			if ((corbctl & HDA_CORBCTL_CORBRUN) == 0)
				break;
		}
		if (i <= 0) {
			aprint_error_dev(az->dev, "CORB is running\n");
			return EBUSY;
		}
	}

	/* determine CORB size */
	corbsize = AZ_READ_1(az, CORBSIZE);
	cap = corbsize & HDA_CORBSIZE_CORBSZCAP_MASK;
	corbsize &= ~HDA_CORBSIZE_CORBSIZE_MASK;
	if (cap & HDA_CORBSIZE_CORBSZCAP_256) {
		entries = 256;
		corbsize |= HDA_CORBSIZE_CORBSIZE_256;
	} else if (cap & HDA_CORBSIZE_CORBSZCAP_16) {
		entries = 16;
		corbsize |= HDA_CORBSIZE_CORBSIZE_16;
	} else if (cap & HDA_CORBSIZE_CORBSZCAP_2) {
		entries = 2;
		corbsize |= HDA_CORBSIZE_CORBSIZE_2;
	} else {
		aprint_error_dev(az->dev, "Invalid CORBSZCAP: 0x%2x\n", cap);
		return -1;
	}

	if (reinit == 0) {
		err = azalia_alloc_dmamem(az, entries * sizeof(corb_entry_t),
		    128, &az->corb_dma);
		if (err) {
			aprint_error_dev(az->dev, "can't allocate CORB buffer\n");
			return err;
		}
	}
	AZ_WRITE_4(az, CORBLBASE, (uint32_t)AZALIA_DMA_DMAADDR(&az->corb_dma));
	AZ_WRITE_4(az, CORBUBASE, PTR_UPPER32(AZALIA_DMA_DMAADDR(&az->corb_dma)));
	AZ_WRITE_1(az, CORBSIZE, corbsize);
	az->corb_size = entries;

	DPRINTF(("%s: CORB allocation succeeded.\n", __func__));

	/* reset CORBRP */
	corbrp = AZ_READ_2(az, CORBRP);
	AZ_WRITE_2(az, CORBRP, corbrp | HDA_CORBRP_CORBRPRST);
	AZ_WRITE_2(az, CORBRP, corbrp & ~HDA_CORBRP_CORBRPRST);
	for (i = 5000; i >= 0; i--) {
		DELAY(10);
		corbrp = AZ_READ_2(az, CORBRP);
		if ((corbrp & HDA_CORBRP_CORBRPRST) == 0)
			break;
	}
	if (i <= 0) {
		aprint_error_dev(az->dev, "CORBRP reset failure\n");
		return -1;
	}
	DPRINTF(("%s: CORBWP=%d; size=%d\n", __func__,
		 AZ_READ_2(az, CORBRP) & HDA_CORBRP_CORBRP, az->corb_size));

	/* clear CORBWP */
	corbwp = AZ_READ_2(az, CORBWP);
	AZ_WRITE_2(az, CORBWP, corbwp & ~HDA_CORBWP_CORBWP);

	/* Run! */
	corbctl = AZ_READ_1(az, CORBCTL);
	AZ_WRITE_1(az, CORBCTL, corbctl | HDA_CORBCTL_CORBRUN);
	return 0;
}

static int
azalia_delete_corb(azalia_t *az)
{
	int i;
	uint8_t corbctl;

	if (az->corb_dma.addr == NULL)
		return 0;
	/* stop the CORB */
	corbctl = AZ_READ_1(az, CORBCTL);
	AZ_WRITE_1(az, CORBCTL, corbctl & ~HDA_CORBCTL_CORBRUN);
	for (i = 5000; i >= 0; i--) {
		DELAY(10);
		corbctl = AZ_READ_1(az, CORBCTL);
		if ((corbctl & HDA_CORBCTL_CORBRUN) == 0)
			break;
	}
	azalia_free_dmamem(az, &az->corb_dma);
	return 0;
}

static int
azalia_init_rirb(azalia_t *az, int reinit)
{
	int entries, err, i;
	uint16_t rirbwp;
	uint8_t rirbsize, cap, rirbctl;

	/* stop the RIRB */
	rirbctl = AZ_READ_1(az, RIRBCTL);
	if (rirbctl & HDA_RIRBCTL_RIRBDMAEN) { /* running? */
		AZ_WRITE_1(az, RIRBCTL, rirbctl & ~HDA_RIRBCTL_RIRBDMAEN);
		for (i = 5000; i >= 0; i--) {
			DELAY(10);
			rirbctl = AZ_READ_1(az, RIRBCTL);
			if ((rirbctl & HDA_RIRBCTL_RIRBDMAEN) == 0)
				break;
		}
		if (i <= 0) {
			aprint_error_dev(az->dev, "RIRB is running\n");
			return EBUSY;
		}
	}

	/* determine RIRB size */
	rirbsize = AZ_READ_1(az, RIRBSIZE);
	cap = rirbsize & HDA_RIRBSIZE_RIRBSZCAP_MASK;
	rirbsize &= ~HDA_RIRBSIZE_RIRBSIZE_MASK;
	if (cap & HDA_RIRBSIZE_RIRBSZCAP_256) {
		entries = 256;
		rirbsize |= HDA_RIRBSIZE_RIRBSIZE_256;
	} else if (cap & HDA_RIRBSIZE_RIRBSZCAP_16) {
		entries = 16;
		rirbsize |= HDA_RIRBSIZE_RIRBSIZE_16;
	} else if (cap & HDA_RIRBSIZE_RIRBSZCAP_2) {
		entries = 2;
		rirbsize |= HDA_RIRBSIZE_RIRBSIZE_2;
	} else {
		aprint_error_dev(az->dev, "Invalid RIRBSZCAP: 0x%2x\n", cap);
		return -1;
	}

	if (reinit == 0) {
		err = azalia_alloc_dmamem(az, entries * sizeof(rirb_entry_t),
		    128, &az->rirb_dma);
		if (err) {
			aprint_error_dev(az->dev, "can't allocate RIRB buffer\n");
			return err;
		}
	}
	AZ_WRITE_4(az, RIRBLBASE, (uint32_t)AZALIA_DMA_DMAADDR(&az->rirb_dma));
	AZ_WRITE_4(az, RIRBUBASE, PTR_UPPER32(AZALIA_DMA_DMAADDR(&az->rirb_dma)));
	AZ_WRITE_1(az, RIRBSIZE, rirbsize);
	az->rirb_size = entries;

	DPRINTF(("%s: RIRB allocation succeeded.\n", __func__));

	/* setup the unsolicited response queue */
	az->unsolq_rp = 0;
	az->unsolq_wp = 0;
	az->unsolq_kick = FALSE;
	if (reinit == 0) {
		az->unsolq = kmem_zalloc(sizeof(rirb_entry_t) * UNSOLQ_SIZE,
		    KM_SLEEP);
	} else {
		memset(az->unsolq, 0, sizeof(rirb_entry_t) * UNSOLQ_SIZE);
	}
	if (az->unsolq == NULL) {
		aprint_error_dev(az->dev, "can't allocate unsolicited response queue.\n");
		azalia_free_dmamem(az, &az->rirb_dma);
		return ENOMEM;
	}

#if notyet
	rirbctl = AZ_READ_1(az, RIRBCTL);
	AZ_WRITE_1(az, RIRBCTL, rirbctl & ~HDA_RIRBCTL_RINTCTL);
#endif

	/* reset the write pointer */
	rirbwp = AZ_READ_2(az, RIRBWP);
	AZ_WRITE_2(az, RIRBWP, rirbwp | HDA_RIRBWP_RIRBWPRST);

	/* clear the read pointer */
	az->rirb_rp = AZ_READ_2(az, RIRBWP) & HDA_RIRBWP_RIRBWP;
	DPRINTF(("%s: RIRBRP=%d, size=%d\n", __func__, az->rirb_rp, az->rirb_size));

	AZ_WRITE_2(az, RINTCNT, 1);

	/* Run! */
	rirbctl = AZ_READ_1(az, RIRBCTL);
	AZ_WRITE_1(az, RIRBCTL, rirbctl | HDA_RIRBCTL_RIRBDMAEN | HDA_RIRBCTL_RINTCTL);
	for (i = 5000; i >= 0; i--) {
		DELAY(10);
		rirbctl = AZ_READ_1(az, RIRBCTL);
		if (rirbctl & HDA_RIRBCTL_RIRBDMAEN)
			break;
	}
	if (i <= 0) {
		aprint_error_dev(az->dev, "RIRB is not running\n");
		return EBUSY;
	}

	return 0;
}

static int
azalia_delete_rirb(azalia_t *az)
{
	int i;
	uint8_t rirbctl;

	if (az->unsolq != NULL) {
		kmem_free(az->unsolq, UNSOLQ_SIZE);
		az->unsolq = NULL;
	}
	if (az->rirb_dma.addr == NULL)
		return 0;
	/* stop the RIRB */
	rirbctl = AZ_READ_1(az, RIRBCTL);
	AZ_WRITE_1(az, RIRBCTL, rirbctl & ~HDA_RIRBCTL_RIRBDMAEN);
	for (i = 5000; i >= 0; i--) {
		DELAY(10);
		rirbctl = AZ_READ_1(az, RIRBCTL);
		if ((rirbctl & HDA_RIRBCTL_RIRBDMAEN) == 0)
			break;
	}
	azalia_free_dmamem(az, &az->rirb_dma);
	return 0;
}

static int
azalia_set_command(const azalia_t *az, int caddr, nid_t nid, uint32_t control,
		   uint32_t param)
{
	corb_entry_t *corb;
	int wp;
	uint32_t verb;
	uint16_t corbwp;

#ifdef DIAGNOSTIC
	if ((AZ_READ_1(az, CORBCTL) & HDA_CORBCTL_CORBRUN) == 0) {
		aprint_error_dev(az->dev, "CORB is not running.\n");
		return -1;
	}
#endif
	verb = (caddr << 28) | (nid << 20) | (control << 8) | param;
	corbwp = AZ_READ_2(az, CORBWP);
	wp = corbwp & HDA_CORBWP_CORBWP;
	corb = (corb_entry_t*)az->corb_dma.addr;
	if (++wp >= az->corb_size)
		wp = 0;
	corb[wp] = verb;
	AZ_WRITE_2(az, CORBWP, (corbwp & ~HDA_CORBWP_CORBWP) | wp);
#if 0
	DPRINTF(("%s: caddr=%d nid=%d control=0x%x param=0x%x verb=0x%8.8x wp=%d\n",
		 __func__, caddr, nid, control, param, verb, wp));
#endif
	return 0;
}

static int
azalia_get_response(azalia_t *az, uint32_t *result)
{
	const rirb_entry_t *rirb;
	int i;
	uint16_t wp;

#ifdef DIAGNOSTIC
	if ((AZ_READ_1(az, RIRBCTL) & HDA_RIRBCTL_RIRBDMAEN) == 0) {
		aprint_error_dev(az->dev, "RIRB is not running.\n");
		return -1;
	}
#endif
	for (i = 5000; i >= 0; i--) {
		wp = AZ_READ_2(az, RIRBWP) & HDA_RIRBWP_RIRBWP;
		if (az->rirb_rp != wp)
			break;
		DELAY(10);
	}
	if (i <= 0) {
		aprint_error_dev(az->dev, "RIRB time out\n");
		return ETIMEDOUT;
	}
	rirb = (rirb_entry_t*)az->rirb_dma.addr;
	for (;;) {
		if (++az->rirb_rp >= az->rirb_size)
			az->rirb_rp = 0;
		if (rirb[az->rirb_rp].resp_ex & RIRB_RESP_UNSOL) {
			az->unsolq[az->unsolq_wp].resp = rirb[az->rirb_rp].resp;
			az->unsolq[az->unsolq_wp++].resp_ex = rirb[az->rirb_rp].resp_ex;
			az->unsolq_wp %= UNSOLQ_SIZE;
		} else
			break;
	}
	if (result != NULL)
		*result = rirb[az->rirb_rp].resp;
	azalia_rirb_kick_unsol_events(az);
#if 0
	DPRINTF(("%s: rirbwp=%d rp=%d resp1=0x%8.8x resp2=0x%8.8x\n",
		 __func__, wp, az->rirb_rp, rirb[az->rirb_rp].resp,
		 rirb[az->rirb_rp].resp_ex));
	for (i = 0; i < 16 /*az->rirb_size*/; i++) {
		DPRINTF(("rirb[%d] 0x%8.8x:0x%8.8x ", i, rirb[i].resp, rirb[i].resp_ex));
		if ((i % 2) == 1)
			DPRINTF(("\n"));
	}
#endif
	return 0;
}

static void
azalia_rirb_kick_unsol_events(azalia_t *az)
{
	if (az->unsolq_kick)
		return;
	az->unsolq_kick = TRUE;
	while (az->unsolq_rp != az->unsolq_wp) {
		int i;
		int tag;
		codec_t *codec;
		i = RIRB_RESP_CODEC(az->unsolq[az->unsolq_rp].resp_ex);
		tag = RIRB_UNSOL_TAG(az->unsolq[az->unsolq_rp].resp);
		codec = &az->codecs[i];
		DPRINTF(("%s: codec#=%d tag=%d\n", __func__, i, tag));
		az->unsolq_rp++;
		az->unsolq_rp %= UNSOLQ_SIZE;
		if (codec->unsol_event != NULL)
			codec->unsol_event(codec, tag);
	}
	az->unsolq_kick = FALSE;
}

static void
azalia_rirb_intr(azalia_t *az)
{
	const rirb_entry_t *rirb;
	uint16_t wp, newrp;

	wp = AZ_READ_2(az, RIRBWP) & HDA_RIRBWP_RIRBWP;
	if (az->rirb_rp == wp)
		return;		/* interrupted but no data in RIRB */
	/* Copy the first sequence of unsolicited reponses in the RIRB to
	 * unsolq.  Don't consume non-unsolicited responses. */
	rirb = (rirb_entry_t*)az->rirb_dma.addr;
	while (az->rirb_rp != wp) {
		newrp = az->rirb_rp + 1;
		if (newrp >= az->rirb_size)
			newrp = 0;
		if (rirb[newrp].resp_ex & RIRB_RESP_UNSOL) {
			az->unsolq[az->unsolq_wp].resp = rirb[newrp].resp;
			az->unsolq[az->unsolq_wp++].resp_ex = rirb[newrp].resp_ex;
			az->unsolq_wp %= UNSOLQ_SIZE;
			az->rirb_rp = newrp;
		} else {
			break;
		}
	}
	azalia_rirb_kick_unsol_events(az);
}

static int
azalia_alloc_dmamem(azalia_t *az, size_t size, size_t align, azalia_dma_t *d)
{
	int err;
	int nsegs;

	d->size = size;
	err = bus_dmamem_alloc(az->dmat, size, align, 0, d->segments, 1,
	    &nsegs, BUS_DMA_WAITOK);
	if (err)
		return err;
	if (nsegs != 1)
		goto free;
	err = bus_dmamem_map(az->dmat, d->segments, 1, size,
	    &d->addr, BUS_DMA_WAITOK | BUS_DMA_COHERENT | BUS_DMA_NOCACHE);
	if (err)
		goto free;
	err = bus_dmamap_create(az->dmat, size, 1, size, 0,
	    BUS_DMA_WAITOK, &d->map);
	if (err)
		goto unmap;
	err = bus_dmamap_load(az->dmat, d->map, d->addr, size,
	    NULL, BUS_DMA_WAITOK);
	if (err)
		goto destroy;

	if (!az->ok64 && PTR_UPPER32(AZALIA_DMA_DMAADDR(d)) != 0) {
		azalia_free_dmamem(az, d);
		return -1;
	}
	return 0;

destroy:
	bus_dmamap_destroy(az->dmat, d->map);
unmap:
	bus_dmamem_unmap(az->dmat, d->addr, size);
free:
	bus_dmamem_free(az->dmat, d->segments, 1);
	d->addr = NULL;
	return err;
}

static int
azalia_free_dmamem(const azalia_t *az, azalia_dma_t* d)
{
	if (d->addr == NULL)
		return 0;
	bus_dmamap_unload(az->dmat, d->map);
	bus_dmamap_destroy(az->dmat, d->map);
	bus_dmamem_unmap(az->dmat, d->addr, d->size);
	bus_dmamem_free(az->dmat, d->segments, 1);
	d->addr = NULL;
	return 0;
}

/* ================================================================
 * HDA coodec functions
 * ================================================================ */

static int
azalia_codec_init(codec_t *this, int reinit, uint32_t subid)
{
#define LEAD_LEN	100
	char lead[LEAD_LEN];
	uint32_t rev, id, result;
	int err, addr, n, i;

	this->comresp = azalia_codec_comresp;
	addr = this->address;
	DPRINTF(("%s: information of codec[%d] follows:\n",
	    device_xname(this->dev), addr));
	/* codec vendor/device/revision */
	err = this->comresp(this, CORB_NID_ROOT, CORB_GET_PARAMETER,
	    COP_REVISION_ID, &rev);
	if (err)
		return err;
	err = this->comresp(this, CORB_NID_ROOT, CORB_GET_PARAMETER,
	    COP_VENDOR_ID, &id);
	if (err)
		return err;
	this->vid = id;
	this->subid = subid;

	if (!reinit) {
		err = azalia_codec_init_vtbl(this);
		if (err)
			return err;
	}

	if (!reinit) {
		aprint_normal("%s: codec[%d]: ", device_xname(this->dev), addr);
		if (this->name == NULL) {
			aprint_normal("0x%4.4x/0x%4.4x (rev. %u.%u)",
			    id >> 16, id & 0xffff,
			    COP_RID_REVISION(rev), COP_RID_STEPPING(rev));
		} else {
			aprint_normal("%s (rev. %u.%u)", this->name,
			    COP_RID_REVISION(rev), COP_RID_STEPPING(rev));
		}
		aprint_normal(", HDA rev. %u.%u\n",
		    COP_RID_MAJ(rev), COP_RID_MIN(rev));
	}

	/* identify function nodes */
	err = this->comresp(this, CORB_NID_ROOT, CORB_GET_PARAMETER,
	    COP_SUBORDINATE_NODE_COUNT, &result);
	if (err)
		return err;
	this->nfunctions = COP_NSUBNODES(result);
	if (COP_NSUBNODES(result) <= 0) {
		aprint_error("%s: No function groups\n",
		    device_xname(this->dev));
		return -1;
	}
	/* iterate function nodes and find an audio function */
	n = COP_START_NID(result);
	DPRINTF(("%s: nidstart=%d #functions=%d\n",
	    __func__, n, this->nfunctions));
	this->audiofunc = -1;
	for (i = 0; i < this->nfunctions; i++) {
		err = this->comresp(this, n + i, CORB_GET_PARAMETER,
		    COP_FUNCTION_GROUP_TYPE, &result);
		if (err)
			continue;
		DPRINTF(("%s: FTYPE result = 0x%8.8x\n", __func__, result));
		if (COP_FTYPE(result) == COP_FTYPE_AUDIO) {
			this->audiofunc = n + i;
			break;	/* XXX multiple audio functions? */
		} else if (COP_FTYPE(result) == COP_FTYPE_MODEM && !reinit) {
			aprint_normal("%s: codec[%d]: No support for modem "
			    "function groups\n",
			    device_xname(this->dev), addr);
		}
	}
	if (this->audiofunc < 0 && !reinit) {
		aprint_verbose("%s: codec[%d] has no audio function groups\n",
		    device_xname(this->dev), addr);
		return -1;
	}

	/* power the audio function */
	this->comresp(this, this->audiofunc, CORB_SET_POWER_STATE, CORB_PS_D0, &result);
	DELAY(100);

	/* check widgets in the audio function */
	err = this->comresp(this, this->audiofunc,
	    CORB_GET_PARAMETER, COP_SUBORDINATE_NODE_COUNT, &result);
	if (err)
		return err;
	DPRINTF(("%s: There are %d widgets in the audio function.\n",
	   __func__, COP_NSUBNODES(result)));
	this->wstart = COP_START_NID(result);
	if (this->wstart < 2) {
		if (!reinit)
			aprint_error("%s: invalid node structure\n",
			    device_xname(this->dev));
		return -1;
	}
	this->wend = this->wstart + COP_NSUBNODES(result);
	if (!reinit) {
		this->w = kmem_zalloc(sizeof(widget_t) * this->wend,
		    KM_SLEEP);
		if (this->w == NULL) {
			aprint_error("%s: out of memory\n",
			    device_xname(this->dev));
			return ENOMEM;
		}
	} else
		memset(this->w, 0, sizeof(widget_t) * this->wend);

	/* query the base parameters */
	this->comresp(this, this->audiofunc, CORB_GET_PARAMETER,
	    COP_STREAM_FORMATS, &result);
	this->w[this->audiofunc].d.audio.encodings = result;
	this->comresp(this, this->audiofunc, CORB_GET_PARAMETER,
	    COP_PCM, &result);
	this->w[this->audiofunc].d.audio.bits_rates = result;
	this->comresp(this, this->audiofunc, CORB_GET_PARAMETER,
	    COP_INPUT_AMPCAP, &result);
	this->w[this->audiofunc].inamp_cap = result;
	this->comresp(this, this->audiofunc, CORB_GET_PARAMETER,
	    COP_OUTPUT_AMPCAP, &result);
	this->w[this->audiofunc].outamp_cap = result;
	lead[0] = 0;
#ifdef AZALIA_DEBUG
	snprintf(lead, LEAD_LEN, "%s:    ", device_xname(this->dev));
	azalia_widget_print_audio(&this->w[this->audiofunc], lead, -1);
	result = this->w[this->audiofunc].inamp_cap;
	DPRINTF(("%sinamp: mute=%u size=%u steps=%u offset=%u\n", lead,
	    (result & COP_AMPCAP_MUTE) != 0, COP_AMPCAP_STEPSIZE(result),
	    COP_AMPCAP_NUMSTEPS(result), COP_AMPCAP_OFFSET(result)));
	result = this->w[this->audiofunc].outamp_cap;
	DPRINTF(("%soutamp: mute=%u size=%u steps=%u offset=%u\n", lead,
	    (result & COP_AMPCAP_MUTE) != 0, COP_AMPCAP_STEPSIZE(result),
	    COP_AMPCAP_NUMSTEPS(result), COP_AMPCAP_OFFSET(result)));
#endif

	strlcpy(this->w[CORB_NID_ROOT].name, "root",
	    sizeof(this->w[CORB_NID_ROOT].name));
	strlcpy(this->w[this->audiofunc].name, "hdaudio",
	    sizeof(this->w[this->audiofunc].name));
	FOR_EACH_WIDGET(this, i) {
		err = azalia_widget_init(&this->w[i], this, i, lead);
		if (err)
			return err;
	}
#if defined(AZALIA_DEBUG) &&  defined(AZALIA_DEBUG_DOT)
	DPRINTF(("-------- Graphviz DOT starts\n"));
	if (this->name == NULL) {
		DPRINTF(("digraph \"0x%4.4x/0x%4.4x (rev. %u.%u)\" {\n",
		    id >> 16, id & 0xffff,
		    COP_RID_REVISION(rev), COP_RID_STEPPING(rev)));
	} else {
		DPRINTF(("digraph \"%s (rev. %u.%u)\" {\n", this->name,
		    COP_RID_REVISION(rev), COP_RID_STEPPING(rev)));
	}
	FOR_EACH_WIDGET(this, i) {
		const widget_t *w;
		int j;
		w = &this->w[i];
		switch (w->type) {
		case COP_AWTYPE_AUDIO_OUTPUT:
			DPRINTF((" %s [shape=box,style=filled,fillcolor=\""
			    "#88ff88\"];\n", w->name));
			break;
		case COP_AWTYPE_AUDIO_INPUT:
			DPRINTF((" %s [shape=box,style=filled,fillcolor=\""
			    "#ff8888\"];\n", w->name));
			break;
		case COP_AWTYPE_AUDIO_MIXER:
			DPRINTF((" %s [shape=invhouse];\n", w->name));
			break;
		case COP_AWTYPE_AUDIO_SELECTOR:
			DPRINTF((" %s [shape=invtrapezium];\n", w->name));
			break;
		case COP_AWTYPE_PIN_COMPLEX:
			DPRINTF((" %s [label=\"%s\\ndevice=%s\",style=filled",
			    w->name, w->name, pin_devices[w->d.pin.device]));
			if (w->d.pin.cap & COP_PINCAP_OUTPUT &&
			    w->d.pin.cap & COP_PINCAP_INPUT)
				DPRINTF((",shape=doublecircle,fillcolor=\""
				    "#ffff88\"];\n"));
			else if (w->d.pin.cap & COP_PINCAP_OUTPUT)
				DPRINTF((",shape=circle,fillcolor=\"#88ff88\"];\n"));
			else
				DPRINTF((",shape=circle,fillcolor=\"#ff8888\"];\n"));
			break;
		}
		if ((w->widgetcap & COP_AWCAP_CONNLIST) == 0)
			continue;
		for (j = 0; j < w->nconnections; j++) {
			int src = w->connections[j];
			if (!VALID_WIDGET_NID(src, this))
				continue;
			DPRINTF((" %s -> %s [sametail=%s];\n",
			    this->w[src].name, w->name, this->w[src].name));
		}
	}

	DPRINTF((" {rank=min;"));
	FOR_EACH_WIDGET(this, i) {
		const widget_t *w;
		w = &this->w[i];
		switch (w->type) {
		case COP_AWTYPE_AUDIO_OUTPUT:
		case COP_AWTYPE_AUDIO_INPUT:
			DPRINTF((" %s;", w->name));
			break;
		}
	}
	DPRINTF(("}\n"));

	DPRINTF((" {rank=max;"));
	FOR_EACH_WIDGET(this, i) {
		const widget_t *w;
		w = &this->w[i];
		switch (w->type) {
		case COP_AWTYPE_PIN_COMPLEX:
			DPRINTF((" %s;", w->name));
			break;
		}
	}
	DPRINTF(("}\n"));

	DPRINTF(("}\n"));
	DPRINTF(("-------- Graphviz DOT ends\n"));
#endif	/* AZALIA_DEBUG && AZALIA_DEBUG_DOT */

	err = this->init_dacgroup(this);
	if (err)
		return err;
#ifdef AZALIA_DEBUG
	for (i = 0; i < this->dacs.ngroups; i++) {
		DPRINTF(("%s: dacgroup[%d]:", __func__, i));
		for (n = 0; n < this->dacs.groups[i].nconv; n++) {
			DPRINTF((" %2.2x", this->dacs.groups[i].conv[n]));
		}
		DPRINTF(("\n"));
	}
#endif

	/* set invalid values for azalia_codec_construct_format() to work */
	this->dacs.cur = -1;
	this->adcs.cur = -1;
	err = azalia_codec_construct_format(this,
	    this->dacs.ngroups > 0 ? 0 : -1,
	    this->adcs.ngroups > 0 ? 0 : -1);
	if (err)
		return err;

	return this->mixer_init(this);
}

static int
azalia_codec_delete(codec_t *this)
{
	if (this->mixer_delete != NULL)
		this->mixer_delete(this);
	if (this->formats != NULL) {
		kmem_free(this->formats, this->szformats);
		this->formats = NULL;
	}
	auconv_delete_encodings(this->encodings);
	this->encodings = NULL;
	if (this->extra != NULL) {
		kmem_free(this->extra, this->szextra);
		this->extra = NULL;
	}
	return 0;
}

int
azalia_codec_construct_format(codec_t *this, int newdac, int newadc)
{
#ifdef AZALIA_DEBUG
	char flagbuf[FLAGBUFLEN];
	int prev_dac = this->dacs.cur;
	int prev_adc = this->adcs.cur;
#endif
	const convgroup_t *group;
	uint32_t bits_rates;
	int variation;
	int nbits, c, chan, i, err;
	nid_t nid;

	variation = 0;
	chan = 0;

	if (newdac >= 0 && newdac < this->dacs.ngroups) {
		this->dacs.cur = newdac;
		group = &this->dacs.groups[this->dacs.cur];
		bits_rates = this->w[group->conv[0]].d.audio.bits_rates;
		nbits = 0;
		if (bits_rates & COP_PCM_B8)
			nbits++;
		if (bits_rates & COP_PCM_B16)
			nbits++;
		if (bits_rates & COP_PCM_B20)
			nbits++;
		if (bits_rates & COP_PCM_B24)
			nbits++;
		if (bits_rates & COP_PCM_B32)
			nbits++;
		if (nbits == 0) {
			aprint_error("%s: invalid PCM format: 0x%8.8x\n",
				     device_xname(this->dev), bits_rates);
			return -1;
		}
		variation = group->nconv * nbits;
	}

	if (newadc >= 0 && newadc < this->adcs.ngroups) {
		this->adcs.cur = newadc;
		group = &this->adcs.groups[this->adcs.cur];
		bits_rates = this->w[group->conv[0]].d.audio.bits_rates;
		nbits = 0;
		if (bits_rates & COP_PCM_B8)
			nbits++;
		if (bits_rates & COP_PCM_B16)
			nbits++;
		if (bits_rates & COP_PCM_B20)
			nbits++;
		if (bits_rates & COP_PCM_B24)
			nbits++;
		if (bits_rates & COP_PCM_B32)
			nbits++;
		if (nbits == 0) {
			aprint_error("%s: invalid PCM format: 0x%8.8x\n",
				     device_xname(this->dev), bits_rates);
			return -1;
		}
		variation += group->nconv * nbits;
	}

	if (this->formats != NULL)
		kmem_free(this->formats, this->szformats);
	this->nformats = 0;
	this->szformats = sizeof(struct audio_format) * variation;
	this->formats = kmem_zalloc(this->szformats, KM_SLEEP);
	if (this->formats == NULL) {
		aprint_error("%s: out of memory in %s\n",
		    device_xname(this->dev), __func__);
		return ENOMEM;
	}

	/* register formats for playback */
	if (this->dacs.cur >= 0 && this->dacs.cur < this->dacs.ngroups) {
		group = &this->dacs.groups[this->dacs.cur];
		for (c = 0; c < group->nconv; c++) {
			chan = 0;
			bits_rates = ~0;
			for (i = 0; i <= c; i++) {
				nid = group->conv[i];
				chan += WIDGET_CHANNELS(&this->w[nid]);
				bits_rates &= this->w[nid].d.audio.bits_rates;
			}
			azalia_codec_add_bits(this, chan,
			    bits_rates, AUMODE_PLAY);
		}
#ifdef AZALIA_DEBUG
		/* print playback capability */
		if (prev_dac != this->dacs.cur) {
			snprintf(flagbuf, FLAGBUFLEN, "%s: playback: ",
			    device_xname(this->dev));
			azalia_widget_print_audio(&this->w[group->conv[0]],
			    flagbuf, chan);
		}
#endif
	}

	/* register formats for recording */
	if (this->adcs.cur >= 0 && this->adcs.cur < this->adcs.ngroups) {
		group = &this->adcs.groups[this->adcs.cur];
		for (c = 0; c < group->nconv; c++) {
			chan = 0;
			bits_rates = ~0;
			for (i = 0; i <= c; i++) {
				nid = group->conv[i];
				chan += WIDGET_CHANNELS(&this->w[nid]);
				bits_rates &= this->w[nid].d.audio.bits_rates;
			}
			azalia_codec_add_bits(this, chan,
			    bits_rates, AUMODE_RECORD);
		}
#ifdef AZALIA_DEBUG
		/* print recording capability */
		if (prev_adc != this->adcs.cur) {
			snprintf(flagbuf, FLAGBUFLEN, "%s: recording: ",
			    device_xname(this->dev));
			azalia_widget_print_audio(&this->w[group->conv[0]],
			    flagbuf, chan);
		}
#endif
	}

#ifdef DIAGNOSTIC
	if (this->nformats > variation) {
		aprint_error("%s: Internal error: the format buffer is too small: "
		    "nformats=%d variation=%d\n", device_xname(this->dev),
		    this->nformats, variation);
		return ENOMEM;
	}
#endif

	err = auconv_create_encodings(this->formats, this->nformats,
	    &this->encodings);
	if (err)
		return err;
	return 0;
}

static void
azalia_codec_add_bits(codec_t *this, int chan, uint32_t bits_rates, int mode)
{
	if (bits_rates & COP_PCM_B8)
		azalia_codec_add_format(this, chan, 8, 16, bits_rates, mode);
	if (bits_rates & COP_PCM_B16)
		azalia_codec_add_format(this, chan, 16, 16, bits_rates, mode);
	if (bits_rates & COP_PCM_B20)
		azalia_codec_add_format(this, chan, 20, 32, bits_rates, mode);
	if (bits_rates & COP_PCM_B24)
		azalia_codec_add_format(this, chan, 24, 32, bits_rates, mode);
	if (bits_rates & COP_PCM_B32)
		azalia_codec_add_format(this, chan, 32, 32, bits_rates, mode);
}

static void
azalia_codec_add_format(codec_t *this, int chan, int valid, int prec,
    uint32_t rates, int32_t mode)
{
	struct audio_format *f;

	f = &this->formats[this->nformats++];
	f->mode = mode;
	f->encoding = AUDIO_ENCODING_SLINEAR_LE;
	if (valid == 8 && prec == 8)
		f->encoding = AUDIO_ENCODING_ULINEAR_LE;
	f->validbits = valid;
	f->precision = prec;
	f->channels = chan;
	switch (chan) {
	case 1:
		f->channel_mask = AUFMT_MONAURAL;
		break;
	case 2:
		f->channel_mask = AUFMT_STEREO;
		break;
	case 4:
		f->channel_mask = AUFMT_SURROUND4;
		break;
	case 6:
		f->channel_mask = AUFMT_DOLBY_5_1;
		break;
	case 8:
		f->channel_mask = AUFMT_DOLBY_5_1
		    | AUFMT_SIDE_LEFT | AUFMT_SIDE_RIGHT;
		break;
	default:
		f->channel_mask = 0;
	}
	f->frequency_type = 0;
	if (rates & COP_PCM_R80)
		f->frequency[f->frequency_type++] = 8000;
	if (rates & COP_PCM_R110)
		f->frequency[f->frequency_type++] = 11025;
	if (rates & COP_PCM_R160)
		f->frequency[f->frequency_type++] = 16000;
	if (rates & COP_PCM_R220)
		f->frequency[f->frequency_type++] = 22050;
	if (rates & COP_PCM_R320)
		f->frequency[f->frequency_type++] = 32000;
	if (rates & COP_PCM_R441)
		f->frequency[f->frequency_type++] = 44100;
	if (rates & COP_PCM_R480)
		f->frequency[f->frequency_type++] = 48000;
	if (rates & COP_PCM_R882)
		f->frequency[f->frequency_type++] = 88200;
	if (rates & COP_PCM_R960)
		f->frequency[f->frequency_type++] = 96000;
	if (rates & COP_PCM_R1764)
		f->frequency[f->frequency_type++] = 176400;
	if (rates & COP_PCM_R1920)
		f->frequency[f->frequency_type++] = 192000;
	if (rates & COP_PCM_R3840)
		f->frequency[f->frequency_type++] = 384000;
}

static int
azalia_codec_comresp(const codec_t *codec, nid_t nid, uint32_t control,
		     uint32_t param, uint32_t* result)
{
	azalia_t *az = device_private(codec->dev);
	int err;

	err = azalia_set_command(az, codec->address, nid, control, param);
	if (err == 0)
		err = azalia_get_response(az, result);
	return err;
}

static int
azalia_codec_connect_stream(codec_t *this, int dir, uint16_t fmt, int number)
{
	const convgroup_t *group;
	uint32_t v;
	int i, err, startchan, nchan;
	nid_t nid;
	bool flag222;

	DPRINTF(("%s: fmt=0x%4.4x number=%d\n", __func__, fmt, number));
	err = 0;
	if (dir == AUMODE_RECORD)
		group = &this->adcs.groups[this->adcs.cur];
	else
		group = &this->dacs.groups[this->dacs.cur];
	flag222 = group->nconv >= 3 &&
	    (WIDGET_CHANNELS(&this->w[group->conv[0]]) == 2) &&
	    (WIDGET_CHANNELS(&this->w[group->conv[1]]) == 2) &&
	    (WIDGET_CHANNELS(&this->w[group->conv[2]]) == 2);
	nchan = (fmt & HDA_SD_FMT_CHAN) + 1;
	startchan = 0;
	for (i = 0; i < group->nconv; i++) {
		uint32_t stream_chan;
		nid = group->conv[i];

		/* surround and c/lfe handling */
		if (nchan >= 6 && flag222 && i == 1) {
			nid = group->conv[2];
		} else if (nchan >= 6 && flag222 && i == 2) {
			nid = group->conv[1];
		}

		err = this->comresp(this, nid, CORB_SET_CONVERTER_FORMAT, fmt, NULL);
		if (err)
			goto exit;
		stream_chan = (number << 4) | startchan;
		if (startchan >= nchan)
			stream_chan = 0; /* stream#0 */
		err = this->comresp(this, nid, CORB_SET_CONVERTER_STREAM_CHANNEL,
				    stream_chan, NULL);
		if (err)
			goto exit;
		if (this->w[nid].widgetcap & COP_AWCAP_DIGITAL) {
			/* enable S/PDIF */
			this->comresp(this, nid, CORB_GET_DIGITAL_CONTROL,
			    0, &v);
			v = (v & 0xff) | CORB_DCC_DIGEN;
			this->comresp(this, nid, CORB_SET_DIGITAL_CONTROL_L,
			    v, NULL);
		}
		startchan += WIDGET_CHANNELS(&this->w[nid]);
	}

exit:
	DPRINTF(("%s: leave with %d\n", __func__, err));
	return err;
}

static int
azalia_codec_disconnect_stream(codec_t *this, int dir)
{
	const convgroup_t *group;
	uint32_t v;
	int i;
	nid_t nid;
	
	if (dir == AUMODE_RECORD)
		group = &this->adcs.groups[this->adcs.cur];
	else
		group = &this->dacs.groups[this->dacs.cur];
	for (i = 0; i < group->nconv; i++) {
		nid = group->conv[i];
		this->comresp(this, nid,
		    CORB_SET_CONVERTER_STREAM_CHANNEL, 0, NULL);/* stream#0 */
		if (this->w[nid].widgetcap & COP_AWCAP_DIGITAL) {
			/* disable S/PDIF */
			this->comresp(this, nid,
			    CORB_GET_DIGITAL_CONTROL, 0, &v);
			v = (v & ~CORB_DCC_DIGEN) & 0xff;
			this->comresp(this, nid,
			    CORB_SET_DIGITAL_CONTROL_L, v, NULL);
		}
	}
	return 0;
}

/* ================================================================
 * HDA widget functions
 * ================================================================ */

static int
azalia_widget_init(widget_t *this, const codec_t *codec,
		   nid_t nid, const char *lead)
{
	char flagbuf[FLAGBUFLEN];
	uint32_t result;
	int err;

	err = codec->comresp(codec, nid, CORB_GET_PARAMETER,
	    COP_AUDIO_WIDGET_CAP, &result);
	if (err)
		return err;
	this->nid = nid;
	this->widgetcap = result;
	this->type = COP_AWCAP_TYPE(result);
	snprintb(flagbuf, sizeof(flagbuf),
	    "\20\014LRSWAP\013POWER\012DIGITAL"
	    "\011CONNLIST\010UNSOL\07PROC\06STRIPE\05FORMATOV\04AMPOV\03OUTAMP"
	    "\02INAMP\01STEREO", this->widgetcap);
	DPRINTF(("%s: ", device_xname(codec->dev)));
	if (this->widgetcap & COP_AWCAP_POWER) {
		codec->comresp(codec, nid, CORB_SET_POWER_STATE, CORB_PS_D0, &result);
		DELAY(100);
	}
	switch (this->type) {
	case COP_AWTYPE_AUDIO_OUTPUT:
		snprintf(this->name, sizeof(this->name), "dac%2.2x", nid);
		DPRINTF(("%s wcap=%s\n", this->name, flagbuf));
		azalia_widget_init_audio(this, codec, lead);
		break;
	case COP_AWTYPE_AUDIO_INPUT:
		snprintf(this->name, sizeof(this->name), "adc%2.2x", nid);
		DPRINTF(("%s wcap=%s\n", this->name, flagbuf));
		azalia_widget_init_audio(this, codec, lead);
		break;
	case COP_AWTYPE_AUDIO_MIXER:
		snprintf(this->name, sizeof(this->name), "mix%2.2x", nid);
		DPRINTF(("%s wcap=%s\n", this->name, flagbuf));
		break;
	case COP_AWTYPE_AUDIO_SELECTOR:
		snprintf(this->name, sizeof(this->name), "sel%2.2x", nid);
		DPRINTF(("%s wcap=%s\n", this->name, flagbuf));
		break;
	case COP_AWTYPE_PIN_COMPLEX:
		azalia_widget_init_pin(this, codec);
		snprintf(this->name, sizeof(this->name), "%s%2.2x",
		    pin_colors[this->d.pin.color], nid);
		DPRINTF(("%s wcap=%s\n", this->name, flagbuf));
		azalia_widget_print_pin(this, lead);
		break;
	case COP_AWTYPE_POWER:
		snprintf(this->name, sizeof(this->name), "pow%2.2x", nid);
		DPRINTF(("%s wcap=%s\n", this->name, flagbuf));
		break;
	case COP_AWTYPE_VOLUME_KNOB:
		snprintf(this->name, sizeof(this->name), "volume%2.2x", nid);
		DPRINTF(("%s wcap=%s\n", this->name, flagbuf));
		err = codec->comresp(codec, nid, CORB_GET_PARAMETER,
		    COP_VOLUME_KNOB_CAPABILITIES, &result);
		if (!err) {
			this->d.volume.cap = result;
			DPRINTF(("%sdelta=%d steps=%d\n", lead,
			    !!(result & COP_VKCAP_DELTA),
			    COP_VKCAP_NUMSTEPS(result)));
		}
		break;
	case COP_AWTYPE_BEEP_GENERATOR:
		snprintf(this->name, sizeof(this->name), "beep%2.2x", nid);
		DPRINTF(("%s wcap=%s\n", this->name, flagbuf));
		break;
	default:
		snprintf(this->name, sizeof(this->name), "widget%2.2x", nid);
		DPRINTF(("%s wcap=%s\n", this->name, flagbuf));
		break;
	}
	azalia_widget_init_connection(this, codec, lead);

	/* amplifier information */
	if (this->widgetcap & COP_AWCAP_INAMP) {
		if (this->widgetcap & COP_AWCAP_AMPOV)
			codec->comresp(codec, nid, CORB_GET_PARAMETER,
			    COP_INPUT_AMPCAP, &this->inamp_cap);
		else
			this->inamp_cap = codec->w[codec->audiofunc].inamp_cap;
		DPRINTF(("%sinamp: mute=%u size=%u steps=%u offset=%u\n",
		    lead, (this->inamp_cap & COP_AMPCAP_MUTE) != 0,
		    COP_AMPCAP_STEPSIZE(this->inamp_cap),
		    COP_AMPCAP_NUMSTEPS(this->inamp_cap),
		    COP_AMPCAP_OFFSET(this->inamp_cap)));
	}
	if (this->widgetcap & COP_AWCAP_OUTAMP) {
		if (this->widgetcap & COP_AWCAP_AMPOV)
			codec->comresp(codec, nid, CORB_GET_PARAMETER,
			    COP_OUTPUT_AMPCAP, &this->outamp_cap);
		else
			this->outamp_cap = codec->w[codec->audiofunc].outamp_cap;
		DPRINTF(("%soutamp: mute=%u size=%u steps=%u offset=%u\n",
		    lead, (this->outamp_cap & COP_AMPCAP_MUTE) != 0,
		    COP_AMPCAP_STEPSIZE(this->outamp_cap),
		    COP_AMPCAP_NUMSTEPS(this->outamp_cap),
		    COP_AMPCAP_OFFSET(this->outamp_cap)));
	}
	if (codec->init_widget != NULL)
		codec->init_widget(codec, this, nid);
	return 0;
}

static int
azalia_widget_init_audio(widget_t *this, const codec_t *codec, const char *lead)
{
	uint32_t result;
	int err;

	/* check audio format */
	if (this->widgetcap & COP_AWCAP_FORMATOV) {
		err = codec->comresp(codec, this->nid,
		    CORB_GET_PARAMETER, COP_STREAM_FORMATS, &result);
		if (err)
			return err;
		this->d.audio.encodings = result;
		if (result == 0) { /* quirk for CMI9880.
				    * This must not occuur usually... */
			this->d.audio.encodings =
			    codec->w[codec->audiofunc].d.audio.encodings;
			this->d.audio.bits_rates =
			    codec->w[codec->audiofunc].d.audio.bits_rates;
		} else {
			if ((result & COP_STREAM_FORMAT_PCM) == 0) {
				aprint_error("%s: %s: No PCM support: %x\n",
				    device_xname(codec->dev), this->name, result);
				return -1;
			}
			err = codec->comresp(codec, this->nid, CORB_GET_PARAMETER,
			    COP_PCM, &result);
			if (err)
				return err;
			this->d.audio.bits_rates = result;
		}
	} else {
		this->d.audio.encodings =
		    codec->w[codec->audiofunc].d.audio.encodings;
		this->d.audio.bits_rates =
		    codec->w[codec->audiofunc].d.audio.bits_rates;
	}
#ifdef AZALIA_DEBUG
	azalia_widget_print_audio(this, lead, -1);
#endif
	return 0;
}

#ifdef AZALIA_DEBUG
static int
azalia_widget_print_audio(const widget_t *this, const char *lead, int channels)
{
	char flagbuf[FLAGBUFLEN];

	snprintb(flagbuf, sizeof(flagbuf), 
	    "\20\3AC3\2FLOAT32\1PCM", this->d.audio.encodings);
	if (channels < 0) {
		aprint_normal("%sencodings=%s\n", lead, flagbuf);
	} else if (this->widgetcap & COP_AWCAP_DIGITAL) {
		aprint_normal("%smax channels=%d, DIGITAL, encodings=%s\n",
		    lead, channels, flagbuf);
	} else {
		aprint_normal("%smax channels=%d, encodings=%s\n",
		    lead, channels, flagbuf);
	}
	snprintb(flagbuf, sizeof(flagbuf), 
	    "\20\x15""32bit\x14""24bit\x13""20bit"
	    "\x12""16bit\x11""8bit""\x0c""384kHz\x0b""192kHz\x0a""176.4kHz"
	    "\x09""96kHz\x08""88.2kHz\x07""48kHz\x06""44.1kHz\x05""32kHz\x04"
	    "22.05kHz\x03""16kHz\x02""11.025kHz\x01""8kHz",
	    this->d.audio.bits_rates);
	aprint_normal("%sPCM formats=%s\n", lead, flagbuf);
	return 0;
}
#endif

static int
azalia_widget_init_pin(widget_t *this, const codec_t *codec)
{
	uint32_t result;
	int err;

	err = codec->comresp(codec, this->nid, CORB_GET_CONFIGURATION_DEFAULT,
	    0, &result);
	if (err)
		return err;
	this->d.pin.config = result;
	this->d.pin.sequence = CORB_CD_SEQUENCE(result);
	this->d.pin.association = CORB_CD_ASSOCIATION(result);
	this->d.pin.color = CORB_CD_COLOR(result);
	this->d.pin.device = CORB_CD_DEVICE(result);

	err = codec->comresp(codec, this->nid, CORB_GET_PARAMETER,
	    COP_PINCAP, &result);
	if (err)
		return err;
	this->d.pin.cap = result;

	/* input pin */
	if ((this->d.pin.cap & COP_PINCAP_INPUT) &&
	    (this->d.pin.cap & COP_PINCAP_OUTPUT) == 0) {
		err = codec->comresp(codec, this->nid,
		    CORB_GET_PIN_WIDGET_CONTROL, 0, &result);
		if (err == 0) {
			result &= ~CORB_PWC_OUTPUT;
			result |= CORB_PWC_INPUT;
			codec->comresp(codec, this->nid,
			     CORB_SET_PIN_WIDGET_CONTROL, result, NULL);
		}
	}
	/* output pin, or bidirectional pin */
	if (this->d.pin.cap & COP_PINCAP_OUTPUT) {
		err = codec->comresp(codec, this->nid,
		    CORB_GET_PIN_WIDGET_CONTROL, 0, &result);
		if (err == 0) {
			result &= ~CORB_PWC_INPUT;
			result |= CORB_PWC_OUTPUT;
			codec->comresp(codec, this->nid,
			    CORB_SET_PIN_WIDGET_CONTROL, result, NULL);
		}
	}
	return 0;
}

static int
azalia_widget_print_pin(const widget_t *this, const char *lead)
{
	char flagbuf[FLAGBUFLEN];

	DPRINTF(("%spin config; device=%s color=%s assoc=%d seq=%d", lead,
	    pin_devices[this->d.pin.device], pin_colors[this->d.pin.color],
	    this->d.pin.association, this->d.pin.sequence));
	snprintb(flagbuf, sizeof(flagbuf), 
	    "\20\021EAPD\07BALANCE\06INPUT"
	    "\05OUTPUT\04HEADPHONE\03PRESENCE\02TRIGGER\01IMPEDANCE",
	    this->d.pin.cap);
	DPRINTF((" cap=%s\n", flagbuf));
	return 0;
}

static int
azalia_widget_init_connection(widget_t *this, const codec_t *codec,
			      const char *lead)
{
	uint32_t result;
	int err;
	bool longform;
	int length, i;

	this->selected = -1;
	if ((this->widgetcap & COP_AWCAP_CONNLIST) == 0)
		return 0;

	err = codec->comresp(codec, this->nid, CORB_GET_PARAMETER,
	    COP_CONNECTION_LIST_LENGTH, &result);
	if (err)
		return err;
	longform = (result & COP_CLL_LONG) != 0;
	length = COP_CLL_LENGTH(result);
	if (length == 0)
		return 0;
	this->nconnections = length;
	this->connections = kmem_alloc(sizeof(nid_t) * (length + 3), KM_SLEEP);
	if (this->connections == NULL) {
		aprint_error("%s: out of memory\n", device_xname(codec->dev));
		return ENOMEM;
	}
	if (longform) {
		for (i = 0; i < length;) {
			err = codec->comresp(codec, this->nid,
			    CORB_GET_CONNECTION_LIST_ENTRY, i, &result);
			if (err)
				return err;
			this->connections[i++] = CORB_CLE_LONG_0(result);
			this->connections[i++] = CORB_CLE_LONG_1(result);
		}
	} else {
		for (i = 0; i < length;) {
			err = codec->comresp(codec, this->nid,
			    CORB_GET_CONNECTION_LIST_ENTRY, i, &result);
			if (err)
				return err;
			this->connections[i++] = CORB_CLE_SHORT_0(result);
			this->connections[i++] = CORB_CLE_SHORT_1(result);
			this->connections[i++] = CORB_CLE_SHORT_2(result);
			this->connections[i++] = CORB_CLE_SHORT_3(result);
		}
	}
	if (length > 0) {
		DPRINTF(("%sconnections=0x%x", lead, this->connections[0]));
		for (i = 1; i < length; i++) {
			DPRINTF((",0x%x", this->connections[i]));
		}

		err = codec->comresp(codec, this->nid,
		    CORB_GET_CONNECTION_SELECT_CONTROL, 0, &result);
		if (err)
			return err;
		this->selected = CORB_CSC_INDEX(result);
		DPRINTF(("; selected=0x%x\n", this->connections[result]));
	}
	return 0;
}

/* ================================================================
 * Stream functions
 * ================================================================ */

static int
azalia_stream_init(stream_t *this, azalia_t *az, int regindex, int strnum, int dir)
{
	int err;

	this->az = az;
	this->regbase = HDA_SD_BASE + regindex * HDA_SD_SIZE;
	this->intr_bit = 1 << regindex;
	this->number = strnum;
	this->dir = dir;

	/* setup BDL buffers */
	err = azalia_alloc_dmamem(az, sizeof(bdlist_entry_t) * HDA_BDL_MAX,
				  128, &this->bdlist);
	if (err) {
		aprint_error_dev(az->dev, "can't allocate a BDL buffer\n");
		return err;
	}
	return 0;
}

static int
azalia_stream_delete(stream_t *this, azalia_t *az)
{
	if (this->bdlist.addr == NULL)
		return 0;
	azalia_free_dmamem(az, &this->bdlist);
	return 0;
}

static int
azalia_stream_reset(stream_t *this)
{
	int i;
	uint16_t ctl;

	if (this->bdlist.addr == NULL)
		return EINVAL;
	ctl = STR_READ_2(this, CTL);
	STR_WRITE_2(this, CTL, ctl | HDA_SD_CTL_SRST);
	for (i = 5000; i >= 0; i--) {
		DELAY(10);
		ctl = STR_READ_2(this, CTL);
		if (ctl & HDA_SD_CTL_SRST)
			break;
	}
	if (i <= 0) {
		aprint_error_dev(this->az->dev, "stream reset failure 1\n");
		return -1;
	}
	STR_WRITE_2(this, CTL, ctl & ~HDA_SD_CTL_SRST);
	for (i = 5000; i >= 0; i--) {
		DELAY(10);
		ctl = STR_READ_2(this, CTL);
		if ((ctl & HDA_SD_CTL_SRST) == 0)
			break;
	}
	if (i <= 0) {
		aprint_error_dev(this->az->dev, "stream reset failure 2\n");
		return -1;
	}
	return 0;
}

static int
azalia_stream_start(stream_t *this, void *start, void *end, int blk,
    void (*intr)(void *), void *arg, uint16_t fmt)
{
	bdlist_entry_t *bdlist;
	bus_addr_t dmaaddr;
	int err, index;
	uint32_t intctl;
	uint8_t ctl, ctl2;

	DPRINTF(("%s: start=%p end=%p\n", __func__, start, end));
	if (this->bdlist.addr == NULL)
		return EINVAL;
	this->intr = intr;
	this->intr_arg = arg;

	err = azalia_stream_reset(this);
	if (err)
		return err;

	/* setup BDL */
	dmaaddr = AZALIA_DMA_DMAADDR(&this->buffer);
	this->dmaend = dmaaddr + ((char *)end - (char *)start);
	bdlist = (bdlist_entry_t*)this->bdlist.addr;
	for (index = 0; index < HDA_BDL_MAX; index++) {
		bdlist[index].low = dmaaddr;
		bdlist[index].high = PTR_UPPER32(dmaaddr);
		bdlist[index].length = blk;
		bdlist[index].flags = BDLIST_ENTRY_IOC;
		dmaaddr += blk;
		if (dmaaddr >= this->dmaend) {
			index++;
			break;
		}
	}
	/* The BDL covers the whole of the buffer. */
	this->dmanext = AZALIA_DMA_DMAADDR(&this->buffer);

	dmaaddr = AZALIA_DMA_DMAADDR(&this->bdlist);
	STR_WRITE_4(this, BDPL, dmaaddr);
	STR_WRITE_4(this, BDPU, PTR_UPPER32(dmaaddr));
	STR_WRITE_2(this, LVI, (index - 1) & HDA_SD_LVI_LVI);
	ctl2 = STR_READ_1(this, CTL2);
	STR_WRITE_1(this, CTL2,
	    (ctl2 & ~HDA_SD_CTL2_STRM) | (this->number << HDA_SD_CTL2_STRM_SHIFT));
	STR_WRITE_4(this, CBL, ((char *)end - (char *)start));

	STR_WRITE_2(this, FMT, fmt);

	err = azalia_codec_connect_stream(&this->az->codecs[this->az->codecno],
	    this->dir, fmt, this->number);
	if (err)
		return EINVAL;

	intctl = AZ_READ_4(this->az, INTCTL);
	intctl |= this->intr_bit;
	AZ_WRITE_4(this->az, INTCTL, intctl);

	ctl = STR_READ_1(this, CTL);
	ctl |= ctl | HDA_SD_CTL_DEIE | HDA_SD_CTL_FEIE | HDA_SD_CTL_IOCE | HDA_SD_CTL_RUN;
	STR_WRITE_1(this, CTL, ctl);
	return 0;
}

static int
azalia_stream_halt(stream_t *this)
{
	uint16_t ctl;

	if (this->bdlist.addr == NULL)
		return EINVAL;
	this->intr = this->intr_arg = NULL;
	ctl = STR_READ_2(this, CTL);
	ctl &= ~(HDA_SD_CTL_DEIE | HDA_SD_CTL_FEIE | HDA_SD_CTL_IOCE | HDA_SD_CTL_RUN);
	STR_WRITE_2(this, CTL, ctl);
	AZ_WRITE_4(this->az, INTCTL, AZ_READ_4(this->az, INTCTL) & ~this->intr_bit);
	azalia_codec_disconnect_stream
	    (&this->az->codecs[this->az->codecno], this->dir);
	return 0;
}

static int
azalia_stream_intr(stream_t *this, uint32_t intsts)
{
	if (this->bdlist.addr == NULL)
		return 0;
	if ((intsts & this->intr_bit) == 0)
		return 0;
	STR_WRITE_1(this, STS, HDA_SD_STS_DESE
	    | HDA_SD_STS_FIFOE | HDA_SD_STS_BCIS);

	if (this->intr != NULL)
		this->intr(this->intr_arg);
	return 1;
}

/* ================================================================
 * MI audio entries
 * ================================================================ */

static int
azalia_open(void *v, int flags)
{
	azalia_t *az;
	codec_t *codec;

	DPRINTF(("%s: flags=0x%x\n", __func__, flags));
	az = v;
	codec = &az->codecs[az->codecno];
	if (flags & FWRITE && (az->mode_cap & AUMODE_PLAY) == 0)
		return EACCES;
	if (flags & FREAD && (az->mode_cap & AUMODE_RECORD) == 0)
		return EACCES;
	codec->running++;
	return 0;
}

static void
azalia_close(void *v)
{
	azalia_t *az;
	codec_t *codec;

	DPRINTF(("%s\n", __func__));
	az = v;
	codec = &az->codecs[az->codecno];
	codec->running--;
}

static int
azalia_query_encoding(void *v, audio_encoding_t *enc)
{
	azalia_t *az;
	codec_t *codec;

	az = v;
	codec = &az->codecs[az->codecno];
	return auconv_query_encoding(codec->encodings, enc);
}

static int
azalia_set_params(void *v, int smode, int umode, audio_params_t *p,
    audio_params_t *r, stream_filter_list_t *pfil, stream_filter_list_t *rfil)
{
	azalia_t *az;
	codec_t *codec;
	int index;

	az = v;
	codec = &az->codecs[az->codecno];
	smode &= az->mode_cap;
	if (smode & AUMODE_RECORD && r != NULL) {
		index = auconv_set_converter(codec->formats, codec->nformats,
		    AUMODE_RECORD, r, TRUE, rfil);
		if (index < 0)
			return EINVAL;
	}
	if (smode & AUMODE_PLAY && p != NULL) {
		index = auconv_set_converter(codec->formats, codec->nformats,
		    AUMODE_PLAY, p, TRUE, pfil);
		if (index < 0)
			return EINVAL;
	}
	return 0;
}

static int
azalia_round_blocksize(void *v, int blk, int mode,
    const audio_params_t *param)
{
	azalia_t *az;
	size_t size;

	blk &= ~0x7f;		/* must be multiple of 128 */
	if (blk <= 0)
		blk = 128;
	/* number of blocks must be <= HDA_BDL_MAX */
	az = v;
	size = mode == AUMODE_PLAY ? az->pstream.buffer.size : az->rstream.buffer.size;
#ifdef DIAGNOSTIC
	if (size <= 0) {
		aprint_error("%s: size is 0", __func__);
		return 256;
	}
#endif
	if (size > HDA_BDL_MAX * blk) {
		blk = size / HDA_BDL_MAX;
		if (blk & 0x7f)
			blk = (blk + 0x7f) & ~0x7f;
	}
	DPRINTF(("%s: resultant block size = %d\n", __func__, blk));
	return blk;
}

static int
azalia_halt_output(void *v)
{
	azalia_t *az;

	DPRINTF(("%s\n", __func__));
	az = v;
	return azalia_stream_halt(&az->pstream);
}

static int
azalia_halt_input(void *v)
{
	azalia_t *az;

	DPRINTF(("%s\n", __func__));
	az = v;
	return azalia_stream_halt(&az->rstream);
}

static int
azalia_getdev(void *v, struct audio_device *dev)
{
	azalia_t *az;

	az = v;
	strlcpy(dev->name, "HD-Audio", MAX_AUDIO_DEV_LEN);
	snprintf(dev->version, MAX_AUDIO_DEV_LEN,
	    "%d.%d", AZ_READ_1(az, VMAJ), AZ_READ_1(az, VMIN));
	strlcpy(dev->config, device_xname(az->dev), MAX_AUDIO_DEV_LEN);
	return 0;
}

static int
azalia_set_port(void *v, mixer_ctrl_t *mc)
{
	azalia_t *az;
	codec_t *co;

	az = v;
	co = &az->codecs[az->codecno];
	return co->set_port(co, mc);
}

static int
azalia_get_port(void *v, mixer_ctrl_t *mc)
{
	azalia_t *az;
	codec_t *co;

	az = v;
	co = &az->codecs[az->codecno];
	return co->get_port(co, mc);
}

static int
azalia_query_devinfo(void *v, mixer_devinfo_t *mdev)
{
	azalia_t *az;
	const codec_t *co;

	az = v;
	co = &az->codecs[az->codecno];
	if (mdev->index < 0 || mdev->index >= co->nmixers)
		return ENXIO;
	*mdev = co->mixers[mdev->index].devinfo;
	return 0;
}

static void *
azalia_allocm(void *v, int dir, size_t size)
{
	azalia_t *az;
	stream_t *stream;
	int err;

	az = v;
	stream = dir == AUMODE_PLAY ? &az->pstream : &az->rstream;
	err = azalia_alloc_dmamem(az, size, 128, &stream->buffer);
	if (err)
		return NULL;
	return stream->buffer.addr;
}

static void
azalia_freem(void *v, void *addr, size_t size)
{
	azalia_t *az;
	stream_t *stream;

	az = v;
	if (addr == az->pstream.buffer.addr) {
		stream = &az->pstream;
	} else if (addr == az->rstream.buffer.addr) {
		stream = &az->rstream;
	} else {
		return;
	}
	azalia_free_dmamem(az, &stream->buffer);
}

static size_t
azalia_round_buffersize(void *v, int dir, size_t size)
{
	size &= ~0x7f;		/* must be multiple of 128 */
	if (size <= 0)
		size = 128;
	return size;
}

static int
azalia_get_props(void *v)
{
	return AUDIO_PROP_INDEPENDENT | AUDIO_PROP_FULLDUPLEX;
}

static int
azalia_trigger_output(void *v, void *start, void *end, int blk,
    void (*intr)(void *), void *arg, const audio_params_t *param)
{
	azalia_t *az;
	int err;
	uint16_t fmt;

	DPRINTF(("%s: this=%p start=%p end=%p blk=%d {enc=%u %uch %u/%ubit %uHz}\n",
	    __func__, v, start, end, blk, param->encoding, param->channels,
	    param->validbits, param->precision, param->sample_rate));

	err = azalia_params2fmt(param, &fmt);
	if (err)
		return EINVAL;

	az = v;
	return azalia_stream_start(&az->pstream, start, end, blk, intr, arg, fmt);
}

static int
azalia_trigger_input(void *v, void *start, void *end, int blk,
    void (*intr)(void *), void *arg, const audio_params_t *param)
{
	azalia_t *az;
	int err;
	uint16_t fmt;

	DPRINTF(("%s: this=%p start=%p end=%p blk=%d {enc=%u %uch %u/%ubit %uHz}\n",
	    __func__, v, start, end, blk, param->encoding, param->channels,
	    param->validbits, param->precision, param->sample_rate));

	err = azalia_params2fmt(param, &fmt);
	if (err)
		return EINVAL;

	az = v;
	return azalia_stream_start(&az->rstream, start, end, blk, intr, arg, fmt);
}

/* --------------------------------
 * helpers for MI audio functions
 * -------------------------------- */
static int
azalia_params2fmt(const audio_params_t *param, uint16_t *fmt)
{
	uint16_t ret;

	ret = 0;
#ifdef DIAGNOSTIC
	if (param->channels > HDA_MAX_CHANNELS) {
		aprint_error("%s: too many channels: %u\n", __func__,
		    param->channels);
		return EINVAL;
	}
#endif
	ret |= param->channels - 1;

	switch (param->validbits) {
	case 8:
		ret |= HDA_SD_FMT_BITS_8_16;
		break;
	case 16:
		ret |= HDA_SD_FMT_BITS_16_16;
		break;
	case 20:
		ret |= HDA_SD_FMT_BITS_20_32;
		break;
	case 24:
		ret |= HDA_SD_FMT_BITS_24_32;
		break;
	case 32:
		ret |= HDA_SD_FMT_BITS_32_32;
		break;
	default:
		aprint_error("%s: invalid validbits: %u\n", __func__,
		    param->validbits);
	}

	if (param->sample_rate == 384000) {
		aprint_error("%s: invalid sample_rate: %u\n", __func__,
		    param->sample_rate);
		return EINVAL;
	} else if (param->sample_rate == 192000) {
		ret |= HDA_SD_FMT_BASE_48 | HDA_SD_FMT_MULT_X4 | HDA_SD_FMT_DIV_BY1;
	} else if (param->sample_rate == 176400) {
		ret |= HDA_SD_FMT_BASE_44 | HDA_SD_FMT_MULT_X4 | HDA_SD_FMT_DIV_BY1;
	} else if (param->sample_rate == 96000) {
		ret |= HDA_SD_FMT_BASE_48 | HDA_SD_FMT_MULT_X2 | HDA_SD_FMT_DIV_BY1;
	} else if (param->sample_rate == 88200) {
		ret |= HDA_SD_FMT_BASE_44 | HDA_SD_FMT_MULT_X2 | HDA_SD_FMT_DIV_BY1;
	} else if (param->sample_rate == 48000) {
		ret |= HDA_SD_FMT_BASE_48 | HDA_SD_FMT_MULT_X1 | HDA_SD_FMT_DIV_BY1;
	} else if (param->sample_rate == 44100) {
		ret |= HDA_SD_FMT_BASE_44 | HDA_SD_FMT_MULT_X1 | HDA_SD_FMT_DIV_BY1;
	} else if (param->sample_rate == 32000) {
		ret |= HDA_SD_FMT_BASE_48 | HDA_SD_FMT_MULT_X2 | HDA_SD_FMT_DIV_BY3;
	} else if (param->sample_rate == 22050) {
		ret |= HDA_SD_FMT_BASE_44 | HDA_SD_FMT_MULT_X1 | HDA_SD_FMT_DIV_BY2;
	} else if (param->sample_rate == 16000) {
		ret |= HDA_SD_FMT_BASE_48 | HDA_SD_FMT_MULT_X1 | HDA_SD_FMT_DIV_BY3;
	} else if (param->sample_rate == 11025) {
		ret |= HDA_SD_FMT_BASE_44 | HDA_SD_FMT_MULT_X1 | HDA_SD_FMT_DIV_BY4;
	} else if (param->sample_rate == 8000) {
		ret |= HDA_SD_FMT_BASE_48 | HDA_SD_FMT_MULT_X1 | HDA_SD_FMT_DIV_BY6;
	} else {
		aprint_error("%s: invalid sample_rate: %u\n", __func__,
		    param->sample_rate);
		return EINVAL;
	}
	*fmt = ret;
	return 0;
}

MODULE(MODULE_CLASS_DRIVER, azalia, "pci");

static void
azalia_get_locks(void *addr, kmutex_t **intr, kmutex_t **thread)
{
	azalia_t *az;

	az = addr;
	*intr = &az->intr_lock;
	*thread = &az->lock;
}

#ifdef _MODULE
#include "ioconf.c"
#endif

static int
azalia_modcmd(modcmd_t cmd, void *arg)
{
	int error = 0;

	switch (cmd) {
	case MODULE_CMD_INIT:
#ifdef _MODULE
		error = config_init_component(cfdriver_ioconf_azalia,
		    cfattach_ioconf_azalia, cfdata_ioconf_azalia);
#endif
		break;
	case MODULE_CMD_FINI:
#ifdef _MODULE
		error = config_fini_component(cfdriver_ioconf_azalia,
		    cfattach_ioconf_azalia, cfdata_ioconf_azalia);
#endif
		break;
	default:
		return ENOTTY;
	}

	return error;
}
