/*	$NetBSD: cs4280.c,v 1.68 2014/09/21 15:07:19 christos Exp $	*/

/*
 * Copyright (c) 1999, 2000 Tatoku Ogaito.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Tatoku Ogaito
 *	for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 * Cirrus Logic CS4280 (and maybe CS461x) driver.
 * Data sheets can be found
 * http://www.cirrus.com/ftp/pubs/4280.pdf
 * http://www.cirrus.com/ftp/pubs/4297.pdf
 * ftp://ftp.alsa-project.org/pub/manuals/cirrus/embedded_audio_spec.pdf
 * ftp://ftp.alsa-project.org/pub/manuals/cirrus/embedded_audio_spec.doc
 *
 * Note:  CS4610/CS4611 + CS423x ISA codec should be worked with
 *	 wss* at pnpbios?
 * or
 *       sb* at pnpbios?
 * Since I could not find any documents on handling ISA codec,
 * clcs does not support those chips.
 */

/*
 * TODO
 * Joystick support
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: cs4280.c,v 1.68 2014/09/21 15:07:19 christos Exp $");

#include "midi.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/fcntl.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/audioio.h>
#include <sys/bus.h>
#include <sys/bswap.h>

#include <dev/audio_if.h>
#include <dev/midi_if.h>
#include <dev/mulaw.h>
#include <dev/auconv.h>

#include <dev/ic/ac97reg.h>
#include <dev/ic/ac97var.h>

#include <dev/pci/pcidevs.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/cs4280reg.h>
#include <dev/pci/cs4280_image.h>
#include <dev/pci/cs428xreg.h>
#include <dev/pci/cs428x.h>

#define BA1READ4(sc, r) bus_space_read_4((sc)->ba1t, (sc)->ba1h, (r))
#define BA1WRITE4(sc, r, x) bus_space_write_4((sc)->ba1t, (sc)->ba1h, (r), (x))

/* IF functions for audio driver */
static int  cs4280_match(device_t, cfdata_t, void *);
static void cs4280_attach(device_t, device_t, void *);
static int  cs4280_intr(void *);
static int  cs4280_query_encoding(void *, struct audio_encoding *);
static int  cs4280_set_params(void *, int, int, audio_params_t *,
			      audio_params_t *, stream_filter_list_t *,
			      stream_filter_list_t *);
static int  cs4280_halt_output(void *);
static int  cs4280_halt_input(void *);
static int  cs4280_getdev(void *, struct audio_device *);
static int  cs4280_trigger_output(void *, void *, void *, int, void (*)(void *),
				  void *, const audio_params_t *);
static int  cs4280_trigger_input(void *, void *, void *, int, void (*)(void *),
				 void *, const audio_params_t *);
static int  cs4280_read_codec(void *, u_int8_t, u_int16_t *);
static int  cs4280_write_codec(void *, u_int8_t, u_int16_t);
#if 0
static int cs4280_reset_codec(void *);
#endif
static enum ac97_host_flags cs4280_flags_codec(void *);

static bool cs4280_resume(device_t, const pmf_qual_t *);
static bool cs4280_suspend(device_t, const pmf_qual_t *);

/* Internal functions */
static const struct cs4280_card_t * cs4280_identify_card(const struct pci_attach_args *);
static int  cs4280_piix4_match(const struct pci_attach_args *);
static void cs4280_clkrun_hack(struct cs428x_softc *, int);
static void cs4280_clkrun_hack_init(struct cs428x_softc *);
static void cs4280_set_adc_rate(struct cs428x_softc *, int );
static void cs4280_set_dac_rate(struct cs428x_softc *, int );
static int  cs4280_download(struct cs428x_softc *, const uint32_t *, uint32_t,
			    uint32_t);
static int  cs4280_download_image(struct cs428x_softc *);
static void cs4280_reset(void *);
static int  cs4280_init(struct cs428x_softc *, int);
static void cs4280_clear_fifos(struct cs428x_softc *);

#if CS4280_DEBUG > 10
/* Thease two function is only for checking image loading is succeeded or not. */
static int  cs4280_check_images(struct cs428x_softc *);
static int  cs4280_checkimage(struct cs428x_softc *, uint32_t *, uint32_t,
			      uint32_t);
#endif

/* Special cards */
struct cs4280_card_t
{
	pcireg_t id;
	enum cs428x_flags flags;
};

#define _card(vend, prod, flags) \
	{PCI_ID_CODE(vend, prod), flags}

static const struct cs4280_card_t cs4280_cards[] = {
#if 0	/* untested, from ALSA driver */
	_card(PCI_VENDOR_MITAC, PCI_PRODUCT_MITAC_MI6020,
	      CS428X_FLAG_INVAC97EAMP),
#endif
	_card(PCI_VENDOR_TURTLE_BEACH, PCI_PRODUCT_TURTLE_BEACH_SANTA_CRUZ,
	      CS428X_FLAG_INVAC97EAMP),
	_card(PCI_VENDOR_IBM, PCI_PRODUCT_IBM_TPAUDIO,
	      CS428X_FLAG_CLKRUNHACK)
};

#undef _card

#define CS4280_CARDS_SIZE (sizeof(cs4280_cards)/sizeof(cs4280_cards[0]))

static const struct audio_hw_if cs4280_hw_if = {
	NULL,			/* open */
	NULL,			/* close */
	NULL,
	cs4280_query_encoding,
	cs4280_set_params,
	cs428x_round_blocksize,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	cs4280_halt_output,
	cs4280_halt_input,
	NULL,
	cs4280_getdev,
	NULL,
	cs428x_mixer_set_port,
	cs428x_mixer_get_port,
	cs428x_query_devinfo,
	cs428x_malloc,
	cs428x_free,
	cs428x_round_buffersize,
	cs428x_mappage,
	cs428x_get_props,
	cs4280_trigger_output,
	cs4280_trigger_input,
	NULL,
	cs428x_get_locks,
};

#if NMIDI > 0
/* Midi Interface */
static int  cs4280_midi_open(void *, int, void (*)(void *, int),
		      void (*)(void *), void *);
static void cs4280_midi_close(void*);
static int  cs4280_midi_output(void *, int);
static void cs4280_midi_getinfo(void *, struct midi_info *);

static const struct midi_hw_if cs4280_midi_hw_if = {
	cs4280_midi_open,
	cs4280_midi_close,
	cs4280_midi_output,
	cs4280_midi_getinfo,
	0,
	cs428x_get_locks,
};
#endif

CFATTACH_DECL_NEW(clcs, sizeof(struct cs428x_softc),
    cs4280_match, cs4280_attach, NULL, NULL);

static struct audio_device cs4280_device = {
	"CS4280",
	"",
	"cs4280"
};


static int
cs4280_match(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa;

	pa = (struct pci_attach_args *)aux;
	if (PCI_VENDOR(pa->pa_id) != PCI_VENDOR_CIRRUS)
		return 0;
	if (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_CIRRUS_CS4280
#if 0  /* I can't confirm */
	    || PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_CIRRUS_CS4610
#endif
	    )
		return 1;
	return 0;
}

