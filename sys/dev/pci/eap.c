/*	$NetBSD: eap.c,v 1.95 2014/03/29 19:28:24 christos Exp $	*/
/*      $OpenBSD: eap.c,v 1.6 1999/10/05 19:24:42 csapuntz Exp $ */

/*
 * Copyright (c) 1998, 1999, 2002, 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson <augustss@NetBSD.org>, Charles M. Hannum,
 * Antti Kantee <pooka@NetBSD.org>, and Andrew Doran.
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
 * Debugging:   Andreas Gustafsson <gson@araneus.fi>
 * Testing:     Chuck Cranor       <chuck@maria.wustl.edu>
 *              Phil Nelson        <phil@cs.wwu.edu>
 *
 * ES1371/AC97:	Ezra Story         <ezy@panix.com>
 */

/*
 * Ensoniq ES1370 + AK4531 and ES1371/ES1373 + AC97
 *
 * Documentation links:
 *
 * ftp://ftp.alsa-project.org/pub/manuals/ensoniq/ (ES1370 and 1371 datasheets)
 * http://web.archive.org/web/20040622012936/http://www.corbac.com/Data/Misc/es1373.ps.gz
 * ftp://ftp.alsa-project.org/pub/manuals/asahi_kasei/4531.pdf
 * ftp://download.intel.com/ial/scalableplatforms/audio/ac97r21.pdf
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: eap.c,v 1.95 2014/03/29 19:28:24 christos Exp $");

#include "midi.h"
#include "joy_eap.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/fcntl.h>
#include <sys/kmem.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/select.h>
#include <sys/mutex.h>
#include <sys/bus.h>
#include <sys/audioio.h>

#include <dev/audio_if.h>
#include <dev/midi_if.h>
#include <dev/audiovar.h>
#include <dev/mulaw.h>
#include <dev/auconv.h>

#include <dev/pci/pcidevs.h>
#include <dev/pci/eapreg.h>
#include <dev/pci/eapvar.h>

#define	PCI_CBIO		0x10

/* Debug */
#ifdef AUDIO_DEBUG
#define DPRINTF(x)	if (eapdebug) printf x
#define DPRINTFN(n,x)	if (eapdebug>(n)) printf x
int	eapdebug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

static int	eap_match(device_t, cfdata_t, void *);
static void	eap_attach(device_t, device_t, void *);
static int	eap_detach(device_t, int);
static int	eap_intr(void *);

static int	eap_allocmem(struct eap_softc *, size_t, size_t,
			     struct eap_dma *);
static int	eap_freemem(struct eap_softc *, struct eap_dma *);

#define EWRITE1(sc, r, x) bus_space_write_1((sc)->iot, (sc)->ioh, (r), (x))
#define EWRITE2(sc, r, x) bus_space_write_2((sc)->iot, (sc)->ioh, (r), (x))
#define EWRITE4(sc, r, x) bus_space_write_4((sc)->iot, (sc)->ioh, (r), (x))
#define EREAD1(sc, r) bus_space_read_1((sc)->iot, (sc)->ioh, (r))
#define EREAD2(sc, r) bus_space_read_2((sc)->iot, (sc)->ioh, (r))
#define EREAD4(sc, r) bus_space_read_4((sc)->iot, (sc)->ioh, (r))

CFATTACH_DECL_NEW(eap, sizeof(struct eap_softc),
    eap_match, eap_attach, eap_detach, NULL);

static int	eap_open(void *, int);
static int	eap_query_encoding(void *, struct audio_encoding *);
static int	eap_set_params(void *, int, int, audio_params_t *,
			       audio_params_t *, stream_filter_list_t *,
			       stream_filter_list_t *);
static int	eap_round_blocksize(void *, int, int, const audio_params_t *);
static int	eap_trigger_output(void *, void *, void *, int,
				   void (*)(void *), void *,
				   const audio_params_t *);
static int	eap_trigger_input(void *, void *, void *, int,
				  void (*)(void *), void *,
				  const audio_params_t *);
static int	eap_halt_output(void *);
static int	eap_halt_input(void *);
static void	eap1370_write_codec(struct eap_softc *, int, int);
static int	eap_getdev(void *, struct audio_device *);
static int	eap1370_mixer_set_port(void *, mixer_ctrl_t *);
static int	eap1370_mixer_get_port(void *, mixer_ctrl_t *);
static int	eap1371_mixer_set_port(void *, mixer_ctrl_t *);
static int	eap1371_mixer_get_port(void *, mixer_ctrl_t *);
static int	eap1370_query_devinfo(void *, mixer_devinfo_t *);
static void	*eap_malloc(void *, int, size_t);
static void	eap_free(void *, void *, size_t);
static size_t	eap_round_buffersize(void *, int, size_t);
static paddr_t	eap_mappage(void *, void *, off_t, int);
static int	eap_get_props(void *);
static void	eap1370_set_mixer(struct eap_softc *, int, int);
static uint32_t eap1371_src_wait(struct eap_softc *);
static void	eap1371_set_adc_rate(struct eap_softc *, int);
static void	eap1371_set_dac_rate(struct eap_instance *, int);
static int	eap1371_src_read(struct eap_softc *, int);
static void	eap1371_src_write(struct eap_softc *, int, int);
static int	eap1371_query_devinfo(void *, mixer_devinfo_t *);

static int	eap1371_attach_codec(void *, struct ac97_codec_if *);
static int	eap1371_read_codec(void *, uint8_t, uint16_t *);
static int	eap1371_write_codec(void *, uint8_t, uint16_t );
static int	eap1371_reset_codec(void *);
static void	eap_get_locks(void *, kmutex_t **, kmutex_t **);

#if NMIDI > 0
static void	eap_midi_close(void *);
static void	eap_midi_getinfo(void *, struct midi_info *);
static int	eap_midi_open(void *, int, void (*)(void *, int),
			      void (*)(void *), void *);
static int	eap_midi_output(void *, int);
static void	eap_uart_txrdy(struct eap_softc *);
#endif

static const struct audio_hw_if eap1370_hw_if = {
	eap_open,
	NULL,			/* close */
	NULL,
	eap_query_encoding,
	eap_set_params,
	eap_round_blocksize,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	eap_halt_output,
	eap_halt_input,
	NULL,
	eap_getdev,
	NULL,
	eap1370_mixer_set_port,
	eap1370_mixer_get_port,
	eap1370_query_devinfo,
	eap_malloc,
	eap_free,
	eap_round_buffersize,
	eap_mappage,
	eap_get_props,
	eap_trigger_output,
	eap_trigger_input,
	NULL,
	eap_get_locks,
};

static const struct audio_hw_if eap1371_hw_if = {
	eap_open,
	NULL,			/* close */
	NULL,
	eap_query_encoding,
	eap_set_params,
	eap_round_blocksize,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	eap_halt_output,
	eap_halt_input,
	NULL,
	eap_getdev,
	NULL,
	eap1371_mixer_set_port,
	eap1371_mixer_get_port,
	eap1371_query_devinfo,
	eap_malloc,
	eap_free,
	eap_round_buffersize,
	eap_mappage,
	eap_get_props,
	eap_trigger_output,
	eap_trigger_input,
	NULL,
	eap_get_locks,
};

#if NMIDI > 0
static const struct midi_hw_if eap_midi_hw_if = {
	eap_midi_open,
	eap_midi_close,
	eap_midi_output,
	eap_midi_getinfo,
	0,				/* ioctl */
	eap_get_locks,
};
#endif

static struct audio_device eap_device = {
	"Ensoniq AudioPCI",
	"",
	"eap"
};

#define EAP_NFORMATS	4
static const struct audio_format eap_formats[EAP_NFORMATS] = {
	{NULL, AUMODE_PLAY | AUMODE_RECORD, AUDIO_ENCODING_SLINEAR_LE, 16, 16,
	 2, AUFMT_STEREO, 0, {4000, 48000}},
	{NULL, AUMODE_PLAY | AUMODE_RECORD, AUDIO_ENCODING_SLINEAR_LE, 16, 16,
	 1, AUFMT_MONAURAL, 0, {4000, 48000}},
	{NULL, AUMODE_PLAY | AUMODE_RECORD, AUDIO_ENCODING_ULINEAR_LE, 8, 8,
	 2, AUFMT_STEREO, 0, {4000, 48000}},
	{NULL, AUMODE_PLAY | AUMODE_RECORD, AUDIO_ENCODING_ULINEAR_LE, 8, 8,
	 1, AUFMT_MONAURAL, 0, {4000, 48000}},
};

