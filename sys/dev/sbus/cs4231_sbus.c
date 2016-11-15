/*	$NetBSD: cs4231_sbus.c,v 1.49 2011/11/23 23:07:36 jmcneill Exp $	*/

/*-
 * Copyright (c) 1998, 1999, 2002, 2007 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Paul Kranenburg.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: cs4231_sbus.c,v 1.49 2011/11/23 23:07:36 jmcneill Exp $");

#include "audio.h"
#if NAUDIO > 0

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/sbus/sbusvar.h>

#include <sys/audioio.h>
#include <dev/audio_if.h>

#include <dev/ic/ad1848reg.h>
#include <dev/ic/cs4231reg.h>
#include <dev/ic/ad1848var.h>
#include <dev/ic/cs4231var.h>

#include <dev/ic/apcdmareg.h>

#ifdef AUDIO_DEBUG
int cs4231_sbus_debug = 0;
#define DPRINTF(x)      if (cs4231_sbus_debug) printf x
#else
#define DPRINTF(x)
#endif

/* where APC DMA registers are located */
#define CS4231_APCDMA_OFFSET	16

/* interrupt enable bits except those specific for playback/capture */
#define APC_ENABLE		(APC_EI | APC_IE | APC_EIE)

struct cs4231_sbus_softc {
	struct cs4231_softc sc_cs4231;

	void *sc_pint;
	void *sc_rint;
	bus_space_tag_t sc_bt;			/* DMA controller tag */
	bus_space_handle_t sc_bh;		/* DMA controller registers */
};


static int	cs4231_sbus_match(device_t, cfdata_t, void *);
static void	cs4231_sbus_attach(device_t, device_t, void *);
static int	cs4231_sbus_pint(void *);
static int	cs4231_sbus_rint(void *);

CFATTACH_DECL_NEW(audiocs_sbus, sizeof(struct cs4231_sbus_softc),
    cs4231_sbus_match, cs4231_sbus_attach, NULL, NULL);

/* audio_hw_if methods specific to apc DMA */
static int	cs4231_sbus_trigger_output(void *, void *, void *, int,
					   void (*)(void *), void *,
					   const audio_params_t *);
static int	cs4231_sbus_trigger_input(void *, void *, void *, int,
					  void (*)(void *), void *,
					  const audio_params_t *);
static int	cs4231_sbus_halt_output(void *);
static int	cs4231_sbus_halt_input(void *);

const struct audio_hw_if audiocs_sbus_hw_if = {
	cs4231_open,
	cs4231_close,
	NULL,			/* drain */
	ad1848_query_encoding,
	ad1848_set_params,
	NULL,			/* round_blocksize */
	ad1848_commit_settings,
	NULL,			/* init_output */
	NULL,			/* init_input */
	NULL,			/* start_output */
	NULL,			/* start_input */
	cs4231_sbus_halt_output,
	cs4231_sbus_halt_input,
	NULL,			/* speaker_ctl */
	cs4231_getdev,
	NULL,			/* setfd */
	cs4231_set_port,
	cs4231_get_port,
	cs4231_query_devinfo,
	cs4231_malloc,
	cs4231_free,
	NULL,			/* round_buffersize */
	NULL,			/* mappage */
	cs4231_get_props,
	cs4231_sbus_trigger_output,
	cs4231_sbus_trigger_input,
	NULL,			/* dev_ioctl */
	ad1848_get_locks,
};


#ifdef AUDIO_DEBUG
static void	cs4231_sbus_regdump(char *, struct cs4231_sbus_softc *);
#endif

static int	cs4231_sbus_intr(void *);



static int
cs4231_sbus_match(device_t parent, cfdata_t cf, void *aux)
{
	struct sbus_attach_args *sa;

	sa = aux;
	return strcmp(sa->sa_name, AUDIOCS_PROM_NAME) == 0;
}