static void
cs4280_attach(device_t parent, device_t self, void *aux)
{
	struct cs428x_softc *sc;
	struct pci_attach_args *pa;
	pci_chipset_tag_t pc;
	const struct cs4280_card_t *cs_card;
	char const *intrstr;
	pcireg_t reg;
	uint32_t mem;
	int error;
	char vendor[PCI_VENDORSTR_LEN];
	char product[PCI_PRODUCTSTR_LEN];
	char intrbuf[PCI_INTRSTR_LEN];

	sc = device_private(self);
	sc->sc_dev = self;
	pa = (struct pci_attach_args *)aux;
	pc = pa->pa_pc;

	pci_aprint_devinfo(pa, "Audio controller");

	cs_card = cs4280_identify_card(pa);
	if (cs_card != NULL) {
		pci_findvendor(vendor, sizeof(vendor), PCI_VENDOR(cs_card->id));
		pci_findproduct(product, sizeof(product),
		    PCI_VENDOR(cs_card->id), PCI_PRODUCT(cs_card->id)); 
		aprint_normal_dev(sc->sc_dev, "%s %s\n", vendor, product);
		sc->sc_flags = cs_card->flags;
	} else {
		sc->sc_flags = CS428X_FLAG_NONE;
	}

	sc->sc_pc = pa->pa_pc;
	sc->sc_pt = pa->pa_tag;

	/* Map I/O register */
	if (pci_mapreg_map(pa, PCI_BA0,
	    PCI_MAPREG_TYPE_MEM|PCI_MAPREG_MEM_TYPE_32BIT, 0,
	    &sc->ba0t, &sc->ba0h, NULL, NULL)) {
		aprint_error_dev(sc->sc_dev, "can't map BA0 space\n");
		return;
	}
	if (pci_mapreg_map(pa, PCI_BA1,
	    PCI_MAPREG_TYPE_MEM|PCI_MAPREG_MEM_TYPE_32BIT, 0,
	    &sc->ba1t, &sc->ba1h, NULL, NULL)) {
		aprint_error_dev(sc->sc_dev, "can't map BA1 space\n");
		return;
	}

	sc->sc_dmatag = pa->pa_dmat;

	/* power up chip */
	if ((error = pci_activate(pa->pa_pc, pa->pa_tag, self,
	    pci_activate_null)) && error != EOPNOTSUPP) {
		aprint_error_dev(sc->sc_dev, "cannot activate %d\n", error);
		return;
	}

	/* Enable the device (set bus master flag) */
	reg = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG,
		       reg | PCI_COMMAND_MASTER_ENABLE);

	/* LATENCY_TIMER setting */
	mem = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_BHLC_REG);
	if ( PCI_LATTIMER(mem) < 32 ) {
		mem &= 0xffff00ff;
		mem |= 0x00002000;
		pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_BHLC_REG, mem);
	}

	/* CLKRUN hack initialization */
	cs4280_clkrun_hack_init(sc);

	/* Map and establish the interrupt. */
	if (pci_intr_map(pa, &sc->intrh)) {
		aprint_error_dev(sc->sc_dev, "couldn't map interrupt\n");
		return;
	}
	intrstr = pci_intr_string(pc, sc->intrh, intrbuf, sizeof(intrbuf));

	mutex_init(&sc->sc_lock, MUTEX_DEFAULT, IPL_NONE);
	mutex_init(&sc->sc_intr_lock, MUTEX_DEFAULT, IPL_AUDIO);

	sc->sc_ih = pci_intr_establish(sc->sc_pc, sc->intrh, IPL_AUDIO,
	    cs4280_intr, sc);
	if (sc->sc_ih == NULL) {
		aprint_error_dev(sc->sc_dev, "couldn't establish interrupt");
		if (intrstr != NULL)
			aprint_error(" at %s", intrstr);
		aprint_error("\n");
		mutex_destroy(&sc->sc_lock);
		mutex_destroy(&sc->sc_intr_lock);
		return;
	}
	aprint_normal_dev(sc->sc_dev, "interrupting at %s\n", intrstr);

	/* Initialization */
	if(cs4280_init(sc, 1) != 0) {
		mutex_destroy(&sc->sc_lock);
		mutex_destroy(&sc->sc_intr_lock);
		return;
	}

	sc->type = TYPE_CS4280;
	sc->halt_input  = cs4280_halt_input;
	sc->halt_output = cs4280_halt_output;

	/* setup buffer related parameters */
	sc->dma_size     = CS4280_DCHUNK;
	sc->dma_align    = CS4280_DALIGN;
	sc->hw_blocksize = CS4280_ICHUNK;

	/* AC 97 attachment */
	sc->host_if.arg = sc;
	sc->host_if.attach = cs428x_attach_codec;
	sc->host_if.read   = cs4280_read_codec;
	sc->host_if.write  = cs4280_write_codec;
#if 0
	sc->host_if.reset  = cs4280_reset_codec;
#else
	sc->host_if.reset  = NULL;
#endif
	sc->host_if.flags  = cs4280_flags_codec;
	if (ac97_attach(&sc->host_if, self, &sc->sc_lock) != 0) {
		aprint_error_dev(sc->sc_dev, "ac97_attach failed\n");
		return;
	}

	audio_attach_mi(&cs4280_hw_if, sc, sc->sc_dev);

#if NMIDI > 0
	midi_attach_mi(&cs4280_midi_hw_if, sc, sc->sc_dev);
#endif

	if (!pmf_device_register(self, cs4280_suspend, cs4280_resume))
		aprint_error_dev(self, "couldn't establish power handler\n");
}