static int
eap_match(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa;

	pa = (struct pci_attach_args *)aux;
	switch (PCI_VENDOR(pa->pa_id)) {
	case PCI_VENDOR_CREATIVELABS:
		switch (PCI_PRODUCT(pa->pa_id)) {
		case PCI_PRODUCT_CREATIVELABS_EV1938:
			return 1;
		}
		break;
	case PCI_VENDOR_ENSONIQ:
		switch (PCI_PRODUCT(pa->pa_id)) {
		case PCI_PRODUCT_ENSONIQ_AUDIOPCI:
		case PCI_PRODUCT_ENSONIQ_AUDIOPCI97:
		case PCI_PRODUCT_ENSONIQ_CT5880:
			return 1;
		}
		break;
	}

	return 0;
}

static void
eap1370_write_codec(struct eap_softc *sc, int a, int d)
{
	int icss, to;

	to = EAP_WRITE_TIMEOUT;
	do {
		icss = EREAD4(sc, EAP_ICSS);
		DPRINTFN(5,("eap: codec %d prog: icss=0x%08x\n", a, icss));
		if (!to--) {
			printf("eap: timeout writing to codec\n");
			return;
		}
	} while(icss & EAP_CWRIP);  /* XXX could use CSTAT here */
	EWRITE4(sc, EAP_CODEC, EAP_SET_CODEC(a, d));
}

/*
 * Reading and writing the CODEC is very convoluted.  This mimics the
 * FreeBSD and Linux drivers.
 */

static inline void
eap1371_ready_codec(struct eap_softc *sc, uint8_t a, uint32_t wd)
{
	int to;
	uint32_t src, t;

	for (to = 0; to < EAP_WRITE_TIMEOUT; to++) {
		if (!(EREAD4(sc, E1371_CODEC) & E1371_CODEC_WIP))
			break;
		delay(1);
	}
	if (to >= EAP_WRITE_TIMEOUT)
		aprint_error_dev(sc->sc_dev,
		    "eap1371_ready_codec timeout 1\n");

	mutex_spin_enter(&sc->sc_intr_lock);
	src = eap1371_src_wait(sc) & E1371_SRC_CTLMASK;
	EWRITE4(sc, E1371_SRC, src | E1371_SRC_STATE_OK);

	for (to = 0; to < EAP_READ_TIMEOUT; to++) {
		t = EREAD4(sc, E1371_SRC);
		if ((t & E1371_SRC_STATE_MASK) == 0)
			break;
		delay(1);
	}
	if (to >= EAP_READ_TIMEOUT)
		aprint_error_dev(sc->sc_dev,
		    "eap1371_ready_codec timeout 2\n");

	for (to = 0; to < EAP_READ_TIMEOUT; to++) {
		t = EREAD4(sc, E1371_SRC);
		if ((t & E1371_SRC_STATE_MASK) == E1371_SRC_STATE_OK)
			break;
		delay(1);
	}
	if (to >= EAP_READ_TIMEOUT)
		aprint_error_dev(sc->sc_dev,
		    "eap1371_ready_codec timeout 3\n");

	EWRITE4(sc, E1371_CODEC, wd);

	eap1371_src_wait(sc);
	EWRITE4(sc, E1371_SRC, src);

	mutex_spin_exit(&sc->sc_intr_lock);
}

static int
eap1371_read_codec(void *sc_, uint8_t a, uint16_t *d)
{
	struct eap_softc *sc;
	int to;
	uint32_t t;

	sc = sc_;
	eap1371_ready_codec(sc, a, E1371_SET_CODEC(a, 0) | E1371_CODEC_READ);

	for (to = 0; to < EAP_WRITE_TIMEOUT; to++) {
		if (!(EREAD4(sc, E1371_CODEC) & E1371_CODEC_WIP))
			break;
	}
	if (to > EAP_WRITE_TIMEOUT)
		aprint_error_dev(sc->sc_dev,
		    "eap1371_read_codec timeout 1\n");

	for (to = 0; to < EAP_WRITE_TIMEOUT; to++) {
		t = EREAD4(sc, E1371_CODEC);
		if (t & E1371_CODEC_VALID)
			break;
	}
	if (to > EAP_WRITE_TIMEOUT)
		aprint_error_dev(sc->sc_dev, "eap1371_read_codec timeout 2\n");

	*d = (uint16_t)t;

	DPRINTFN(10, ("eap1371: reading codec (%x) = %x\n", a, *d));

	return 0;
}

static int
eap1371_write_codec(void *sc_, uint8_t a, uint16_t d)
{
	struct eap_softc *sc;

	sc = sc_;
	eap1371_ready_codec(sc, a, E1371_SET_CODEC(a, d));

	DPRINTFN(10, ("eap1371: writing codec %x --> %x\n", d, a));

	return 0;
}

static uint32_t
eap1371_src_wait(struct eap_softc *sc)
{
	int to;
	u_int32_t src;

	for (to = 0; to < EAP_READ_TIMEOUT; to++) {
		src = EREAD4(sc, E1371_SRC);
		if (!(src & E1371_SRC_RBUSY))
			return src;
		delay(1);
	}
	aprint_error_dev(sc->sc_dev, "eap1371_src_wait timeout\n");
	return src;
}

static int
eap1371_src_read(struct eap_softc *sc, int a)
{
	int to;
	uint32_t src, t;

	src = eap1371_src_wait(sc) & E1371_SRC_CTLMASK;
	src |= E1371_SRC_ADDR(a);
	EWRITE4(sc, E1371_SRC, src | E1371_SRC_STATE_OK);

	t = eap1371_src_wait(sc);
	if ((t & E1371_SRC_STATE_MASK) != E1371_SRC_STATE_OK) {
		for (to = 0; to < EAP_READ_TIMEOUT; to++) {
			t = EREAD4(sc, E1371_SRC);
			if ((t & E1371_SRC_STATE_MASK) == E1371_SRC_STATE_OK)
				break;
			delay(1);
		}
	}

	EWRITE4(sc, E1371_SRC, src);

	return t & E1371_SRC_DATAMASK;
}

static void
eap1371_src_write(struct eap_softc *sc, int a, int d)
{
	uint32_t r;

	r = eap1371_src_wait(sc) & E1371_SRC_CTLMASK;
	r |= E1371_SRC_RAMWE | E1371_SRC_ADDR(a) | E1371_SRC_DATA(d);
	EWRITE4(sc, E1371_SRC, r);
}

static void
eap1371_set_adc_rate(struct eap_softc *sc, int rate)
{
	int freq, n, truncm;
	int out;

	/* Whatever, it works, so I'll leave it :) */

	if (rate > 48000)
		rate = 48000;
	if (rate < 4000)
		rate = 4000;
	n = rate / 3000;
	if ((1 << n) & SRC_MAGIC)
		n--;
	truncm = ((21 * n) - 1) | 1;
	freq = ((48000 << 15) / rate) * n;
	if (rate >= 24000) {
		if (truncm > 239)
			truncm = 239;
		out = ESRC_SET_TRUNC((239 - truncm) / 2);
	} else {
		if (truncm > 119)
			truncm = 119;
		out = ESRC_SMF | ESRC_SET_TRUNC((119 - truncm) / 2);
	}
	out |= ESRC_SET_N(n);
	mutex_spin_enter(&sc->sc_intr_lock);
	eap1371_src_write(sc, ESRC_ADC+ESRC_TRUNC_N, out);

	out = eap1371_src_read(sc, ESRC_ADC+ESRC_IREGS) & 0xff;
	eap1371_src_write(sc, ESRC_ADC+ESRC_IREGS, out |
			  ESRC_SET_VFI(freq >> 15));
	eap1371_src_write(sc, ESRC_ADC+ESRC_VFF, freq & 0x7fff);
	eap1371_src_write(sc, ESRC_ADC_VOLL, ESRC_SET_ADC_VOL(n));
	eap1371_src_write(sc, ESRC_ADC_VOLR, ESRC_SET_ADC_VOL(n));
	mutex_spin_exit(&sc->sc_intr_lock);
}