static void
cs4231_sbus_attach(device_t parent, device_t self, void *aux)
{
	struct cs4231_sbus_softc *sbsc;
	struct cs4231_softc *sc;
	struct sbus_attach_args *sa;
	bus_space_handle_t bh;

	sbsc = device_private(self);
	sc = &sbsc->sc_cs4231;
	sa = aux;
	sbsc->sc_bt = sc->sc_bustag = sa->sa_bustag;
	sc->sc_dmatag = sa->sa_dmatag;

	sbsc->sc_pint = sparc_softintr_establish(IPL_SCHED,
	    (void *)cs4231_sbus_pint, sc);
	sbsc->sc_rint = sparc_softintr_establish(IPL_SCHED,
	    (void *)cs4231_sbus_rint, sc);

	/*
	 * Map my registers in, if they aren't already in virtual
	 * address space.
	 */
	if (sa->sa_npromvaddrs) {
		sbus_promaddr_to_handle(sa->sa_bustag,
			sa->sa_promvaddrs[0], &bh);
	} else {
		if (sbus_bus_map(sa->sa_bustag,	sa->sa_slot,
			sa->sa_offset, sa->sa_size, 0, &bh) != 0) {
			aprint_error("%s @ sbus: cannot map registers\n",
				device_xname(self));
			return;
		}
	}

	bus_space_subregion(sa->sa_bustag, bh, CS4231_APCDMA_OFFSET,
		APC_DMA_SIZE, &sbsc->sc_bh);

	cs4231_common_attach(sc, self, bh);
	printf("\n");

	ad1848_init_locks(&sc->sc_ad1848, IPL_SCHED);
	/* Establish interrupt channel */
	if (sa->sa_nintr)
		bus_intr_establish(sa->sa_bustag,
				   sa->sa_pri, IPL_SCHED,
				   cs4231_sbus_intr, sbsc);

	audio_attach_mi(&audiocs_sbus_hw_if, sbsc, self);
}


#ifdef AUDIO_DEBUG
static void
cs4231_sbus_regdump(char *label, struct cs4231_sbus_softc *sc)
{
	char bits[128];

	printf("cs4231regdump(%s): regs:", label);
	printf("dmapva: 0x%x; ",
		bus_space_read_4(sc->sc_bh, sc->sc_bh, APC_DMA_PVA));
	printf("dmapc: 0x%x; ",
		bus_space_read_4(sc->sc_bh, sc->sc_bh, APC_DMA_PC));
	printf("dmapnva: 0x%x; ",
		bus_space_read_4(sc->sc_bh, sc->sc_bh, APC_DMA_PNVA));
	printf("dmapnc: 0x%x\n",
		bus_space_read_4(sc->sc_bh, sc->sc_bh, APC_DMA_PNC));
	printf("dmacva: 0x%x; ",
		bus_space_read_4(sc->sc_bh, sc->sc_bh, APC_DMA_CVA));
	printf("dmacc: 0x%x; ",
		bus_space_read_4(sc->sc_bh, sc->sc_bh, APC_DMA_CC));
	printf("dmacnva: 0x%x; ",
		bus_space_read_4(sc->sc_bh, sc->sc_bh, APC_DMA_CNVA));
	printf("dmacnc: 0x%x\n",
		bus_space_read_4(sc->sc_bh, sc->sc_bh, APC_DMA_CNC));

	snprintb(bits, sizeof(bits), APC_BITS,
	    bus_space_read_4(sc->sc_bh, sc->sc_bh, APC_DMA_CSR));
	printf("apc_dmacsr=%s\n", bits);

	ad1848_dump_regs(&sc->sc_cs4231.sc_ad1848);
}
#endif /* AUDIO_DEBUG */