/* Interrupt handling function */
static int
cs4280_intr(void *p)
{
	/*
	 * XXX
	 *
	 * Since CS4280 has only 4kB DMA buffer and
	 * interrupt occurs every 2kB block, I create dummy buffer
	 * which returns to audio driver and actual DMA buffer
	 * using in DMA transfer.
	 *
	 *
	 *  ring buffer in audio.c is pointed by BUFADDR
	 *	 <------ ring buffer size == 64kB ------>
	 *	 <-----> blksize == 2048*(sc->sc_[pr]count) kB
	 *	|= = = =|= = = =|= = = =|= = = =|= = = =|
	 *	|	|	|	|	|	| <- call audio_intp every
	 *						     sc->sc_[pr]_count time.
	 *
	 *  actual DMA buffer is pointed by KERNADDR
	 *	 <-> DMA buffer size = 4kB
	 *	|= =|
	 *
	 *
	 */
	struct cs428x_softc *sc;
	uint32_t intr, mem;
	char * empty_dma;
	int handled;

	sc = p;
	handled = 0;

	mutex_spin_enter(&sc->sc_intr_lock);

	/* grab interrupt register then clear it */
	intr = BA0READ4(sc, CS4280_HISR);
	BA0WRITE4(sc, CS4280_HICR, HICR_CHGM | HICR_IEV);

	/* not for us ? */
	if ((intr & HISR_INTENA) == 0) {
		mutex_spin_exit(&sc->sc_intr_lock);
		return 0;
	}

	/* Playback Interrupt */
	if (intr & HISR_PINT) {
		handled = 1;
		mem = BA1READ4(sc, CS4280_PFIE);
		BA1WRITE4(sc, CS4280_PFIE, (mem & ~PFIE_PI_MASK) | PFIE_PI_DISABLE);
		if (sc->sc_prun) {
			if ((sc->sc_pi%sc->sc_pcount) == 0)
				sc->sc_pintr(sc->sc_parg);
			/* copy buffer */
			++sc->sc_pi;
			empty_dma = sc->sc_pdma->addr;
			if (sc->sc_pi&1)
				empty_dma += sc->hw_blocksize;
			memcpy(empty_dma, sc->sc_pn, sc->hw_blocksize);
			sc->sc_pn += sc->hw_blocksize;
			if (sc->sc_pn >= sc->sc_pe)
				sc->sc_pn = sc->sc_ps;
		} else {
			aprint_error_dev(sc->sc_dev, "unexpected play intr\n");
		}
		BA1WRITE4(sc, CS4280_PFIE, mem);
	}
	/* Capture Interrupt */
	if (intr & HISR_CINT) {
		int  i;
		int16_t rdata;

		handled = 1;
		mem = BA1READ4(sc, CS4280_CIE);
		BA1WRITE4(sc, CS4280_CIE, (mem & ~CIE_CI_MASK) | CIE_CI_DISABLE);

		if (sc->sc_rrun) {
			++sc->sc_ri;
			empty_dma = sc->sc_rdma->addr;
			if ((sc->sc_ri&1) == 0)
				empty_dma += sc->hw_blocksize;

			/*
			 * XXX
			 * I think this audio data conversion should be
			 * happend in upper layer, but I put this here
			 * since there is no conversion function available.
			 */
			switch(sc->sc_rparam) {
			case CF_16BIT_STEREO:
				/* just copy it */
				memcpy(sc->sc_rn, empty_dma, sc->hw_blocksize);
				sc->sc_rn += sc->hw_blocksize;
				break;
			case CF_16BIT_MONO:
				for (i = 0; i < 512; i++) {
					rdata  = *((int16_t *)empty_dma)>>1;
					empty_dma += 2;
					rdata += *((int16_t *)empty_dma)>>1;
					empty_dma += 2;
					*((int16_t *)sc->sc_rn) = rdata;
					sc->sc_rn += 2;
				}
				break;
			case CF_8BIT_STEREO:
				for (i = 0; i < 512; i++) {
					rdata = *((int16_t*)empty_dma);
					empty_dma += 2;
					*sc->sc_rn++ = rdata >> 8;
					rdata = *((int16_t*)empty_dma);
					empty_dma += 2;
					*sc->sc_rn++ = rdata >> 8;
				}
				break;
			case CF_8BIT_MONO:
				for (i = 0; i < 512; i++) {
					rdata =	 *((int16_t*)empty_dma) >>1;
					empty_dma += 2;
					rdata += *((int16_t*)empty_dma) >>1;
					empty_dma += 2;
					*sc->sc_rn++ = rdata >>8;
				}
				break;
			default:
				/* Should not reach here */
				aprint_error_dev(sc->sc_dev,
				    "unknown sc->sc_rparam: %d\n",
				    sc->sc_rparam);
			}
			if (sc->sc_rn >= sc->sc_re)
				sc->sc_rn = sc->sc_rs;
		}
		BA1WRITE4(sc, CS4280_CIE, mem);

		if (sc->sc_rrun) {
			if ((sc->sc_ri%(sc->sc_rcount)) == 0)
				sc->sc_rintr(sc->sc_rarg);
		} else {
			aprint_error_dev(sc->sc_dev,
			    "unexpected record intr\n");
		}
	}

#if NMIDI > 0
	/* Midi port Interrupt */
	if (intr & HISR_MIDI) {
		int data;

		handled = 1;
		DPRINTF(("i: %d: ",
			 BA0READ4(sc, CS4280_MIDSR)));
		/* Read the received data */
		while ((sc->sc_iintr != NULL) &&
		       ((BA0READ4(sc, CS4280_MIDSR) & MIDSR_RBE) == 0)) {
			data = BA0READ4(sc, CS4280_MIDRP) & MIDRP_MASK;
			DPRINTF(("r:%x\n",data));
			sc->sc_iintr(sc->sc_arg, data);
		}

		/* Write the data */
#if 1
		/* XXX:
		 * It seems "Transmit Buffer Full" never activate until EOI
		 * is deliverd.  Shall I throw EOI top of this routine ?
		 */
		if ((BA0READ4(sc, CS4280_MIDSR) & MIDSR_TBF) == 0) {
			DPRINTF(("w: "));
			if (sc->sc_ointr != NULL)
				sc->sc_ointr(sc->sc_arg);
		}
#else
		while ((sc->sc_ointr != NULL) &&
		       ((BA0READ4(sc, CS4280_MIDSR) & MIDSR_TBF) == 0)) {
			DPRINTF(("w: "));
			sc->sc_ointr(sc->sc_arg);
		}
#endif
		DPRINTF(("\n"));
	}
#endif

	mutex_spin_exit(&sc->sc_intr_lock);
	return handled;
}

static int
cs4280_query_encoding(void *addr, struct audio_encoding *fp)
{
	switch (fp->index) {
	case 0:
		strcpy(fp->name, AudioEulinear);
		fp->encoding = AUDIO_ENCODING_ULINEAR;
		fp->precision = 8;
		fp->flags = 0;
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
		fp->flags = 0;
		break;
	case 6:
		strcpy(fp->name, AudioEslinear_be);
		fp->encoding = AUDIO_ENCODING_SLINEAR_BE;
		fp->precision = 16;
		fp->flags = 0;
		break;
	case 7:
		strcpy(fp->name, AudioEulinear_be);
		fp->encoding = AUDIO_ENCODING_ULINEAR_BE;
		fp->precision = 16;
		fp->flags = 0;
		break;
	default:
		return EINVAL;
	}
	return 0;
}

static int
cs4280_set_params(void *addr, int setmode, int usemode,
    audio_params_t *play, audio_params_t *rec, stream_filter_list_t *pfil,
    stream_filter_list_t *rfil)
{
	audio_params_t hw;
	struct cs428x_softc *sc;
	struct audio_params *p;
	stream_filter_list_t *fil;
	int mode;

	sc = addr;
	for (mode = AUMODE_RECORD; mode != -1;
	    mode = mode == AUMODE_RECORD ? AUMODE_PLAY : -1 ) {
		if ((setmode & mode) == 0)
			continue;

		p = mode == AUMODE_PLAY ? play : rec;

		if (p == play) {
			DPRINTFN(5,("play: sample=%d precision=%d channels=%d\n",
				p->sample_rate, p->precision, p->channels));
			/* play back data format may be 8- or 16-bit and
			 * either stereo or mono.
			 * playback rate may range from 8000Hz to 48000Hz
			 */
			if (p->sample_rate < 8000 || p->sample_rate > 48000 ||
			    (p->precision != 8 && p->precision != 16) ||
			    (p->channels != 1  && p->channels != 2) ) {
				return EINVAL;
			}
		} else {
			DPRINTFN(5,("rec: sample=%d precision=%d channels=%d\n",
				p->sample_rate, p->precision, p->channels));
			/* capture data format must be 16bit stereo
			 * and sample rate range from 11025Hz to 48000Hz.
			 *
			 * XXX: it looks like to work with 8000Hz,
			 *	although data sheets say lower limit is
			 *	11025 Hz.
			 */

			if (p->sample_rate < 8000 || p->sample_rate > 48000 ||
			    (p->precision != 8 && p->precision != 16) ||
			    (p->channels  != 1 && p->channels  != 2) ) {
				return EINVAL;
			}
		}
		fil = mode == AUMODE_PLAY ? pfil : rfil;
		hw = *p;
		hw.encoding = AUDIO_ENCODING_SLINEAR_LE;

		/* capturing data is slinear */
		switch (p->encoding) {
		case AUDIO_ENCODING_SLINEAR_BE:
			if (mode == AUMODE_RECORD && p->precision == 16) {
				fil->append(fil, swap_bytes, &hw);
			}
			break;
		case AUDIO_ENCODING_SLINEAR_LE:
			break;
		case AUDIO_ENCODING_ULINEAR_BE:
			if (mode == AUMODE_RECORD) {
				fil->append(fil, p->precision == 16
					    ? swap_bytes_change_sign16
					    : change_sign8, &hw);
			}
			break;
		case AUDIO_ENCODING_ULINEAR_LE:
			if (mode == AUMODE_RECORD) {
				fil->append(fil, p->precision == 16
					    ? change_sign16 : change_sign8,
					    &hw);
			}
			break;
		case AUDIO_ENCODING_ULAW:
			if (mode == AUMODE_PLAY) {
				hw.precision = 16;
				hw.validbits = 16;
				fil->append(fil, mulaw_to_linear16, &hw);
			} else {
				fil->append(fil, linear8_to_mulaw, &hw);
			}
			break;
		case AUDIO_ENCODING_ALAW:
			if (mode == AUMODE_PLAY) {
				hw.precision = 16;
				hw.validbits = 16;
				fil->append(fil, alaw_to_linear16, &hw);
			} else {
				fil->append(fil, linear8_to_alaw, &hw);
			}
			break;
		default:
			return EINVAL;
		}
	}

	/* set sample rate */
	cs4280_set_dac_rate(sc, play->sample_rate);
	cs4280_set_adc_rate(sc, rec->sample_rate);
	return 0;
}