static void
eap1371_set_dac_rate(struct eap_instance *ei, int rate)
{
	struct eap_softc *sc;
	int dac;
	int freq, r;

	DPRINTFN(2, ("eap1371_set_dac_date: set rate for %d\n", ei->index));
	sc = device_private(ei->parent);
	dac = ei->index == EAP_DAC1 ? ESRC_DAC1 : ESRC_DAC2;

	/* Whatever, it works, so I'll leave it :) */

	if (rate > 48000)
	    rate = 48000;
	if (rate < 4000)
	    rate = 4000;
	freq = ((rate << 15) + 1500) / 3000;

	mutex_spin_enter(&sc->sc_intr_lock);
	eap1371_src_wait(sc);
	r = EREAD4(sc, E1371_SRC) & (E1371_SRC_DISABLE |
	    E1371_SRC_DISP2 | E1371_SRC_DISP1 | E1371_SRC_DISREC);
	r |= ei->index == EAP_DAC1 ? E1371_SRC_DISP1 : E1371_SRC_DISP2;
	EWRITE4(sc, E1371_SRC, r);
	r = eap1371_src_read(sc, dac + ESRC_IREGS) & 0x00ff;
	eap1371_src_write(sc, dac + ESRC_IREGS, r | ((freq >> 5) & 0xfc00));
	eap1371_src_write(sc, dac + ESRC_VFF, freq & 0x7fff);
	r = EREAD4(sc, E1371_SRC) & (E1371_SRC_DISABLE |
	    E1371_SRC_DISP2 | E1371_SRC_DISP1 | E1371_SRC_DISREC);
	r &= ~(ei->index == EAP_DAC1 ? E1371_SRC_DISP1 : E1371_SRC_DISP2);
	EWRITE4(sc, E1371_SRC, r);
	mutex_spin_exit(&sc->sc_intr_lock);
}

static void
eap_attach(device_t parent, device_t self, void *aux)
{
	struct eap_softc *sc;
	struct pci_attach_args *pa;
	pci_chipset_tag_t pc;
	const struct audio_hw_if *eap_hw_if;
	char const *intrstr;
	pci_intr_handle_t ih;
	pcireg_t csr;
	char devinfo[256];
	mixer_ctrl_t ctl;
	int i;
	int revision, ct5880;
	const char *revstr;
#if NJOY_EAP > 0
	struct eap_gameport_args gpargs;
#endif
	char intrbuf[PCI_INTRSTR_LEN];

	sc = device_private(self);
	sc->sc_dev = self;
	pa = (struct pci_attach_args *)aux;
	pc = pa->pa_pc;
	revstr = "";
	aprint_naive(": Audio controller\n");

	mutex_init(&sc->sc_lock, MUTEX_DEFAULT, IPL_NONE);
	mutex_init(&sc->sc_intr_lock, MUTEX_DEFAULT, IPL_AUDIO);

	/* Stash this away for detach */
	sc->sc_pc = pc;

	/* Flag if we're "creative" */
	sc->sc_1371 = !(PCI_VENDOR(pa->pa_id) == PCI_VENDOR_ENSONIQ &&
			PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_ENSONIQ_AUDIOPCI);

	/*
	 * The vendor and product ID's are quite "interesting". Just
	 * trust the following and be happy.
	 */
	pci_devinfo(pa->pa_id, pa->pa_class, 0, devinfo, sizeof(devinfo));
	revision = PCI_REVISION(pa->pa_class);
	ct5880 = 0;
	if (sc->sc_1371) {
		if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_ENSONIQ &&
		    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_ENSONIQ_CT5880) {
			ct5880 = 1;
			switch (revision) {
			case EAP_CT5880_C: revstr = "CT5880-C "; break;
			case EAP_CT5880_D: revstr = "CT5880-D "; break;
			case EAP_CT5880_E: revstr = "CT5880-E "; break;
			}
		} else {
			switch (revision) {
			case EAP_EV1938_A: revstr = "EV1938-A "; break;
			case EAP_ES1373_A: revstr = "ES1373-A "; break;
			case EAP_ES1373_B: revstr = "ES1373-B "; break;
			case EAP_CT5880_A: revstr = "CT5880-A "; ct5880=1;break;
			case EAP_ES1373_8: revstr = "ES1373-8" ; ct5880=1;break;
			case EAP_ES1371_B: revstr = "ES1371-B "; break;
			}
		}
	}
	aprint_normal(": %s %s(rev. 0x%02x)\n", devinfo, revstr, revision);

	/* Map I/O register */
	if (pci_mapreg_map(pa, PCI_CBIO, PCI_MAPREG_TYPE_IO, 0,
	      &sc->iot, &sc->ioh, NULL, &sc->iosz)) {
		aprint_error_dev(sc->sc_dev, "can't map i/o space\n");
		return;
	}

	sc->sc_dmatag = pa->pa_dmat;

	/* Enable the device. */
	csr = pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	pci_conf_write(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG,
		       csr | PCI_COMMAND_MASTER_ENABLE);

	/* Map and establish the interrupt. */
	if (pci_intr_map(pa, &ih)) {
		aprint_error_dev(sc->sc_dev, "couldn't map interrupt\n");
		return;
	}
	intrstr = pci_intr_string(pc, ih, intrbuf, sizeof(intrbuf));
	sc->sc_ih = pci_intr_establish(pc, ih, IPL_AUDIO, eap_intr, sc);
	if (sc->sc_ih == NULL) {
		aprint_error_dev(sc->sc_dev, "couldn't establish interrupt");
		if (intrstr != NULL)
			aprint_error(" at %s", intrstr);
		aprint_error("\n");
		return;
	}
	aprint_normal_dev(self, "interrupting at %s\n", intrstr);

	sc->sc_ei[EAP_I1].parent = self;
	sc->sc_ei[EAP_I1].index = EAP_DAC2;
	sc->sc_ei[EAP_I2].parent = self;
	sc->sc_ei[EAP_I2].index = EAP_DAC1;

	if (!sc->sc_1371) {
		/* Enable interrupts and looping mode. */
		/* enable the parts we need */
		EWRITE4(sc, EAP_SIC, EAP_P2_INTR_EN | EAP_R1_INTR_EN);
		EWRITE4(sc, EAP_ICSC, EAP_CDC_EN);

		/* reset codec */
		/* normal operation */
		/* select codec clocks */
		eap1370_write_codec(sc, AK_RESET, AK_PD);
		eap1370_write_codec(sc, AK_RESET, AK_PD | AK_NRST);
		eap1370_write_codec(sc, AK_CS, 0x0);

		eap_hw_if = &eap1370_hw_if;

		/* Enable all relevant mixer switches. */
		ctl.dev = EAP_INPUT_SOURCE;
		ctl.type = AUDIO_MIXER_SET;
		ctl.un.mask = 1 << EAP_VOICE_VOL | 1 << EAP_FM_VOL |
			1 << EAP_CD_VOL | 1 << EAP_LINE_VOL | 1 << EAP_AUX_VOL |
			1 << EAP_MIC_VOL;
		eap_hw_if->set_port(&sc->sc_ei[EAP_I1], &ctl);

		ctl.type = AUDIO_MIXER_VALUE;
		ctl.un.value.num_channels = 1;
		for (ctl.dev = EAP_MASTER_VOL; ctl.dev < EAP_MIC_VOL;
		     ctl.dev++) {
			ctl.un.value.level[AUDIO_MIXER_LEVEL_MONO] = VOL_0DB;
			eap_hw_if->set_port(&sc->sc_ei[EAP_I1], &ctl);
		}
		ctl.un.value.level[AUDIO_MIXER_LEVEL_MONO] = 0;
		eap_hw_if->set_port(&sc->sc_ei[EAP_I1], &ctl);
		ctl.dev = EAP_MIC_PREAMP;
		ctl.type = AUDIO_MIXER_ENUM;
		ctl.un.ord = 0;
		eap_hw_if->set_port(&sc->sc_ei[EAP_I1], &ctl);
		ctl.dev = EAP_RECORD_SOURCE;
		ctl.type = AUDIO_MIXER_SET;
		ctl.un.mask = 1 << EAP_MIC_VOL;
		eap_hw_if->set_port(&sc->sc_ei[EAP_I1], &ctl);
	} else {
		/* clean slate */

		EWRITE4(sc, EAP_SIC, 0);
		EWRITE4(sc, EAP_ICSC, 0);
		EWRITE4(sc, E1371_LEGACY, 0);

		if (ct5880) {
			EWRITE4(sc, EAP_ICSS, EAP_CT5880_AC97_RESET);
			/* Let codec wake up */
			delay(20000);
		}

		/* Reset from es1371's perspective */
		EWRITE4(sc, EAP_ICSC, E1371_SYNC_RES);
		delay(20);
		EWRITE4(sc, EAP_ICSC, 0);

		/*
		 * Must properly reprogram sample rate converter,
		 * or it locks up.  Set some defaults for the life of the
		 * machine, and set up a sb default sample rate.
		 */
		EWRITE4(sc, E1371_SRC, E1371_SRC_DISABLE);
		for (i = 0; i < 0x80; i++)
			eap1371_src_write(sc, i, 0);
		eap1371_src_write(sc, ESRC_DAC1+ESRC_TRUNC_N, ESRC_SET_N(16));
		eap1371_src_write(sc, ESRC_DAC2+ESRC_TRUNC_N, ESRC_SET_N(16));
		eap1371_src_write(sc, ESRC_DAC1+ESRC_IREGS, ESRC_SET_VFI(16));
		eap1371_src_write(sc, ESRC_DAC2+ESRC_IREGS, ESRC_SET_VFI(16));
		eap1371_src_write(sc, ESRC_ADC_VOLL, ESRC_SET_ADC_VOL(16));
		eap1371_src_write(sc, ESRC_ADC_VOLR, ESRC_SET_ADC_VOL(16));
		eap1371_src_write(sc, ESRC_DAC1_VOLL, ESRC_SET_DAC_VOLI(1));
		eap1371_src_write(sc, ESRC_DAC1_VOLR, ESRC_SET_DAC_VOLI(1));
		eap1371_src_write(sc, ESRC_DAC2_VOLL, ESRC_SET_DAC_VOLI(1));
		eap1371_src_write(sc, ESRC_DAC2_VOLR, ESRC_SET_DAC_VOLI(1));
		eap1371_set_adc_rate(sc, 22050);
		eap1371_set_dac_rate(&sc->sc_ei[0], 22050);
		eap1371_set_dac_rate(&sc->sc_ei[1], 22050);

		EWRITE4(sc, E1371_SRC, 0);

		/* Reset codec */

		/* Interrupt enable */
		sc->host_if.arg = sc;
		sc->host_if.attach = eap1371_attach_codec;
		sc->host_if.read = eap1371_read_codec;
		sc->host_if.write = eap1371_write_codec;
		sc->host_if.reset = eap1371_reset_codec;

		if (ac97_attach(&sc->host_if, self, &sc->sc_lock) == 0) {
			/* Interrupt enable */
			EWRITE4(sc, EAP_SIC, EAP_P2_INTR_EN | EAP_R1_INTR_EN);
		} else
			return;

		eap_hw_if = &eap1371_hw_if;
	}

	sc->sc_ei[EAP_I1].ei_audiodev =
	    audio_attach_mi(eap_hw_if, &sc->sc_ei[EAP_I1], sc->sc_dev);