static int
cs4231_sbus_trigger_output(void *addr, void *start, void *end, int blksize,
			   void (*intr)(void *), void *arg,
			   const audio_params_t *param)
{
	struct cs4231_sbus_softc *sbsc;
	struct cs4231_softc *sc;
	struct cs_transfer *t;
	uint32_t csr;
	bus_addr_t dmaaddr;
	bus_size_t dmasize;
	int ret;
#ifdef AUDIO_DEBUG
	char bits[128];
#endif

	sbsc = addr;
	sc = &sbsc->sc_cs4231;
	t = &sc->sc_playback;
	ret = cs4231_transfer_init(sc, t, &dmaaddr, &dmasize,
				   start, end, blksize, intr, arg);
	if (ret != 0)
		return ret;

	DPRINTF(("trigger_output: was: %x %d, %x %d\n",
		bus_space_read_4(sbsc->sc_bt, sbsc->sc_bh, APC_DMA_PVA),
		bus_space_read_4(sbsc->sc_bt, sbsc->sc_bh, APC_DMA_PC),
		bus_space_read_4(sbsc->sc_bt, sbsc->sc_bh, APC_DMA_PNVA),
		bus_space_read_4(sbsc->sc_bt, sbsc->sc_bh, APC_DMA_PNC)));

	/* load first block */
	bus_space_write_4(sbsc->sc_bt, sbsc->sc_bh, APC_DMA_PNVA, dmaaddr);
	bus_space_write_4(sbsc->sc_bt, sbsc->sc_bh, APC_DMA_PNC, dmasize);

	DPRINTF(("trigger_output: 1st: %x %d, %x %d\n",
		bus_space_read_4(sbsc->sc_bt, sbsc->sc_bh, APC_DMA_PVA),
		bus_space_read_4(sbsc->sc_bt, sbsc->sc_bh, APC_DMA_PC),
		bus_space_read_4(sbsc->sc_bt, sbsc->sc_bh, APC_DMA_PNVA),
		bus_space_read_4(sbsc->sc_bt, sbsc->sc_bh, APC_DMA_PNC)));

	csr = bus_space_read_4(sbsc->sc_bt, sbsc->sc_bh, APC_DMA_CSR);
#ifdef AUDIO_DEBUG
	snprintb(bits, sizeof(bits), APC_BITS, csr);
#endif
	DPRINTF(("trigger_output: csr=%s\n", bits));
	if ((csr & PDMA_GO) == 0 || (csr & APC_PPAUSE) != 0) {
		int cfg;

		csr &= ~(APC_PPAUSE | APC_PMIE | APC_INTR_MASK);
		bus_space_write_4(sbsc->sc_bt, sbsc->sc_bh, APC_DMA_CSR, csr);

		csr = bus_space_read_4(sbsc->sc_bt, sbsc->sc_bh, APC_DMA_CSR);
		csr &= ~APC_INTR_MASK;
		csr |= APC_ENABLE | APC_PIE | APC_PMIE | PDMA_GO;
		bus_space_write_4(sbsc->sc_bt, sbsc->sc_bh, APC_DMA_CSR, csr);

		ad_write(&sc->sc_ad1848, SP_LOWER_BASE_COUNT, 0xff);
		ad_write(&sc->sc_ad1848, SP_UPPER_BASE_COUNT, 0xff);

		cfg = ad_read(&sc->sc_ad1848, SP_INTERFACE_CONFIG);
		ad_write(&sc->sc_ad1848, SP_INTERFACE_CONFIG,
			 (cfg | PLAYBACK_ENABLE));
	} else {
#ifdef AUDIO_DEBUG
		snprintb(bits, sizeof(bits), APC_BITS, csr);
#endif
		DPRINTF(("trigger_output: already: csr=%s\n", bits));
			 
	}

	/* load next block if we can */
	csr = bus_space_read_4(sbsc->sc_bt, sbsc->sc_bh, APC_DMA_CSR);
	if (csr & APC_PD) {
		cs4231_transfer_advance(t, &dmaaddr, &dmasize);
		bus_space_write_4(sbsc->sc_bt, sbsc->sc_bh, APC_DMA_PNVA, dmaaddr);
		bus_space_write_4(sbsc->sc_bt, sbsc->sc_bh, APC_DMA_PNC, dmasize);

		DPRINTF(("trigger_output: 2nd: %x %d, %x %d\n",
		    bus_space_read_4(sbsc->sc_bt, sbsc->sc_bh, APC_DMA_PVA),
		    bus_space_read_4(sbsc->sc_bt, sbsc->sc_bh, APC_DMA_PC),
		    bus_space_read_4(sbsc->sc_bt, sbsc->sc_bh, APC_DMA_PNVA),
		    bus_space_read_4(sbsc->sc_bt, sbsc->sc_bh, APC_DMA_PNC)));
	}

	return 0;
}