static int
cs4280_halt_output(void *addr)
{
	struct cs428x_softc *sc;
	uint32_t mem;

	sc = addr;
	mem = BA1READ4(sc, CS4280_PCTL);
	BA1WRITE4(sc, CS4280_PCTL, mem & ~PCTL_MASK);
	sc->sc_prun = 0;
	cs4280_clkrun_hack(sc, -1);

	return 0;
}

static int
cs4280_halt_input(void *addr)
{
	struct cs428x_softc *sc;
	uint32_t mem;

	sc = addr;
	mem = BA1READ4(sc, CS4280_CCTL);
	BA1WRITE4(sc, CS4280_CCTL, mem & ~CCTL_MASK);
	sc->sc_rrun = 0;
	cs4280_clkrun_hack(sc, -1);

	return 0;
}

static int
cs4280_getdev(void *addr, struct audio_device *retp)
{

	*retp = cs4280_device;
	return 0;
}

static int
cs4280_trigger_output(void *addr, void *start, void *end, int blksize,
		      void (*intr)(void *), void *arg,
		      const audio_params_t *param)
{
	struct cs428x_softc *sc;
	uint32_t pfie, pctl, pdtc;
	struct cs428x_dma *p;

	sc = addr;
#ifdef DIAGNOSTIC
	if (sc->sc_prun)
		printf("cs4280_trigger_output: already running\n");
#endif
	sc->sc_prun = 1;
	cs4280_clkrun_hack(sc, 1);

	DPRINTF(("cs4280_trigger_output: sc=%p start=%p end=%p "
	    "blksize=%d intr=%p(%p)\n", addr, start, end, blksize, intr, arg));
	sc->sc_pintr = intr;
	sc->sc_parg  = arg;

	/* stop playback DMA */
	BA1WRITE4(sc, CS4280_PCTL, BA1READ4(sc, CS4280_PCTL) & ~PCTL_MASK);

	/* setup PDTC */
	pdtc = BA1READ4(sc, CS4280_PDTC);
	pdtc &= ~PDTC_MASK;
	pdtc |= CS4280_MK_PDTC(param->precision * param->channels);
	BA1WRITE4(sc, CS4280_PDTC, pdtc);

	DPRINTF(("param: precision=%d channels=%d encoding=%d\n",
	       param->precision, param->channels, param->encoding));
	for (p = sc->sc_dmas; p != NULL && BUFADDR(p) != start; p = p->next)
		continue;
	if (p == NULL) {
		printf("cs4280_trigger_output: bad addr %p\n", start);
		return EINVAL;
	}
	if (DMAADDR(p) % sc->dma_align != 0 ) {
		printf("cs4280_trigger_output: DMAADDR(p)=0x%lx does not start"
		       "4kB align\n", (ulong)DMAADDR(p));
		return EINVAL;
	}

	sc->sc_pcount = blksize / sc->hw_blocksize; /* sc->hw_blocksize is fixed hardware blksize*/
	sc->sc_ps = (char *)start;
	sc->sc_pe = (char *)end;
	sc->sc_pdma = p;
	sc->sc_pbuf = KERNADDR(p);
	sc->sc_pi = 0;
	sc->sc_pn = sc->sc_ps;
	if (blksize >= sc->dma_size) {
		sc->sc_pn = sc->sc_ps + sc->dma_size;
		memcpy(sc->sc_pbuf, start, sc->dma_size);
		++sc->sc_pi;
	} else {
		sc->sc_pn = sc->sc_ps + sc->hw_blocksize;
		memcpy(sc->sc_pbuf, start, sc->hw_blocksize);
	}

	/* initiate playback DMA */
	BA1WRITE4(sc, CS4280_PBA, DMAADDR(p));

	/* set PFIE */
	pfie = BA1READ4(sc, CS4280_PFIE) & ~PFIE_MASK;

	if (param->precision == 8)
		pfie |= PFIE_8BIT;
	if (param->channels == 1)
		pfie |= PFIE_MONO;

	if (param->encoding == AUDIO_ENCODING_ULINEAR_BE ||
	    param->encoding == AUDIO_ENCODING_SLINEAR_BE)
		pfie |= PFIE_SWAPPED;
	if (param->encoding == AUDIO_ENCODING_ULINEAR_BE ||
	    param->encoding == AUDIO_ENCODING_ULINEAR_LE)
		pfie |= PFIE_UNSIGNED;

	BA1WRITE4(sc, CS4280_PFIE, pfie | PFIE_PI_ENABLE);

	sc->sc_prate = param->sample_rate;
	cs4280_set_dac_rate(sc, param->sample_rate);

	pctl = BA1READ4(sc, CS4280_PCTL) & ~PCTL_MASK;
	pctl |= sc->pctl;
	BA1WRITE4(sc, CS4280_PCTL, pctl);
	return 0;
}

static int
cs4280_trigger_input(void *addr, void *start, void *end, int blksize,
		     void (*intr)(void *), void *arg,
		     const audio_params_t *param)
{
	struct cs428x_softc *sc;
	uint32_t cctl, cie;
	struct cs428x_dma *p;

	sc = addr;
#ifdef DIAGNOSTIC
	if (sc->sc_rrun)
		printf("cs4280_trigger_input: already running\n");
#endif
	sc->sc_rrun = 1;
	cs4280_clkrun_hack(sc, 1);

	DPRINTF(("cs4280_trigger_input: sc=%p start=%p end=%p "
	    "blksize=%d intr=%p(%p)\n", addr, start, end, blksize, intr, arg));
	sc->sc_rintr = intr;
	sc->sc_rarg  = arg;

	/* stop capture DMA */
	BA1WRITE4(sc, CS4280_CCTL, BA1READ4(sc, CS4280_CCTL) & ~CCTL_MASK);

	for (p = sc->sc_dmas; p && BUFADDR(p) != start; p = p->next)
		continue;
	if (p == NULL) {
		printf("cs4280_trigger_input: bad addr %p\n", start);
		return EINVAL;
	}
	if (DMAADDR(p) % sc->dma_align != 0) {
		printf("cs4280_trigger_input: DMAADDR(p)=0x%lx does not start"
		       "4kB align\n", (ulong)DMAADDR(p));
		return EINVAL;
	}

	sc->sc_rcount = blksize / sc->hw_blocksize; /* sc->hw_blocksize is fixed hardware blksize*/
	sc->sc_rs = (char *)start;
	sc->sc_re = (char *)end;
	sc->sc_rdma = p;
	sc->sc_rbuf = KERNADDR(p);
	sc->sc_ri = 0;
	sc->sc_rn = sc->sc_rs;

	/* initiate capture DMA */
	BA1WRITE4(sc, CS4280_CBA, DMAADDR(p));

	/* setup format information for internal converter */
	sc->sc_rparam = 0;
	if (param->precision == 8) {
		sc->sc_rparam += CF_8BIT;
		sc->sc_rcount <<= 1;
	}
	if (param->channels  == 1) {
		sc->sc_rparam += CF_MONO;
		sc->sc_rcount <<= 1;
	}

	/* set CIE */
	cie = BA1READ4(sc, CS4280_CIE) & ~CIE_CI_MASK;
	BA1WRITE4(sc, CS4280_CIE, cie | CIE_CI_ENABLE);

	sc->sc_rrate = param->sample_rate;
	cs4280_set_adc_rate(sc, param->sample_rate);

	cctl = BA1READ4(sc, CS4280_CCTL) & ~CCTL_MASK;
	cctl |= sc->cctl;
	BA1WRITE4(sc, CS4280_CCTL, cctl);
	return 0;
}