#ifdef EAP_USE_BOTH_DACS
	aprint_normal_dev(self, "attaching secondary DAC\n");
	sc->sc_ei[EAP_I2].ei_audiodev =
	    audio_attach_mi(eap_hw_if, &sc->sc_ei[EAP_I2], sc->sc_dev);
#endif

#if NMIDI > 0
	sc->sc_mididev = midi_attach_mi(&eap_midi_hw_if, sc, sc->sc_dev);
#endif

#if NJOY_EAP > 0
	if (sc->sc_1371) {
		gpargs.gpa_iot = sc->iot;
		gpargs.gpa_ioh = sc->ioh;
		sc->sc_gameport = eap_joy_attach(sc->sc_dev, &gpargs);
	}
#endif
}

static int
eap_detach(device_t self, int flags)
{
	struct eap_softc *sc;
	int res;
#if NJOY_EAP > 0
	struct eap_gameport_args gpargs;

	sc = device_private(self);
	if (sc->sc_gameport) {
		gpargs.gpa_iot = sc->iot;
		gpargs.gpa_ioh = sc->ioh;
		res = eap_joy_detach(sc->sc_gameport, &gpargs);
		if (res)
			return res;
	}
#else
	sc = device_private(self);
#endif
#if NMIDI > 0
	if (sc->sc_mididev != NULL) {
		res = config_detach(sc->sc_mididev, 0);
		if (res)
			return res;
	}
#endif
#ifdef EAP_USE_BOTH_DACS
	if (sc->sc_ei[EAP_I2].ei_audiodev != NULL) {
		res = config_detach(sc->sc_ei[EAP_I2].ei_audiodev, 0);
		if (res)
			return res;
	}
#endif
	if (sc->sc_ei[EAP_I1].ei_audiodev != NULL) {
		res = config_detach(sc->sc_ei[EAP_I1].ei_audiodev, 0);
		if (res)
			return res;
	}

	bus_space_unmap(sc->iot, sc->ioh, sc->iosz);
	pci_intr_disestablish(sc->sc_pc, sc->sc_ih);
	mutex_destroy(&sc->sc_lock);
	mutex_destroy(&sc->sc_intr_lock);

	return 0;
}

static int
eap1371_attach_codec(void *sc_, struct ac97_codec_if *codec_if)
{
	struct eap_softc *sc;

	sc = sc_;
	sc->codec_if = codec_if;
	return 0;
}

static int
eap1371_reset_codec(void *sc_)
{
	struct eap_softc *sc;
	uint32_t icsc;

	sc = sc_;
	mutex_spin_enter(&sc->sc_intr_lock);
	icsc = EREAD4(sc, EAP_ICSC);
	EWRITE4(sc, EAP_ICSC, icsc | E1371_SYNC_RES);
	delay(20);
	EWRITE4(sc, EAP_ICSC, icsc & ~E1371_SYNC_RES);
	delay(1);
	mutex_spin_exit(&sc->sc_intr_lock);

	return 0;
}

static int
eap_intr(void *p)
{
	struct eap_softc *sc;
	uint32_t intr, sic;

	sc = p;
	mutex_spin_enter(&sc->sc_intr_lock);
	intr = EREAD4(sc, EAP_ICSS);
	if (!(intr & EAP_INTR)) {
		mutex_spin_exit(&sc->sc_intr_lock);
		return 0;
	}
	sic = EREAD4(sc, EAP_SIC);
	DPRINTFN(5, ("eap_intr: ICSS=0x%08x, SIC=0x%08x\n", intr, sic));
	if (intr & EAP_I_ADC) {
#if 0
		/*
		 * XXX This is a hack!
		 * The EAP chip sometimes generates the recording interrupt
		 * while it is still transferring the data.  To make sure
		 * it has all arrived we busy wait until the count is right.
		 * The transfer we are waiting for is 8 longwords.
		 */
		int s, nw, n;
		EWRITE4(sc, EAP_MEMPAGE, EAP_ADC_PAGE);
		s = EREAD4(sc, EAP_ADC_CSR);
		nw = ((s & 0xffff) + 1) >> 2; /* # of words in DMA */
		n = 0;
		while (((EREAD4(sc, EAP_ADC_SIZE) >> 16) + 8) % nw == 0) {
			delay(10);
			if (++n > 100) {
				printf("eapintr: DMA fix timeout");
				break;
			}
		}
		/* Continue with normal interrupt handling. */
#endif
		EWRITE4(sc, EAP_SIC, sic & ~EAP_R1_INTR_EN);
		EWRITE4(sc, EAP_SIC, sic | EAP_R1_INTR_EN);
		if (sc->sc_rintr)
			sc->sc_rintr(sc->sc_rarg);
	}

	if (intr & EAP_I_DAC2) {
		EWRITE4(sc, EAP_SIC, sic & ~EAP_P2_INTR_EN);
		EWRITE4(sc, EAP_SIC, sic | EAP_P2_INTR_EN);
		if (sc->sc_ei[EAP_DAC2].ei_pintr)
			sc->sc_ei[EAP_DAC2].ei_pintr(sc->sc_ei[EAP_DAC2].ei_parg);
	}

	if (intr & EAP_I_DAC1) {
		EWRITE4(sc, EAP_SIC, sic & ~EAP_P1_INTR_EN);
		EWRITE4(sc, EAP_SIC, sic | EAP_P1_INTR_EN);
		if (sc->sc_ei[EAP_DAC1].ei_pintr)
			sc->sc_ei[EAP_DAC1].ei_pintr(sc->sc_ei[EAP_DAC1].ei_parg);
	}

	if (intr & EAP_I_MCCB)
		panic("eap_intr: unexpected MCCB interrupt");
#if NMIDI > 0
	if (intr & EAP_I_UART) {
		uint8_t ustat;
		uint32_t data;
		
		ustat = EREAD1(sc, EAP_UART_STATUS);

		if (ustat & EAP_US_RXINT) {
			while (EREAD1(sc, EAP_UART_STATUS) & EAP_US_RXRDY) {
				data = EREAD1(sc, EAP_UART_DATA);
				sc->sc_iintr(sc->sc_arg, data);
			}
		}
		
		if (ustat & EAP_US_TXINT)
			eap_uart_txrdy(sc);
	}
#endif
	mutex_spin_exit(&sc->sc_intr_lock);
	return 1;
}