static int
cs4231_sbus_halt_output(void *addr)
{
	struct cs4231_sbus_softc *sbsc;
	struct cs4231_softc *sc;
	uint32_t csr;
	int cfg;
#ifdef AUDIO_DEBUG
	char bits[128];
#endif

	sbsc = addr;
	sc = &sbsc->sc_cs4231;
	sc->sc_playback.t_active = 0;

	csr = bus_space_read_4(sbsc->sc_bt, sbsc->sc_bh, APC_DMA_CSR);
#ifdef AUDIO_DEBUG
	snprintb(bits, sizeof(bits), APC_BITS, csr);
#endif
	DPRINTF(("halt_output: csr=%s\n", bits));

	csr &= ~APC_INTR_MASK;	/* do not clear interrupts accidentally */
	csr |= APC_PPAUSE;	/* pause playback (let current complete) */
	bus_space_write_4(sbsc->sc_bt, sbsc->sc_bh, APC_DMA_CSR, csr);

	/* let the curernt transfer complete */
	if (csr & PDMA_GO)
		do {
			csr = bus_space_read_4(sbsc->sc_bt, sbsc->sc_bh,
				APC_DMA_CSR);
#ifdef AUDIO_DEBUG
			snprintb(bits, sizeof(bits), APC_BITS, csr);
#endif
			DPRINTF(("halt_output: csr=%s\n", bits));
		} while ((csr & APC_PM) == 0);

	cfg = ad_read(&sc->sc_ad1848, SP_INTERFACE_CONFIG);
	ad_write(&sc->sc_ad1848, SP_INTERFACE_CONFIG,(cfg & ~PLAYBACK_ENABLE));

	return 0;
}


/* NB: we don't enable APC_CMIE and won't use APC_CM */
static int
cs4231_sbus_trigger_input(void *addr, void *start, void *end, int blksize,
			  void (*intr)(void *), void *arg,
			  const audio_params_t *param)
{
	struct cs4231_sbus_softc *sbsc;
	struct cs4231_softc *sc;
	struct cs_transfer *t;
	uint32_t csr;
	bus_addr_t dmaaddr;
	bus_size_t dmasize;
	int ret;
#ifdef AUDIO_DEBUG
	char bits[128];
#endif

	sbsc = addr;
	sc = &sbsc->sc_cs4231;
	t = &sc->sc_capture;
	ret = cs4231_transfer_init(sc, t, &dmaaddr, &dmasize,
				   start, end, blksize, intr, arg);
	if (ret != 0)
		return ret;

	csr = bus_space_read_4(sbsc->sc_bt, sbsc->sc_bh, APC_DMA_CSR);
#ifdef AUDIO_DEBUG
	snprintb(bits, sizeof(bits), APC_BITS, csr);
#endif
	DPRINTF(("trigger_input: csr=%s\n", bits));
	DPRINTF(("trigger_input: was: %x %d, %x %d\n",
		bus_space_read_4(sbsc->sc_bt, sbsc->sc_bh, APC_DMA_CVA),
		bus_space_read_4(sbsc->sc_bt, sbsc->sc_bh, APC_DMA_CC),
		bus_space_read_4(sbsc->sc_bt, sbsc->sc_bh, APC_DMA_CNVA),
		bus_space_read_4(sbsc->sc_bt, sbsc->sc_bh, APC_DMA_CNC)));

	/* supply first block */
	bus_space_write_4(sbsc->sc_bt, sbsc->sc_bh, APC_DMA_CNVA, dmaaddr);
	bus_space_write_4(sbsc->sc_bt, sbsc->sc_bh, APC_DMA_CNC, dmasize);

	DPRINTF(("trigger_input: 1st: %x %d, %x %d\n",
		bus_space_read_4(sbsc->sc_bt, sbsc->sc_bh, APC_DMA_CVA),
		bus_space_read_4(sbsc->sc_bt, sbsc->sc_bh, APC_DMA_CC),
		bus_space_read_4(sbsc->sc_bt, sbsc->sc_bh, APC_DMA_CNVA),
		bus_space_read_4(sbsc->sc_bt, sbsc->sc_bh, APC_DMA_CNC)));

	csr = bus_space_read_4(sbsc->sc_bt, sbsc->sc_bh, APC_DMA_CSR);
	if ((csr & CDMA_GO) == 0 || (csr & APC_CPAUSE) != 0) {
		int cfg;

		csr &= ~(APC_CPAUSE | APC_CMIE | APC_INTR_MASK);
		bus_space_write_4(sbsc->sc_bt, sbsc->sc_bh, APC_DMA_CSR, csr);

		csr = bus_space_read_4(sbsc->sc_bt, sbsc->sc_bh, APC_DMA_CSR);
		csr &= ~APC_INTR_MASK;
		csr |= APC_ENABLE | APC_CIE | CDMA_GO;
		bus_space_write_4(sbsc->sc_bt, sbsc->sc_bh, APC_DMA_CSR, csr);

		ad_write(&sc->sc_ad1848, CS_LOWER_REC_CNT, 0xff);
		ad_write(&sc->sc_ad1848, CS_UPPER_REC_CNT, 0xff);

		cfg = ad_read(&sc->sc_ad1848, SP_INTERFACE_CONFIG);
		ad_write(&sc->sc_ad1848, SP_INTERFACE_CONFIG,
			 (cfg | CAPTURE_ENABLE));
	} else {
#ifdef AUDIO_DEBUG
		snprintb(bits, sizeof(bits), APC_BITS, csr);
#endif
		DPRINTF(("trigger_input: already: csr=%s\n", bits));
	}

	/* supply next block if we can */
	csr = bus_space_read_4(sbsc->sc_bt, sbsc->sc_bh, APC_DMA_CSR);
	if (csr & APC_CD) {
		cs4231_transfer_advance(t, &dmaaddr, &dmasize);
		bus_space_write_4(sbsc->sc_bt, sbsc->sc_bh, APC_DMA_CNVA, dmaaddr);
		bus_space_write_4(sbsc->sc_bt, sbsc->sc_bh, APC_DMA_CNC, dmasize);
		DPRINTF(("trigger_input: 2nd: %x %d, %x %d\n",
		    bus_space_read_4(sbsc->sc_bt, sbsc->sc_bh, APC_DMA_CVA),
		    bus_space_read_4(sbsc->sc_bt, sbsc->sc_bh, APC_DMA_CC),
		    bus_space_read_4(sbsc->sc_bt, sbsc->sc_bh, APC_DMA_CNVA),
		    bus_space_read_4(sbsc->sc_bt, sbsc->sc_bh, APC_DMA_CNC)));
	}

	return 0;
}