static bool
cs4280_suspend(device_t dv, const pmf_qual_t *qual)
{
	struct cs428x_softc *sc = device_private(dv);

	mutex_exit(&sc->sc_lock);
	mutex_spin_enter(&sc->sc_intr_lock);

	if (sc->sc_prun) {
		sc->sc_suspend_state.cs4280.pctl = BA1READ4(sc, CS4280_PCTL);
		sc->sc_suspend_state.cs4280.pfie = BA1READ4(sc, CS4280_PFIE);
		sc->sc_suspend_state.cs4280.pba  = BA1READ4(sc, CS4280_PBA);
		sc->sc_suspend_state.cs4280.pdtc = BA1READ4(sc, CS4280_PDTC);
		DPRINTF(("pctl=0x%08x pfie=0x%08x pba=0x%08x pdtc=0x%08x\n",
		    sc->sc_suspend_state.cs4280.pctl,
		    sc->sc_suspend_state.cs4280.pfie,
		    sc->sc_suspend_state.cs4280.pba,
		    sc->sc_suspend_state.cs4280.pdtc));
	}

	/* save current capture status */
	if (sc->sc_rrun) {
		sc->sc_suspend_state.cs4280.cctl = BA1READ4(sc, CS4280_CCTL);
		sc->sc_suspend_state.cs4280.cie  = BA1READ4(sc, CS4280_CIE);
		sc->sc_suspend_state.cs4280.cba  = BA1READ4(sc, CS4280_CBA);
		DPRINTF(("cctl=0x%08x cie=0x%08x cba=0x%08x\n",
		    sc->sc_suspend_state.cs4280.cctl,
		    sc->sc_suspend_state.cs4280.cie,
		    sc->sc_suspend_state.cs4280.cba));
	}

	/* Stop DMA */
	BA1WRITE4(sc, CS4280_PCTL, sc->sc_suspend_state.cs4280.pctl & ~PCTL_MASK);
	BA1WRITE4(sc, CS4280_CCTL, BA1READ4(sc, CS4280_CCTL) & ~CCTL_MASK);

	mutex_spin_exit(&sc->sc_intr_lock);
	mutex_exit(&sc->sc_lock);

	return true;
}

static bool
cs4280_resume(device_t dv, const pmf_qual_t *qual)
{
	struct cs428x_softc *sc = device_private(dv);

	mutex_exit(&sc->sc_lock);
	mutex_spin_enter(&sc->sc_intr_lock);
	cs4280_init(sc, 0);
#if 0
	cs4280_reset_codec(sc);
#endif

	/* restore DMA related status */
	if(sc->sc_prun) {
		DPRINTF(("pctl=0x%08x pfie=0x%08x pba=0x%08x pdtc=0x%08x\n",
		    sc->sc_suspend_state.cs4280.pctl,
		    sc->sc_suspend_state.cs4280.pfie,
		    sc->sc_suspend_state.cs4280.pba,
		    sc->sc_suspend_state.cs4280.pdtc));
		cs4280_set_dac_rate(sc, sc->sc_prate);
		BA1WRITE4(sc, CS4280_PDTC, sc->sc_suspend_state.cs4280.pdtc);
		BA1WRITE4(sc, CS4280_PBA,  sc->sc_suspend_state.cs4280.pba);
		BA1WRITE4(sc, CS4280_PFIE, sc->sc_suspend_state.cs4280.pfie);
		BA1WRITE4(sc, CS4280_PCTL, sc->sc_suspend_state.cs4280.pctl);
	}

	if (sc->sc_rrun) {
		DPRINTF(("cctl=0x%08x cie=0x%08x cba=0x%08x\n",
		    sc->sc_suspend_state.cs4280.cctl,
		    sc->sc_suspend_state.cs4280.cie,
		    sc->sc_suspend_state.cs4280.cba));
		cs4280_set_adc_rate(sc, sc->sc_rrate);
		BA1WRITE4(sc, CS4280_CBA,  sc->sc_suspend_state.cs4280.cba);
		BA1WRITE4(sc, CS4280_CIE,  sc->sc_suspend_state.cs4280.cie);
		BA1WRITE4(sc, CS4280_CCTL, sc->sc_suspend_state.cs4280.cctl);
	}

	mutex_spin_exit(&sc->sc_intr_lock);

	/* restore ac97 registers */
	(*sc->codec_if->vtbl->restore_ports)(sc->codec_if);

	mutex_exit(&sc->sc_lock);

	return true;
}

static int
cs4280_read_codec(void *addr, u_int8_t reg, u_int16_t *result)
{
	struct cs428x_softc *sc = addr;
	int rv;

	cs4280_clkrun_hack(sc, 1);
	rv = cs428x_read_codec(addr, reg, result);
	cs4280_clkrun_hack(sc, -1);

	return rv;
}

static int
cs4280_write_codec(void *addr, u_int8_t reg, u_int16_t data)
{
	struct cs428x_softc *sc = addr;
	int rv;

	cs4280_clkrun_hack(sc, 1);
	rv = cs428x_write_codec(addr, reg, data);
	cs4280_clkrun_hack(sc, -1);

	return rv;
}

#if 0 /* XXX buggy and not required */
/* control AC97 codec */
static int
cs4280_reset_codec(void *addr)
{
	struct cs428x_softc *sc;
	int n;

	sc = addr;

	/* Reset codec */
	BA0WRITE4(sc, CS428X_ACCTL, 0);
	delay(100);    /* delay 100us */
	BA0WRITE4(sc, CS428X_ACCTL, ACCTL_RSTN);

	/*
	 * It looks like we do the following procedure, too
	 */

	/* Enable AC-link sync generation */
	BA0WRITE4(sc, CS428X_ACCTL, ACCTL_ESYN | ACCTL_RSTN);
	delay(50*1000); /* XXX delay 50ms */

	/* Assert valid frame signal */
	BA0WRITE4(sc, CS428X_ACCTL, ACCTL_VFRM | ACCTL_ESYN | ACCTL_RSTN);

	/* Wait for valid AC97 input slot */
	n = 0;
	while ((BA0READ4(sc, CS428X_ACISV) & (ACISV_ISV3 | ACISV_ISV4)) !=
	       (ACISV_ISV3 | ACISV_ISV4)) {
		delay(1000);
		if (++n > 1000) {
			printf("reset_codec: AC97 inputs slot ready timeout\n");
			return ETIMEDOUT;
		}
	}

	return 0;
}
#endif

static enum ac97_host_flags
cs4280_flags_codec(void *addr)
{
	struct cs428x_softc *sc;

	sc = addr;
	if (sc->sc_flags & CS428X_FLAG_INVAC97EAMP)
		return AC97_HOST_INVERTED_EAMP;

	return 0;
}

/* Internal functions */

static const struct cs4280_card_t *
cs4280_identify_card(const struct pci_attach_args *pa)
{
	pcireg_t idreg;
	u_int16_t i;

	idreg = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_SUBSYS_ID_REG);
	for (i = 0; i < CS4280_CARDS_SIZE; i++) {
		if (idreg == cs4280_cards[i].id)
			return &cs4280_cards[i];
	}

	return NULL;
}

static int
cs4280_piix4_match(const struct pci_attach_args *pa)
{
	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_INTEL &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_INTEL_82371AB_PMC) {
			return 1;
	}

	return 0;
}