static int
eap_allocmem(struct eap_softc *sc, size_t size, size_t align, struct eap_dma *p)
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
	return (0);

destroy:
	bus_dmamap_destroy(sc->sc_dmatag, p->map);
unmap:
	bus_dmamem_unmap(sc->sc_dmatag, p->addr, p->size);
free:
	bus_dmamem_free(sc->sc_dmatag, p->segs, p->nsegs);
	return error;
}

static int
eap_freemem(struct eap_softc *sc, struct eap_dma *p)
{

	bus_dmamap_unload(sc->sc_dmatag, p->map);
	bus_dmamap_destroy(sc->sc_dmatag, p->map);
	bus_dmamem_unmap(sc->sc_dmatag, p->addr, p->size);
	bus_dmamem_free(sc->sc_dmatag, p->segs, p->nsegs);
	return 0;
}

static int
eap_open(void *addr, int flags)
{
	struct eap_instance *ei;

	ei = addr;
	/* there is only one ADC */
	if (ei->index == EAP_I2 && flags & FREAD)
		return EOPNOTSUPP;

	return 0;
}

static int
eap_query_encoding(void *addr, struct audio_encoding *fp)
{

	switch (fp->index) {
	case 0:
		strcpy(fp->name, AudioEulinear);
		fp->encoding = AUDIO_ENCODING_ULINEAR;
		fp->precision = 8;
		fp->flags = 0;
		return 0;
	case 1:
		strcpy(fp->name, AudioEmulaw);
		fp->encoding = AUDIO_ENCODING_ULAW;
		fp->precision = 8;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		return 0;
	case 2:
		strcpy(fp->name, AudioEalaw);
		fp->encoding = AUDIO_ENCODING_ALAW;
		fp->precision = 8;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		return 0;
	case 3:
		strcpy(fp->name, AudioEslinear);
		fp->encoding = AUDIO_ENCODING_SLINEAR;
		fp->precision = 8;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		return 0;
	case 4:
		strcpy(fp->name, AudioEslinear_le);
		fp->encoding = AUDIO_ENCODING_SLINEAR_LE;
		fp->precision = 16;
		fp->flags = 0;
		return 0;
	case 5:
		strcpy(fp->name, AudioEulinear_le);
		fp->encoding = AUDIO_ENCODING_ULINEAR_LE;
		fp->precision = 16;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		return 0;
	case 6:
		strcpy(fp->name, AudioEslinear_be);
		fp->encoding = AUDIO_ENCODING_SLINEAR_BE;
		fp->precision = 16;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		return 0;
	case 7:
		strcpy(fp->name, AudioEulinear_be);
		fp->encoding = AUDIO_ENCODING_ULINEAR_BE;
		fp->precision = 16;
		fp->flags = AUDIO_ENCODINGFLAG_EMULATED;
		return 0;
	default:
		return EINVAL;
	}
}

static int
eap_set_params(void *addr, int setmode, int usemode,
	       audio_params_t *play, audio_params_t *rec,
	       stream_filter_list_t *pfil, stream_filter_list_t *rfil)
{
	struct eap_instance *ei;
	struct eap_softc *sc;
	struct audio_params *p;
	stream_filter_list_t *fil;
	int mode, i;
	uint32_t div;

	ei = addr;
	sc = device_private(ei->parent);
	/*
	 * The es1370 only has one clock, so make the sample rates match.
	 * This only applies for ADC/DAC2. The FM DAC is handled below.
	 */
	if (!sc->sc_1371 && ei->index == EAP_DAC2) {
		if (play->sample_rate != rec->sample_rate &&
		    usemode == (AUMODE_PLAY | AUMODE_RECORD)) {
			if (setmode == AUMODE_PLAY) {
				rec->sample_rate = play->sample_rate;
				setmode |= AUMODE_RECORD;
			} else if (setmode == AUMODE_RECORD) {
				play->sample_rate = rec->sample_rate;
				setmode |= AUMODE_PLAY;
			} else
				return EINVAL;
		}
	}

	for (mode = AUMODE_RECORD; mode != -1;
	     mode = mode == AUMODE_RECORD ? AUMODE_PLAY : -1) {
		if ((setmode & mode) == 0)
			continue;

		p = mode == AUMODE_PLAY ? play : rec;

		if (p->sample_rate < 4000 || p->sample_rate > 48000 ||
		    (p->precision != 8 && p->precision != 16) ||
		    (p->channels != 1 && p->channels != 2))
			return EINVAL;

		fil = mode == AUMODE_PLAY ? pfil : rfil;
		i = auconv_set_converter(eap_formats, EAP_NFORMATS,
					 mode, p, FALSE, fil);
		if (i < 0)
			return EINVAL;
	}

	if (sc->sc_1371) {
		eap1371_set_dac_rate(ei, play->sample_rate);
		eap1371_set_adc_rate(sc, rec->sample_rate);
	} else if (ei->index == EAP_DAC2) {
		/* Set the speed */
		DPRINTFN(2, ("eap_set_params: old ICSC = 0x%08x\n",
			     EREAD4(sc, EAP_ICSC)));
		div = EREAD4(sc, EAP_ICSC) & ~EAP_PCLKBITS;
		/*
		 * XXX
		 * The -2 isn't documented, but seemed to make the wall
		 * time match
		 * what I expect.  - mycroft
		 */
		if (usemode == AUMODE_RECORD)
			div |= EAP_SET_PCLKDIV(EAP_XTAL_FREQ /
				rec->sample_rate - 2);
		else
			div |= EAP_SET_PCLKDIV(EAP_XTAL_FREQ /
				play->sample_rate - 2);
#if 0
		div |= EAP_CCB_INTRM;
#else
		/*
		 * It is not obvious how to acknowledge MCCB interrupts, so
		 * we had better not enable them.
		 */
#endif
		EWRITE4(sc, EAP_ICSC, div);
		DPRINTFN(2, ("eap_set_params: set ICSC = 0x%08x\n", div));
	} else {
		/*
		 * The FM DAC has only a few fixed-frequency choises, so
		 * pick out the best candidate.
		 */
		div = EREAD4(sc, EAP_ICSC);
		DPRINTFN(2, ("eap_set_params: old ICSC = 0x%08x\n", div));

		div &= ~EAP_WTSRSEL;
		if (play->sample_rate < 8268)
			div |= EAP_WTSRSEL_5;
		else if (play->sample_rate < 16537)
			div |= EAP_WTSRSEL_11;
		else if (play->sample_rate < 33075)
			div |= EAP_WTSRSEL_22;
		else
			div |= EAP_WTSRSEL_44;

		EWRITE4(sc, EAP_ICSC, div);
		DPRINTFN(2, ("eap_set_params: set ICSC = 0x%08x\n", div));
	}

	return 0;
}

static int
eap_round_blocksize(void *addr, int blk, int mode,
    const audio_params_t *param)
{

	return blk & -32;	/* keep good alignment */
}