static int
cs4231_sbus_halt_input(void *addr)
{
	struct cs4231_sbus_softc *sbsc;
	struct cs4231_softc *sc;
	uint32_t csr;
	int cfg;
#ifdef AUDIO_DEBUG
	char bits[128];
#endif

	sbsc = addr;
	sc = &sbsc->sc_cs4231;
	sc->sc_capture.t_active = 0;

	csr = bus_space_read_4(sbsc->sc_bt, sbsc->sc_bh, APC_DMA_CSR);
#ifdef AUDIO_DEBUG
	snprintb(bits, sizeof(bits), APC_BITS, csr);
#endif
	DPRINTF(("halt_input: csr=%s\n", bits));
		 

	csr &= ~APC_INTR_MASK;	/* do not clear interrupts accidentally */
	csr |= APC_CPAUSE;
	bus_space_write_4(sbsc->sc_bt, sbsc->sc_bh, APC_DMA_CSR, csr);

	/* let the curernt transfer complete */
	if (csr & CDMA_GO)
		do {
			csr = bus_space_read_4(sbsc->sc_bt, sbsc->sc_bh,
				APC_DMA_CSR);
#ifdef AUDIO_DEBUG
			snprintb(bits, sizeof(bits), APC_BITS, csr);
#endif
			DPRINTF(("halt_input: csr=%s\n", bits));

					
		} while ((csr & APC_CM) == 0);

	cfg = ad_read(&sc->sc_ad1848, SP_INTERFACE_CONFIG);
	ad_write(&sc->sc_ad1848, SP_INTERFACE_CONFIG, (cfg & ~CAPTURE_ENABLE));

	return 0;
}