static void
cs4280_clkrun_hack(struct cs428x_softc *sc, int change)
{
	uint16_t control, val;
	
	if (!(sc->sc_flags & CS428X_FLAG_CLKRUNHACK))
		return;
		
	sc->sc_active += change;
	val = control = bus_space_read_2(sc->sc_pm_iot, sc->sc_pm_ioh, 0x10);
	if (!sc->sc_active)
		val |= 0x2000;
	else
		val &= ~0x2000;
	if (val != control)
		bus_space_write_2(sc->sc_pm_iot, sc->sc_pm_ioh, 0x10, val);
}

static void
cs4280_clkrun_hack_init(struct cs428x_softc *sc)
{
	struct pci_attach_args smbuspa;
	uint16_t reg;
	pcireg_t port;

	if (!(sc->sc_flags & CS428X_FLAG_CLKRUNHACK))
		return;

	if (pci_find_device(&smbuspa, cs4280_piix4_match)) {
		sc->sc_active = 0;
		aprint_normal_dev(sc->sc_dev, "enabling CLKRUN hack\n");

		reg = pci_conf_read(smbuspa.pa_pc, smbuspa.pa_tag, 0x40);
		port = reg & 0xffc0;
		aprint_normal_dev(sc->sc_dev, "power management port 0x%x\n",
		    port);

		sc->sc_pm_iot = smbuspa.pa_iot;
		if (bus_space_map(sc->sc_pm_iot, port, 0x20 /* XXX */, 0,
		    &sc->sc_pm_ioh) == 0)
			return;
	}

	/* handle error */
	sc->sc_flags &= ~CS428X_FLAG_CLKRUNHACK;
	aprint_normal_dev(sc->sc_dev, "disabling CLKRUN hack\n");
}

static void
cs4280_set_adc_rate(struct cs428x_softc *sc, int rate)
{
	/* calculate capture rate:
	 *
	 * capture_coefficient_increment = -round(rate*128*65536/48000;
	 * capture_phase_increment	 = floor(48000*65536*1024/rate);
	 * cx = round(48000*65536*1024 - capture_phase_increment*rate);
	 * cy = floor(cx/200);
	 * capture_sample_rate_correction = cx - 200*cy;
	 * capture_delay = ceil(24*48000/rate);
	 * capture_num_triplets = floor(65536*rate/24000);
	 * capture_group_length = 24000/GCD(rate, 24000);
	 * where GCD means "Greatest Common Divisor".
	 *
	 * capture_coefficient_increment, capture_phase_increment and
	 * capture_num_triplets are 32-bit signed quantities.
	 * capture_sample_rate_correction and capture_group_length are
	 * 16-bit signed quantities.
	 * capture_delay is a 14-bit unsigned quantity.
	 */
	uint32_t cci, cpi, cnt, cx, cy, tmp1;
	uint16_t csrc, cgl, cdlay;

	/* XXX
	 * Even though, embedded_audio_spec says capture rate range 11025 to
	 * 48000, dhwiface.cpp says,
	 *
	 * "We can only decimate by up to a factor of 1/9th the hardware rate.
	 *  Return an error if an attempt is made to stray outside that limit."
	 *
	 * so assume range as 48000/9 to 48000
	 */

	if (rate < 8000)
		rate = 8000;
	if (rate > 48000)
		rate = 48000;

	cx = rate << 16;
	cci = cx / 48000;
	cx -= cci * 48000;
	cx <<= 7;
	cci <<= 7;
	cci += cx / 48000;
	cci = - cci;

	cx = 48000 << 16;
	cpi = cx / rate;
	cx -= cpi * rate;
	cx <<= 10;
	cpi <<= 10;
	cy = cx / rate;
	cpi += cy;
	cx -= cy * rate;

	cy   = cx / 200;
	csrc = cx - 200*cy;

	cdlay = ((48000 * 24) + rate - 1) / rate;
#if 0
	cdlay &= 0x3fff; /* make sure cdlay is 14-bit */
#endif

	cnt  = rate << 16;
	cnt  /= 24000;

	cgl = 1;
	for (tmp1 = 2; tmp1 <= 64; tmp1 *= 2) {
		if (((rate / tmp1) * tmp1) != rate)
			cgl *= 2;
	}
	if (((rate / 3) * 3) != rate)
		cgl *= 3;
	for (tmp1 = 5; tmp1 <= 125; tmp1 *= 5) {
		if (((rate / tmp1) * tmp1) != rate)
			cgl *= 5;
	}
#if 0
	/* XXX what manual says */
	tmp1 = BA1READ4(sc, CS4280_CSRC) & ~CSRC_MASK;
	tmp1 |= csrc<<16;
	BA1WRITE4(sc, CS4280_CSRC, tmp1);
#else
	/* suggested by cs461x.c (ALSA driver) */
	BA1WRITE4(sc, CS4280_CSRC, CS4280_MK_CSRC(csrc, cy));
#endif

#if 0
	/* I am confused.  The sample rate calculation section says
	 * cci *is* 32-bit signed quantity but in the parameter description
	 * section, CCI only assigned 16bit.
	 * I believe size of the variable.
	 */
	tmp1 = BA1READ4(sc, CS4280_CCI) & ~CCI_MASK;
	tmp1 |= cci<<16;
	BA1WRITE4(sc, CS4280_CCI, tmp1);
#else
	BA1WRITE4(sc, CS4280_CCI, cci);
#endif

	tmp1 = BA1READ4(sc, CS4280_CD) & ~CD_MASK;
	tmp1 |= cdlay <<18;
	BA1WRITE4(sc, CS4280_CD, tmp1);

	BA1WRITE4(sc, CS4280_CPI, cpi);

	tmp1 = BA1READ4(sc, CS4280_CGL) & ~CGL_MASK;
	tmp1 |= cgl;
	BA1WRITE4(sc, CS4280_CGL, tmp1);

	BA1WRITE4(sc, CS4280_CNT, cnt);

	tmp1 = BA1READ4(sc, CS4280_CGC) & ~CGC_MASK;
	tmp1 |= cgl;
	BA1WRITE4(sc, CS4280_CGC, tmp1);
}

static void
cs4280_set_dac_rate(struct cs428x_softc *sc, int rate)
{
	/*
	 * playback rate may range from 8000Hz to 48000Hz
	 *
	 * play_phase_increment = floor(rate*65536*1024/48000)
	 * px = round(rate*65536*1024 - play_phase_incremnt*48000)
	 * py=floor(px/200)
	 * play_sample_rate_correction = px - 200*py
	 *
	 * play_phase_increment is a 32bit signed quantity.
	 * play_sample_rate_correction is a 16bit signed quantity.
	 */
	int32_t ppi;
	int16_t psrc;
	uint32_t px, py;

	if (rate < 8000)
		rate = 8000;
	if (rate > 48000)
		rate = 48000;
	px = rate << 16;
	ppi = px/48000;
	px -= ppi*48000;
	ppi <<= 10;
	px  <<= 10;
	py  = px / 48000;
	ppi += py;
	px -= py*48000;
	py  = px/200;
	px -= py*200;
	psrc = px;
#if 0
	/* what manual says */
	px = BA1READ4(sc, CS4280_PSRC) & ~PSRC_MASK;
	BA1WRITE4(sc, CS4280_PSRC,
			  ( ((psrc<<16) & PSRC_MASK) | px ));
#else
	/* suggested by cs461x.c (ALSA driver) */
	BA1WRITE4(sc, CS4280_PSRC, CS4280_MK_PSRC(psrc,py));
#endif
	BA1WRITE4(sc, CS4280_PPI, ppi);
}