static int
eap_trigger_output(
	void *addr,
	void *start,
	void *end,
	int blksize,
	void (*intr)(void *),
	void *arg,
	const audio_params_t *param)
{
	struct eap_instance *ei;
	struct eap_softc *sc;
	struct eap_dma *p;
	uint32_t icsc, sic;
	int sampshift;

	ei = addr;
	sc = device_private(ei->parent);
#ifdef DIAGNOSTIC
	if (ei->ei_prun)
		panic("eap_trigger_output: already running");
	ei->ei_prun = 1;
#endif

	DPRINTFN(1, ("eap_trigger_output: sc=%p start=%p end=%p "
	    "blksize=%d intr=%p(%p)\n", addr, start, end, blksize, intr, arg));
	ei->ei_pintr = intr;
	ei->ei_parg = arg;

	sic = EREAD4(sc, EAP_SIC);
	sic &= ~(EAP_S_EB(ei->index) | EAP_S_MB(ei->index) | EAP_INC_BITS);

	if (ei->index == EAP_DAC2)
		sic |= EAP_SET_P2_ST_INC(0)
		    | EAP_SET_P2_END_INC(param->precision / 8);

	sampshift = 0;
	if (param->precision == 16) {
		sic |= EAP_S_EB(ei->index);
		sampshift++;
	}
	if (param->channels == 2) {
		sic |= EAP_S_MB(ei->index);
		sampshift++;
	}
	EWRITE4(sc, EAP_SIC, sic & ~EAP_P_INTR_EN(ei->index));
	EWRITE4(sc, EAP_SIC, sic | EAP_P_INTR_EN(ei->index));

	for (p = sc->sc_dmas; p && KERNADDR(p) != start; p = p->next)
		continue;
	if (!p) {
		printf("eap_trigger_output: bad addr %p\n", start);
		return EINVAL;
	}

	if (ei->index == EAP_DAC2) {
		DPRINTF(("eap_trigger_output: DAC2_ADDR=0x%x, DAC2_SIZE=0x%x\n",
			 (int)DMAADDR(p),
			 (int)EAP_SET_SIZE(0,
			 (((char *)end - (char *)start) >> 2) - 1)));
		EWRITE4(sc, EAP_MEMPAGE, EAP_DAC_PAGE);
		EWRITE4(sc, EAP_DAC2_ADDR, DMAADDR(p));
		EWRITE4(sc, EAP_DAC2_SIZE,
			EAP_SET_SIZE(0,
			((char *)end - (char *)start) >> 2) - 1);
		EWRITE4(sc, EAP_DAC2_CSR, (blksize >> sampshift) - 1);
	} else if (ei->index == EAP_DAC1) {
		DPRINTF(("eap_trigger_output: DAC1_ADDR=0x%x, DAC1_SIZE=0x%x\n",
			 (int)DMAADDR(p),
			 (int)EAP_SET_SIZE(0,
			 (((char *)end - (char *)start) >> 2) - 1)));
		EWRITE4(sc, EAP_MEMPAGE, EAP_DAC_PAGE);
		EWRITE4(sc, EAP_DAC1_ADDR, DMAADDR(p));
		EWRITE4(sc, EAP_DAC1_SIZE,
			EAP_SET_SIZE(0,
			((char *)end - (char *)start) >> 2) - 1);
		EWRITE4(sc, EAP_DAC1_CSR, (blksize >> sampshift) - 1);
	}
#ifdef DIAGNOSTIC
	else
		panic("eap_trigger_output: impossible instance %d", ei->index);
#endif

	if (sc->sc_1371)
		EWRITE4(sc, E1371_SRC, 0);

	icsc = EREAD4(sc, EAP_ICSC);
	icsc |= EAP_DAC_EN(ei->index);
	EWRITE4(sc, EAP_ICSC, icsc);

	DPRINTFN(1, ("eap_trigger_output: set ICSC = 0x%08x\n", icsc));

	return 0;
}

static int
eap_trigger_input(
	void *addr,
	void *start,
	void *end,
	int blksize,
	void (*intr)(void *),
	void *arg,
	const audio_params_t *param)
{
	struct eap_instance *ei;
	struct eap_softc *sc;
	struct eap_dma *p;
	uint32_t icsc, sic;
	int sampshift;

	ei = addr;
	sc = device_private(ei->parent);
#ifdef DIAGNOSTIC
	if (sc->sc_rrun)
		panic("eap_trigger_input: already running");
	sc->sc_rrun = 1;
#endif

	DPRINTFN(1, ("eap_trigger_input: ei=%p start=%p end=%p blksize=%d intr=%p(%p)\n",
	    addr, start, end, blksize, intr, arg));
	sc->sc_rintr = intr;
	sc->sc_rarg = arg;

	sic = EREAD4(sc, EAP_SIC);
	sic &= ~(EAP_R1_S_EB | EAP_R1_S_MB);
	sampshift = 0;
	if (param->precision == 16) {
		sic |= EAP_R1_S_EB;
		sampshift++;
	}
	if (param->channels == 2) {
		sic |= EAP_R1_S_MB;
		sampshift++;
	}
	EWRITE4(sc, EAP_SIC, sic & ~EAP_R1_INTR_EN);
	EWRITE4(sc, EAP_SIC, sic | EAP_R1_INTR_EN);

	for (p = sc->sc_dmas; p && KERNADDR(p) != start; p = p->next)
		continue;
	if (!p) {
		printf("eap_trigger_input: bad addr %p\n", start);
		return (EINVAL);
	}

	DPRINTF(("eap_trigger_input: ADC_ADDR=0x%x, ADC_SIZE=0x%x\n",
		 (int)DMAADDR(p),
		 (int)EAP_SET_SIZE(0, (((char *)end - (char *)start) >> 2) - 1)));
	EWRITE4(sc, EAP_MEMPAGE, EAP_ADC_PAGE);
	EWRITE4(sc, EAP_ADC_ADDR, DMAADDR(p));
	EWRITE4(sc, EAP_ADC_SIZE,
		EAP_SET_SIZE(0, (((char *)end - (char *)start) >> 2) - 1));

	EWRITE4(sc, EAP_ADC_CSR, (blksize >> sampshift) - 1);

	if (sc->sc_1371)
		EWRITE4(sc, E1371_SRC, 0);

	icsc = EREAD4(sc, EAP_ICSC);
	icsc |= EAP_ADC_EN;
	EWRITE4(sc, EAP_ICSC, icsc);

	DPRINTFN(1, ("eap_trigger_input: set ICSC = 0x%08x\n", icsc));

	return 0;
}

static int
eap_halt_output(void *addr)
{
	struct eap_instance *ei;
	struct eap_softc *sc;
	uint32_t icsc;

	DPRINTF(("eap: eap_halt_output\n"));
	ei = addr;
	sc = device_private(ei->parent);
	icsc = EREAD4(sc, EAP_ICSC);
	EWRITE4(sc, EAP_ICSC, icsc & ~(EAP_DAC_EN(ei->index)));
	ei->ei_pintr = 0;
#ifdef DIAGNOSTIC
	ei->ei_prun = 0;
#endif

	return 0;
}

static int
eap_halt_input(void *addr)
{
	struct eap_instance *ei;
	struct eap_softc *sc;
	uint32_t icsc;

#define EAP_USE_FMDAC_ALSO
	DPRINTF(("eap: eap_halt_input\n"));
	ei = addr;
	sc = device_private(ei->parent);
	icsc = EREAD4(sc, EAP_ICSC);
	EWRITE4(sc, EAP_ICSC, icsc & ~EAP_ADC_EN);
	sc->sc_rintr = 0;
#ifdef DIAGNOSTIC
	sc->sc_rrun = 0;
#endif

	return 0;
}

static int
eap_getdev(void *addr, struct audio_device *retp)
{

	*retp = eap_device;
	return 0;
}

static int
eap1371_mixer_set_port(void *addr, mixer_ctrl_t *cp)
{
	struct eap_instance *ei;
	struct eap_softc *sc;

	ei = addr;
	sc = device_private(ei->parent);
	return sc->codec_if->vtbl->mixer_set_port(sc->codec_if, cp);
}

static int
eap1371_mixer_get_port(void *addr, mixer_ctrl_t *cp)
{
	struct eap_instance *ei;
	struct eap_softc *sc;

	ei = addr;
	sc = device_private(ei->parent);
	return sc->codec_if->vtbl->mixer_get_port(sc->codec_if, cp);
}

static int
eap1371_query_devinfo(void *addr, mixer_devinfo_t *dip)
{
	struct eap_instance *ei;
	struct eap_softc *sc;

	ei = addr;
	sc = device_private(ei->parent);
	return sc->codec_if->vtbl->query_devinfo(sc->codec_if, dip);
}

static void
eap1370_set_mixer(struct eap_softc *sc, int a, int d)
{
	eap1370_write_codec(sc, a, d);

	sc->sc_port[a] = d;
	DPRINTFN(1, ("eap1370_mixer_set_port port 0x%02x = 0x%02x\n", a, d));
}