static int
cs4231_sbus_intr(void *arg)
{
	struct cs4231_sbus_softc *sbsc;
	struct cs4231_softc *sc;
	uint32_t csr;
	int status;
	bus_addr_t dmaaddr;
	bus_size_t dmasize;
	int served;
#if defined(AUDIO_DEBUG) || defined(DIAGNOSTIC)
	char bits[128];
#endif

	sbsc = arg;
	sc = &sbsc->sc_cs4231;
	csr = bus_space_read_4(sbsc->sc_bt, sbsc->sc_bh, APC_DMA_CSR);
	if ((csr & APC_INTR_MASK) == 0)	/* any interrupt pedning? */
		return 0;

	mutex_spin_enter(&sc->sc_ad1848.sc_intr_lock);

	/* write back DMA status to clear interrupt */
	bus_space_write_4(sbsc->sc_bt, sbsc->sc_bh, APC_DMA_CSR, csr);
	++sc->sc_intrcnt.ev_count;
	served = 0;

#ifdef AUDIO_DEBUG
	if (cs4231_sbus_debug > 1)
		cs4231_sbus_regdump("audiointr", sbsc);
#endif

	status = ADREAD(&sc->sc_ad1848, AD1848_STATUS);
#ifdef AUDIO_DEBUG
	snprintb(bits, sizeof(bits), AD_R2_BITS, status);
#endif
	DPRINTF(("%s: status: %s\n", device_xname(sc->sc_ad1848.sc_dev),
	    bits));
	if (status & INTERRUPT_STATUS) {
#ifdef AUDIO_DEBUG
		int reason;

		reason = ad_read(&sc->sc_ad1848, CS_IRQ_STATUS);
		snprintb(bits, sizeof(bits), CS_I24_BITS, reason);
		DPRINTF(("%s: i24: %s\n", device_xname(sc->sc_ad1848.sc_dev),
		    bits));
#endif
		/* clear ad1848 interrupt */
		ADWRITE(&sc->sc_ad1848, AD1848_STATUS, 0);
	}

	if (csr & APC_CI) {
		if (csr & APC_CD) { /* can supply new block */
			struct cs_transfer *t = &sc->sc_capture;

			cs4231_transfer_advance(t, &dmaaddr, &dmasize);
			bus_space_write_4(sbsc->sc_bt, sbsc->sc_bh,
				APC_DMA_CNVA, dmaaddr);
			bus_space_write_4(sbsc->sc_bt, sbsc->sc_bh,
				APC_DMA_CNC, dmasize);

			if (t->t_intr != NULL)
				sparc_softintr_schedule(sbsc->sc_rint);
			++t->t_intrcnt.ev_count;
			served = 1;
		}
	}

	if (csr & APC_PMI) {
		if (!sc->sc_playback.t_active)
			served = 1; /* draining in halt_output() */
	}

	if (csr & APC_PI) {
		if (csr & APC_PD) { /* can load new block */
			struct cs_transfer *t = &sc->sc_playback;

			if (t->t_active) {
				cs4231_transfer_advance(t, &dmaaddr, &dmasize);
				bus_space_write_4(sbsc->sc_bt, sbsc->sc_bh,
					APC_DMA_PNVA, dmaaddr);
				bus_space_write_4(sbsc->sc_bt, sbsc->sc_bh,
					APC_DMA_PNC, dmasize);
			}

			if (t->t_intr != NULL)
				sparc_softintr_schedule(sbsc->sc_pint);
			++t->t_intrcnt.ev_count;
			served = 1;
		}
	}

	/* got an interrupt we don't know how to handle */
	if (!served) {
#ifdef DIAGNOSTIC
	        snprintb(bits, sizeof(bits), APC_BITS, csr);
		printf("%s: unhandled csr=%s\n",
		    device_xname(sc->sc_ad1848.sc_dev), bits);
#endif
		/* evcnt? */
	}

	mutex_spin_exit(&sc->sc_ad1848.sc_intr_lock);

	return 1;
}

static int
cs4231_sbus_pint(void *cookie)
{
	struct cs4231_softc *sc = cookie;
	struct cs_transfer *t;

	mutex_spin_enter(&sc->sc_ad1848.sc_intr_lock);
	t = &sc->sc_playback;
	if (t->t_intr != NULL)
		(*t->t_intr)(t->t_arg);
	mutex_spin_exit(&sc->sc_ad1848.sc_intr_lock);
	return 0;
}

static int
cs4231_sbus_rint(void *cookie)
{
	struct cs4231_softc *sc = cookie;
	struct cs_transfer *t;

	mutex_spin_enter(&sc->sc_ad1848.sc_intr_lock);
	t = &sc->sc_capture;
	if (t->t_intr != NULL)
		(*t->t_intr)(t->t_arg);
	mutex_spin_exit(&sc->sc_ad1848.sc_intr_lock);
	return 0;
}

#endif /* NAUDIO > 0 */