/* Download Processor Code and Data image */
static int
cs4280_download(struct cs428x_softc *sc, const uint32_t *src,
		uint32_t offset, uint32_t len)
{
	uint32_t ctr;
#if CS4280_DEBUG > 10
	uint32_t con, data;
	uint8_t c0, c1, c2, c3;
#endif
	if ((offset & 3) || (len & 3))
		return -1;

	len /= sizeof(uint32_t);
	for (ctr = 0; ctr < len; ctr++) {
		/* XXX:
		 * I cannot confirm this is the right thing or not
		 * on BIG-ENDIAN machines.
		 */
		BA1WRITE4(sc, offset+ctr*4, htole32(*(src+ctr)));
#if CS4280_DEBUG > 10
		data = htole32(*(src+ctr));
		c0 = bus_space_read_1(sc->ba1t, sc->ba1h, offset+ctr*4+0);
		c1 = bus_space_read_1(sc->ba1t, sc->ba1h, offset+ctr*4+1);
		c2 = bus_space_read_1(sc->ba1t, sc->ba1h, offset+ctr*4+2);
		c3 = bus_space_read_1(sc->ba1t, sc->ba1h, offset+ctr*4+3);
		con = (c3 << 24) | (c2 << 16) | (c1 << 8) | c0;
		if (data != con ) {
			printf("0x%06x: write=0x%08x read=0x%08x\n",
			       offset+ctr*4, data, con);
			return -1;
		}
#endif
	}
	return 0;
}

static int
cs4280_download_image(struct cs428x_softc *sc)
{
	int idx, err;
	uint32_t offset = 0;

	err = 0;
	for (idx = 0; idx < BA1_MEMORY_COUNT; ++idx) {
		err = cs4280_download(sc, &BA1Struct.map[offset],
				  BA1Struct.memory[idx].offset,
				  BA1Struct.memory[idx].size);
		if (err != 0) {
			aprint_error_dev(sc->sc_dev,
			    "load_image failed at %d\n", idx);
			return -1;
		}
		offset += BA1Struct.memory[idx].size / sizeof(uint32_t);
	}
	return err;
}

/* Processor Soft Reset */
static void
cs4280_reset(void *sc_)
{
	struct cs428x_softc *sc;

	sc = sc_;
	/* Set RSTSP bit in SPCR (also clear RUN, RUNFR, and DRQEN) */
	BA1WRITE4(sc, CS4280_SPCR, SPCR_RSTSP);
	delay(100);
	/* Clear RSTSP bit in SPCR */
	BA1WRITE4(sc, CS4280_SPCR, 0);
	/* enable DMA reqest */
	BA1WRITE4(sc, CS4280_SPCR, SPCR_DRQEN);
}

static int
cs4280_init(struct cs428x_softc *sc, int init)
{
	int n;
	uint32_t mem;
	int rv;

	rv = 1;
	cs4280_clkrun_hack(sc, 1);

	/* Start PLL out in known state */
	BA0WRITE4(sc, CS4280_CLKCR1, 0);
	/* Start serial ports out in known state */
	BA0WRITE4(sc, CS4280_SERMC1, 0);

	/* Specify type of CODEC */
/* XXX should not be here */
#define SERACC_CODEC_TYPE_1_03
#ifdef	SERACC_CODEC_TYPE_1_03
	BA0WRITE4(sc, CS4280_SERACC, SERACC_HSP | SERACC_CTYPE_1_03); /* AC 97 1.03 */
#else
	BA0WRITE4(sc, CS4280_SERACC, SERACC_HSP | SERACC_CTYPE_2_0);  /* AC 97 2.0 */
#endif

	/* Reset codec */
	BA0WRITE4(sc, CS428X_ACCTL, 0);
	delay(100);    /* delay 100us */
	BA0WRITE4(sc, CS428X_ACCTL, ACCTL_RSTN);

	/* Enable AC-link sync generation */
	BA0WRITE4(sc, CS428X_ACCTL, ACCTL_ESYN | ACCTL_RSTN);
	delay(50*1000); /* delay 50ms */

	/* Set the serial port timing configuration */
	BA0WRITE4(sc, CS4280_SERMC1, SERMC1_PTC_AC97);

	/* Setup clock control */
	BA0WRITE4(sc, CS4280_PLLCC, PLLCC_CDR_STATE|PLLCC_LPF_STATE);
	BA0WRITE4(sc, CS4280_PLLM, PLLM_STATE);
	BA0WRITE4(sc, CS4280_CLKCR2, CLKCR2_PDIVS_8);

	/* Power up the PLL */
	BA0WRITE4(sc, CS4280_CLKCR1, CLKCR1_PLLP);
	delay(50*1000); /* delay 50ms */

	/* Turn on clock */
	mem = BA0READ4(sc, CS4280_CLKCR1) | CLKCR1_SWCE;
	BA0WRITE4(sc, CS4280_CLKCR1, mem);

	/* Set the serial port FIFO pointer to the
	 * first sample in FIFO. (not documented) */
	cs4280_clear_fifos(sc);

#if 0
	/* Set the serial port FIFO pointer to the first sample in the FIFO */
	BA0WRITE4(sc, CS4280_SERBSP, 0);
#endif

	/* Configure the serial port */
	BA0WRITE4(sc, CS4280_SERC1,  SERC1_SO1EN | SERC1_SO1F_AC97);
	BA0WRITE4(sc, CS4280_SERC2,  SERC2_SI1EN | SERC2_SI1F_AC97);
	BA0WRITE4(sc, CS4280_SERMC1, SERMC1_MSPE | SERMC1_PTC_AC97);

	/* Wait for CODEC ready */
	n = 0;
	while ((BA0READ4(sc, CS428X_ACSTS) & ACSTS_CRDY) == 0) {
		delay(125);
		if (++n > 1000) {
			aprint_error_dev(sc->sc_dev, "codec ready timeout\n");
			goto exit;
		}
	}

	/* Assert valid frame signal */
	BA0WRITE4(sc, CS428X_ACCTL, ACCTL_VFRM | ACCTL_ESYN | ACCTL_RSTN);

	/* Wait for valid AC97 input slot */
	n = 0;
	while ((BA0READ4(sc, CS428X_ACISV) & (ACISV_ISV3 | ACISV_ISV4)) !=
	       (ACISV_ISV3 | ACISV_ISV4)) {
		delay(1000);
		if (++n > 1000) {
			printf("AC97 inputs slot ready timeout\n");
			goto exit;
		}
	}

	/* Set AC97 output slot valid signals */
	BA0WRITE4(sc, CS428X_ACOSV, ACOSV_SLV3 | ACOSV_SLV4);

	/* reset the processor */
	cs4280_reset(sc);

	/* Download the image to the processor */
	if (cs4280_download_image(sc) != 0) {
		aprint_error_dev(sc->sc_dev, "image download error\n");
		goto exit;
	}

	/* Save playback parameter and then write zero.
	 * this ensures that DMA doesn't immediately occur upon
	 * starting the processor core
	 */
	mem = BA1READ4(sc, CS4280_PCTL);
	sc->pctl = mem & PCTL_MASK; /* save startup value */
	BA1WRITE4(sc, CS4280_PCTL, mem & ~PCTL_MASK);
	if (init != 0)
		sc->sc_prun = 0;

	/* Save capture parameter and then write zero.
	 * this ensures that DMA doesn't immediately occur upon
	 * starting the processor core
	 */
	mem = BA1READ4(sc, CS4280_CCTL);
	sc->cctl = mem & CCTL_MASK; /* save startup value */
	BA1WRITE4(sc, CS4280_CCTL, mem & ~CCTL_MASK);
	if (init != 0)
		sc->sc_rrun = 0;

	/* Processor Startup Procedure */
	BA1WRITE4(sc, CS4280_FRMT, FRMT_FTV);
	BA1WRITE4(sc, CS4280_SPCR, SPCR_RUN | SPCR_RUNFR | SPCR_DRQEN);

	/* Monitor RUNFR bit in SPCR for 1 to 0 transition */
	n = 0;
	while (BA1READ4(sc, CS4280_SPCR) & SPCR_RUNFR) {
		delay(10);
		if (++n > 1000) {
			printf("SPCR 1->0 transition timeout\n");
			goto exit;
		}
	}

	n = 0;
	while (!(BA1READ4(sc, CS4280_SPCS) & SPCS_SPRUN)) {
		delay(10);
		if (++n > 1000) {
			printf("SPCS 0->1 transition timeout\n");
			goto exit;
		}
	}
	/* Processor is now running !!! */

	/* Setup  volume */
	BA1WRITE4(sc, CS4280_PVOL, 0x80008000);
	BA1WRITE4(sc, CS4280_CVOL, 0x80008000);

	/* Interrupt enable */
	BA0WRITE4(sc, CS4280_HICR, HICR_IEV|HICR_CHGM);

	/* playback interrupt enable */
	mem = BA1READ4(sc, CS4280_PFIE) & ~PFIE_PI_MASK;
	mem |= PFIE_PI_ENABLE;
	BA1WRITE4(sc, CS4280_PFIE, mem);
	/* capture interrupt enable */
	mem = BA1READ4(sc, CS4280_CIE) & ~CIE_CI_MASK;
	mem |= CIE_CI_ENABLE;
	BA1WRITE4(sc, CS4280_CIE, mem);

#if NMIDI > 0
	/* Reset midi port */
	mem = BA0READ4(sc, CS4280_MIDCR) & ~MIDCR_MASK;
	BA0WRITE4(sc, CS4280_MIDCR, mem | MIDCR_MRST);
	DPRINTF(("midi reset: 0x%x\n", BA0READ4(sc, CS4280_MIDCR)));
	/* midi interrupt enable */
	mem |= MIDCR_TXE | MIDCR_RXE | MIDCR_RIE | MIDCR_TIE;
	BA0WRITE4(sc, CS4280_MIDCR, mem);
#endif

	rv = 0;

exit:
	cs4280_clkrun_hack(sc, -1);
	return rv;
}