static int
eap1370_mixer_set_port(void *addr, mixer_ctrl_t *cp)
{
	struct eap_instance *ei;
	struct eap_softc *sc;
	int lval, rval, l, r, la, ra;
	int l1, r1, l2, r2, m, o1, o2;

	ei = addr;
	sc = device_private(ei->parent);
	if (cp->dev == EAP_RECORD_SOURCE) {
		if (cp->type != AUDIO_MIXER_SET)
			return EINVAL;
		m = sc->sc_record_source = cp->un.mask;
		l1 = l2 = r1 = r2 = 0;
		if (m & (1 << EAP_VOICE_VOL))
			l2 |= AK_M_VOICE, r2 |= AK_M_VOICE;
		if (m & (1 << EAP_FM_VOL))
			l1 |= AK_M_FM_L, r1 |= AK_M_FM_R;
		if (m & (1 << EAP_CD_VOL))
			l1 |= AK_M_CD_L, r1 |= AK_M_CD_R;
		if (m & (1 << EAP_LINE_VOL))
			l1 |= AK_M_LINE_L, r1 |= AK_M_LINE_R;
		if (m & (1 << EAP_AUX_VOL))
			l2 |= AK_M2_AUX_L, r2 |= AK_M2_AUX_R;
		if (m & (1 << EAP_MIC_VOL))
			l2 |= AK_M_TMIC, r2 |= AK_M_TMIC;
		eap1370_set_mixer(sc, AK_IN_MIXER1_L, l1);
		eap1370_set_mixer(sc, AK_IN_MIXER1_R, r1);
		eap1370_set_mixer(sc, AK_IN_MIXER2_L, l2);
		eap1370_set_mixer(sc, AK_IN_MIXER2_R, r2);
		return 0;
	}
	if (cp->dev == EAP_INPUT_SOURCE) {
		if (cp->type != AUDIO_MIXER_SET)
			return EINVAL;
		m = sc->sc_input_source = cp->un.mask;
		o1 = o2 = 0;
		if (m & (1 << EAP_VOICE_VOL))
			o2 |= AK_M_VOICE_L | AK_M_VOICE_R;
		if (m & (1 << EAP_FM_VOL))
			o1 |= AK_M_FM_L | AK_M_FM_R;
		if (m & (1 << EAP_CD_VOL))
			o1 |= AK_M_CD_L | AK_M_CD_R;
		if (m & (1 << EAP_LINE_VOL))
			o1 |= AK_M_LINE_L | AK_M_LINE_R;
		if (m & (1 << EAP_AUX_VOL))
			o2 |= AK_M_AUX_L | AK_M_AUX_R;
		if (m & (1 << EAP_MIC_VOL))
			o1 |= AK_M_MIC;
		eap1370_set_mixer(sc, AK_OUT_MIXER1, o1);
		eap1370_set_mixer(sc, AK_OUT_MIXER2, o2);
		return 0;
	}
	if (cp->dev == EAP_MIC_PREAMP) {
		if (cp->type != AUDIO_MIXER_ENUM)
			return EINVAL;
		if (cp->un.ord != 0 && cp->un.ord != 1)
			return EINVAL;
		sc->sc_mic_preamp = cp->un.ord;
		eap1370_set_mixer(sc, AK_MGAIN, cp->un.ord);
		return 0;
	}
	if (cp->type != AUDIO_MIXER_VALUE)
		return EINVAL;
	if (cp->un.value.num_channels == 1)
		lval = rval = cp->un.value.level[AUDIO_MIXER_LEVEL_MONO];
	else if (cp->un.value.num_channels == 2) {
		lval = cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT];
		rval = cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT];
	} else
		return EINVAL;
	ra = -1;
	switch (cp->dev) {
	case EAP_MASTER_VOL:
		l = VOL_TO_ATT5(lval);
		r = VOL_TO_ATT5(rval);
		la = AK_MASTER_L;
		ra = AK_MASTER_R;
		break;
	case EAP_MIC_VOL:
		if (cp->un.value.num_channels != 1)
			return EINVAL;
		la = AK_MIC;
		goto lr;
	case EAP_VOICE_VOL:
		la = AK_VOICE_L;
		ra = AK_VOICE_R;
		goto lr;
	case EAP_FM_VOL:
		la = AK_FM_L;
		ra = AK_FM_R;
		goto lr;
	case EAP_CD_VOL:
		la = AK_CD_L;
		ra = AK_CD_R;
		goto lr;
	case EAP_LINE_VOL:
		la = AK_LINE_L;
		ra = AK_LINE_R;
		goto lr;
	case EAP_AUX_VOL:
		la = AK_AUX_L;
		ra = AK_AUX_R;
	lr:
		l = VOL_TO_GAIN5(lval);
		r = VOL_TO_GAIN5(rval);
		break;
	default:
		return EINVAL;
	}
	eap1370_set_mixer(sc, la, l);
	if (ra >= 0) {
		eap1370_set_mixer(sc, ra, r);
	}
	return 0;
}

static int
eap1370_mixer_get_port(void *addr, mixer_ctrl_t *cp)
{
	struct eap_instance *ei;
	struct eap_softc *sc;
	int la, ra, l, r;

	ei = addr;
	sc = device_private(ei->parent);
	switch (cp->dev) {
	case EAP_RECORD_SOURCE:
		if (cp->type != AUDIO_MIXER_SET)
			return EINVAL;
		cp->un.mask = sc->sc_record_source;
		return 0;
	case EAP_INPUT_SOURCE:
		if (cp->type != AUDIO_MIXER_SET)
			return EINVAL;
		cp->un.mask = sc->sc_input_source;
		return 0;
	case EAP_MIC_PREAMP:
		if (cp->type != AUDIO_MIXER_ENUM)
			return EINVAL;
		cp->un.ord = sc->sc_mic_preamp;
		return 0;
	case EAP_MASTER_VOL:
		l = ATT5_TO_VOL(sc->sc_port[AK_MASTER_L]);
		r = ATT5_TO_VOL(sc->sc_port[AK_MASTER_R]);
		break;
	case EAP_MIC_VOL:
		if (cp->un.value.num_channels != 1)
			return EINVAL;
		la = ra = AK_MIC;
		goto lr;
	case EAP_VOICE_VOL:
		la = AK_VOICE_L;
		ra = AK_VOICE_R;
		goto lr;
	case EAP_FM_VOL:
		la = AK_FM_L;
		ra = AK_FM_R;
		goto lr;
	case EAP_CD_VOL:
		la = AK_CD_L;
		ra = AK_CD_R;
		goto lr;
	case EAP_LINE_VOL:
		la = AK_LINE_L;
		ra = AK_LINE_R;
		goto lr;
	case EAP_AUX_VOL:
		la = AK_AUX_L;
		ra = AK_AUX_R;
	lr:
		l = GAIN5_TO_VOL(sc->sc_port[la]);
		r = GAIN5_TO_VOL(sc->sc_port[ra]);
		break;
	default:
		return EINVAL;
	}
	if (cp->un.value.num_channels == 1)
		cp->un.value.level[AUDIO_MIXER_LEVEL_MONO] = (l+r) / 2;
	else if (cp->un.value.num_channels == 2) {
		cp->un.value.level[AUDIO_MIXER_LEVEL_LEFT]  = l;
		cp->un.value.level[AUDIO_MIXER_LEVEL_RIGHT] = r;
	} else
		return EINVAL;
	return 0;
}