static void
cs4280_clear_fifos(struct cs428x_softc *sc)
{
	int pd, cnt, n;
	uint32_t mem;

	pd = 0;
	/*
	 * If device power down, power up the device and keep power down
	 * state.
	 */
	mem = BA0READ4(sc, CS4280_CLKCR1);
	if (!(mem & CLKCR1_SWCE)) {
		printf("cs4280_clear_fifo: power down found.\n");
		BA0WRITE4(sc, CS4280_CLKCR1, mem | CLKCR1_SWCE);
		pd = 1;
	}
	BA0WRITE4(sc, CS4280_SERBWP, 0);
	for (cnt = 0; cnt < 256; cnt++) {
		n = 0;
		while (BA0READ4(sc, CS4280_SERBST) & SERBST_WBSY) {
			delay(1000);
			if (++n > 1000) {
				printf("clear_fifo: fist timeout cnt=%d\n", cnt);
				break;
			}
		}
		BA0WRITE4(sc, CS4280_SERBAD, cnt);
		BA0WRITE4(sc, CS4280_SERBCM, SERBCM_WRC);
	}
	if (pd)
		BA0WRITE4(sc, CS4280_CLKCR1, mem);
}

#if NMIDI > 0
static int
cs4280_midi_open(void *addr, int flags, void (*iintr)(void *, int),
		 void (*ointr)(void *), void *arg)
{
	struct cs428x_softc *sc;
	uint32_t mem;

	DPRINTF(("midi_open\n"));
	sc = addr;
	sc->sc_iintr = iintr;
	sc->sc_ointr = ointr;
	sc->sc_arg = arg;

	/* midi interrupt enable */
	mem = BA0READ4(sc, CS4280_MIDCR) & ~MIDCR_MASK;
	mem |= MIDCR_TXE | MIDCR_RXE | MIDCR_RIE | MIDCR_TIE | MIDCR_MLB;
	BA0WRITE4(sc, CS4280_MIDCR, mem);
#ifdef CS4280_DEBUG
	if (mem != BA0READ4(sc, CS4280_MIDCR)) {
		DPRINTF(("midi_open: MIDCR=%d\n", BA0READ4(sc, CS4280_MIDCR)));
		return(EINVAL);
	}
	DPRINTF(("MIDCR=0x%x\n", BA0READ4(sc, CS4280_MIDCR)));
#endif
	return 0;
}

static void
cs4280_midi_close(void *addr)
{
	struct cs428x_softc *sc;
	uint32_t mem;

	DPRINTF(("midi_close\n"));
	sc = addr;
	/* give uart a chance to drain */
	kpause("cs0clm", false, hz/10, &sc->sc_intr_lock);
	mem = BA0READ4(sc, CS4280_MIDCR);
	mem &= ~MIDCR_MASK;
	BA0WRITE4(sc, CS4280_MIDCR, mem);

	sc->sc_iintr = 0;
	sc->sc_ointr = 0;
}

static int
cs4280_midi_output(void *addr, int d)
{
	struct cs428x_softc *sc;
	uint32_t mem;
	int x;

	sc = addr;
	for (x = 0; x != MIDI_BUSY_WAIT; x++) {
		if ((BA0READ4(sc, CS4280_MIDSR) & MIDSR_TBF) == 0) {
			mem = BA0READ4(sc, CS4280_MIDWP) & ~MIDWP_MASK;
			mem |= d & MIDWP_MASK;
			DPRINTFN(5,("midi_output d=0x%08x",d));
			BA0WRITE4(sc, CS4280_MIDWP, mem);
#ifdef DIAGNOSTIC
			if (mem != BA0READ4(sc, CS4280_MIDWP)) {
				DPRINTF(("Bad write data: %d %d",
					 mem, BA0READ4(sc, CS4280_MIDWP)));
				return EIO;
			}
#endif
			return 0;
		}
		delay(MIDI_BUSY_DELAY);
	}
	return EIO;
}

static void
cs4280_midi_getinfo(void *addr, struct midi_info *mi)
{

	mi->name = "CS4280 MIDI UART";
	mi->props = MIDI_PROP_CAN_INPUT | MIDI_PROP_OUT_INTR;
}

#endif	/* NMIDI */

/* DEBUG functions */
#if CS4280_DEBUG > 10
static int
cs4280_checkimage(struct cs428x_softc *sc, uint32_t *src,
		  uint32_t offset, uint32_t len)
{
	uint32_t ctr, data;
	int err;

	if ((offset & 3) || (len & 3))
		return -1;

	err = 0;
	len /= sizeof(uint32_t);
	for (ctr = 0; ctr < len; ctr++) {
		/* I cannot confirm this is the right thing
		 * on BIG-ENDIAN machines
		 */
		data = BA1READ4(sc, offset+ctr*4);
		if (data != htole32(*(src+ctr))) {
			printf("0x%06x: 0x%08x(0x%08x)\n",
			       offset+ctr*4, data, *(src+ctr));
			*(src+ctr) = data;
			++err;
		}
	}
	return err;
}

static int
cs4280_check_images(struct cs428x_softc *sc)
{
	int idx, err;
	uint32_t offset;

	offset = 0;
	err = 0;
	/*for (idx=0; idx < BA1_MEMORY_COUNT; ++idx)*/
	for (idx = 0; idx < 1; ++idx) {
		err = cs4280_checkimage(sc, &BA1Struct.map[offset],
				      BA1Struct.memory[idx].offset,
				      BA1Struct.memory[idx].size);
		if (err != 0) {
			aprint_error_dev(sc->sc_dev,
			    "check_image failed at %d\n", idx);
		}
		offset += BA1Struct.memory[idx].size / sizeof(uint32_t);
	}
	return err;
}

#endif	/* CS4280_DEBUG */