static int
eap1370_query_devinfo(void *addr, mixer_devinfo_t *dip)
{

	switch (dip->index) {
	case EAP_MASTER_VOL:
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = EAP_OUTPUT_CLASS;
		dip->prev = dip->next = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNmaster);
		dip->un.v.num_channels = 2;
		dip->un.v.delta = 8;
		strcpy(dip->un.v.units.name, AudioNvolume);
		return 0;
	case EAP_VOICE_VOL:
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = EAP_INPUT_CLASS;
		dip->prev = AUDIO_MIXER_LAST;
		dip->next = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNdac);
		dip->un.v.num_channels = 2;
		dip->un.v.delta = 8;
		strcpy(dip->un.v.units.name, AudioNvolume);
		return 0;
	case EAP_FM_VOL:
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = EAP_INPUT_CLASS;
		dip->prev = AUDIO_MIXER_LAST;
		dip->next = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNfmsynth);
		dip->un.v.num_channels = 2;
		dip->un.v.delta = 8;
		strcpy(dip->un.v.units.name, AudioNvolume);
		return 0;
	case EAP_CD_VOL:
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = EAP_INPUT_CLASS;
		dip->prev = AUDIO_MIXER_LAST;
		dip->next = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNcd);
		dip->un.v.num_channels = 2;
		dip->un.v.delta = 8;
		strcpy(dip->un.v.units.name, AudioNvolume);
		return 0;
	case EAP_LINE_VOL:
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = EAP_INPUT_CLASS;
		dip->prev = AUDIO_MIXER_LAST;
		dip->next = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNline);
		dip->un.v.num_channels = 2;
		dip->un.v.delta = 8;
		strcpy(dip->un.v.units.name, AudioNvolume);
		return 0;
	case EAP_AUX_VOL:
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = EAP_INPUT_CLASS;
		dip->prev = AUDIO_MIXER_LAST;
		dip->next = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNaux);
		dip->un.v.num_channels = 2;
		dip->un.v.delta = 8;
		strcpy(dip->un.v.units.name, AudioNvolume);
		return 0;
	case EAP_MIC_VOL:
		dip->type = AUDIO_MIXER_VALUE;
		dip->mixer_class = EAP_INPUT_CLASS;
		dip->prev = AUDIO_MIXER_LAST;
		dip->next = EAP_MIC_PREAMP;
		strcpy(dip->label.name, AudioNmicrophone);
		dip->un.v.num_channels = 1;
		dip->un.v.delta = 8;
		strcpy(dip->un.v.units.name, AudioNvolume);
		return 0;
	case EAP_RECORD_SOURCE:
		dip->mixer_class = EAP_RECORD_CLASS;
		dip->prev = dip->next = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNsource);
		dip->type = AUDIO_MIXER_SET;
		dip->un.s.num_mem = 6;
		strcpy(dip->un.s.member[0].label.name, AudioNmicrophone);
		dip->un.s.member[0].mask = 1 << EAP_MIC_VOL;
		strcpy(dip->un.s.member[1].label.name, AudioNcd);
		dip->un.s.member[1].mask = 1 << EAP_CD_VOL;
		strcpy(dip->un.s.member[2].label.name, AudioNline);
		dip->un.s.member[2].mask = 1 << EAP_LINE_VOL;
		strcpy(dip->un.s.member[3].label.name, AudioNfmsynth);
		dip->un.s.member[3].mask = 1 << EAP_FM_VOL;
		strcpy(dip->un.s.member[4].label.name, AudioNaux);
		dip->un.s.member[4].mask = 1 << EAP_AUX_VOL;
		strcpy(dip->un.s.member[5].label.name, AudioNdac);
		dip->un.s.member[5].mask = 1 << EAP_VOICE_VOL;
		return 0;
	case EAP_INPUT_SOURCE:
		dip->mixer_class = EAP_INPUT_CLASS;
		dip->prev = dip->next = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNsource);
		dip->type = AUDIO_MIXER_SET;
		dip->un.s.num_mem = 6;
		strcpy(dip->un.s.member[0].label.name, AudioNmicrophone);
		dip->un.s.member[0].mask = 1 << EAP_MIC_VOL;
		strcpy(dip->un.s.member[1].label.name, AudioNcd);
		dip->un.s.member[1].mask = 1 << EAP_CD_VOL;
		strcpy(dip->un.s.member[2].label.name, AudioNline);
		dip->un.s.member[2].mask = 1 << EAP_LINE_VOL;
		strcpy(dip->un.s.member[3].label.name, AudioNfmsynth);
		dip->un.s.member[3].mask = 1 << EAP_FM_VOL;
		strcpy(dip->un.s.member[4].label.name, AudioNaux);
		dip->un.s.member[4].mask = 1 << EAP_AUX_VOL;
		strcpy(dip->un.s.member[5].label.name, AudioNdac);
		dip->un.s.member[5].mask = 1 << EAP_VOICE_VOL;
		return 0;
	case EAP_MIC_PREAMP:
		dip->type = AUDIO_MIXER_ENUM;
		dip->mixer_class = EAP_INPUT_CLASS;
		dip->prev = EAP_MIC_VOL;
		dip->next = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioNpreamp);
		dip->un.e.num_mem = 2;
		strcpy(dip->un.e.member[0].label.name, AudioNoff);
		dip->un.e.member[0].ord = 0;
		strcpy(dip->un.e.member[1].label.name, AudioNon);
		dip->un.e.member[1].ord = 1;
		return 0;
	case EAP_OUTPUT_CLASS:
		dip->type = AUDIO_MIXER_CLASS;
		dip->mixer_class = EAP_OUTPUT_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioCoutputs);
		return 0;
	case EAP_RECORD_CLASS:
		dip->type = AUDIO_MIXER_CLASS;
		dip->mixer_class = EAP_RECORD_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioCrecord);
		return 0;
	case EAP_INPUT_CLASS:
		dip->type = AUDIO_MIXER_CLASS;
		dip->mixer_class = EAP_INPUT_CLASS;
		dip->next = dip->prev = AUDIO_MIXER_LAST;
		strcpy(dip->label.name, AudioCinputs);
		return 0;
	}
	return ENXIO;
}

static void *
eap_malloc(void *addr, int direction, size_t size)
{
	struct eap_instance *ei;
	struct eap_softc *sc;
	struct eap_dma *p;
	int error;

	p = kmem_alloc(sizeof(*p), KM_SLEEP);
	if (!p)
		return NULL;
	ei = addr;
	sc = device_private(ei->parent);
	error = eap_allocmem(sc, size, 16, p);
	if (error) {
		kmem_free(p, sizeof(*p));
		return NULL;
	}
	p->next = sc->sc_dmas;
	sc->sc_dmas = p;
	return KERNADDR(p);
}

static void
eap_free(void *addr, void *ptr, size_t size)
{
	struct eap_instance *ei;
	struct eap_softc *sc;
	struct eap_dma **pp, *p;

	ei = addr;
	sc = device_private(ei->parent);
	for (pp = &sc->sc_dmas; (p = *pp) != NULL; pp = &p->next) {
		if (KERNADDR(p) == ptr) {
			eap_freemem(sc, p);
			*pp = p->next;
			kmem_free(p, sizeof(*p));
			return;
		}
	}
}

static size_t
eap_round_buffersize(void *addr, int direction, size_t size)
{

	return size;
}

static paddr_t
eap_mappage(void *addr, void *mem, off_t off, int prot)
{
	struct eap_instance *ei;
	struct eap_softc *sc;
	struct eap_dma *p;

	if (off < 0)
		return -1;
	ei = addr;
	sc = device_private(ei->parent);
	for (p = sc->sc_dmas; p && KERNADDR(p) != mem; p = p->next)
		continue;
	if (!p)
		return -1;

	return bus_dmamem_mmap(sc->sc_dmatag, p->segs, p->nsegs,
			       off, prot, BUS_DMA_WAITOK);
}

static int
eap_get_props(void *addr)
{

	return AUDIO_PROP_MMAP | AUDIO_PROP_INDEPENDENT |
	    AUDIO_PROP_FULLDUPLEX;
}

static void
eap_get_locks(void *addr, kmutex_t **intr, kmutex_t **thread)
{
	struct eap_instance *ei;
	struct eap_softc *sc;

	ei = addr;
	sc = device_private(ei->parent);
	*intr = &sc->sc_intr_lock;
	*thread = &sc->sc_lock;
}

#if NMIDI > 0
static int
eap_midi_open(void *addr, int flags,
	      void (*iintr)(void *, int),
	      void (*ointr)(void *),
	      void *arg)
{
	struct eap_softc *sc;
	uint8_t uctrl;

	sc = addr;
	sc->sc_arg = arg;

	EWRITE4(sc, EAP_ICSC, EREAD4(sc, EAP_ICSC) | EAP_UART_EN);
	uctrl = 0;
	if (flags & FREAD) {
		uctrl |= EAP_UC_RXINTEN;
		sc->sc_iintr = iintr;
	}
	if (flags & FWRITE)
		sc->sc_ointr = ointr;
	EWRITE1(sc, EAP_UART_CONTROL, uctrl);

	return 0;
}

static void
eap_midi_close(void *addr)
{
	struct eap_softc *sc;

	sc = addr;
	/* give uart a chance to drain */
	(void)kpause("eapclm", false, hz/10, &sc->sc_intr_lock);
	EWRITE1(sc, EAP_UART_CONTROL, 0);
	EWRITE4(sc, EAP_ICSC, EREAD4(sc, EAP_ICSC) & ~EAP_UART_EN);

	sc->sc_iintr = 0;
	sc->sc_ointr = 0;
}

static int
eap_midi_output(void *addr, int d)
{
	struct eap_softc *sc;
	uint8_t uctrl;

	sc = addr;
	EWRITE1(sc, EAP_UART_DATA, d);
	
	uctrl = EAP_UC_TXINTEN;
	if (sc->sc_iintr)
		uctrl |= EAP_UC_RXINTEN;
	/*
	 * This is a write-only register, so we have to remember the right
	 * value of RXINTEN as well as setting TXINTEN. But if we are open
	 * for reading, it will always be correct to set RXINTEN here; only
	 * during service of a receive interrupt could it be momentarily
	 * toggled off, and whether we got here from the top half or from
	 * an interrupt, that won't be the current state.
	 */
	EWRITE1(sc, EAP_UART_CONTROL, uctrl);
	return 0;
}

static void
eap_midi_getinfo(void *addr, struct midi_info *mi)
{
	mi->name = "AudioPCI MIDI UART";
	mi->props = MIDI_PROP_CAN_INPUT | MIDI_PROP_OUT_INTR;
}

static void
eap_uart_txrdy(struct eap_softc *sc)
{
	uint8_t uctrl;
	uctrl = 0;
	if (sc->sc_iintr)
		uctrl = EAP_UC_RXINTEN;
	EWRITE1(sc, EAP_UART_CONTROL, uctrl);
	sc->sc_ointr(sc->sc_arg);
}

#endif
