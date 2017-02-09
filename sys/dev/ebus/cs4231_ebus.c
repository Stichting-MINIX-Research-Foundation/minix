/*	$NetBSD: cs4231_ebus.c,v 1.35 2011/11/23 23:07:31 jmcneill Exp $ */

/*
 * Copyright (c) 2002 Valeriy E. Ushakov
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
 * 3. The name of the author may not be used to endorse or promote products
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: cs4231_ebus.c,v 1.35 2011/11/23 23:07:31 jmcneill Exp $");

#ifdef _KERNEL_OPT
#include "opt_sparc_arch.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/device.h>
#include <sys/cpu.h>
#include <sys/kmem.h>
#include <sys/malloc.h>

#include <machine/autoconf.h>

#include <dev/ebus/ebusreg.h>
#include <dev/ebus/ebusvar.h>

#include <sys/audioio.h>
#include <dev/audio_if.h>

#include <dev/ic/ad1848reg.h>
#include <dev/ic/cs4231reg.h>
#include <dev/ic/ad1848var.h>
#include <dev/ic/cs4231var.h>

#ifdef AUDIO_DEBUG
int cs4231_ebus_debug = 0;
#define DPRINTF(x)	if (cs4231_ebus_debug) printf x
#else
#define DPRINTF(x)
#endif


struct cs4231_ebus_softc {
	struct cs4231_softc sc_cs4231;

	void *sc_pint;
	void *sc_rint;
	bus_space_tag_t sc_bt;
	bus_space_handle_t sc_pdmareg; /* playback DMA */
	bus_space_handle_t sc_cdmareg; /* record DMA */
};


void	cs4231_ebus_attach(device_t, device_t, void *);
int	cs4231_ebus_match(device_t, cfdata_t, void *);

static int	cs4231_ebus_pint(void *);
static int	cs4231_ebus_rint(void *);

CFATTACH_DECL_NEW(audiocs_ebus, sizeof(struct cs4231_ebus_softc),
    cs4231_ebus_match, cs4231_ebus_attach, NULL, NULL);

/* audio_hw_if methods specific to ebus DMA */
static int	cs4231_ebus_round_blocksize(void *, int, int,
					    const audio_params_t *);
static int	cs4231_ebus_trigger_output(void *, void *, void *, int,
					   void (*)(void *), void *,
					   const audio_params_t *);
static int	cs4231_ebus_trigger_input(void *, void *, void *, int,
					  void (*)(void *), void *,
					  const audio_params_t *);
static int	cs4231_ebus_halt_output(void *);
static int	cs4231_ebus_halt_input(void *);

const struct audio_hw_if audiocs_ebus_hw_if = {
	cs4231_open,
	cs4231_close,
	NULL,			/* drain */
	ad1848_query_encoding,
	ad1848_set_params,
	cs4231_ebus_round_blocksize,
	ad1848_commit_settings,
	NULL,			/* init_output */
	NULL,			/* init_input */
	NULL,			/* start_output */
	NULL,			/* start_input */
	cs4231_ebus_halt_output,
	cs4231_ebus_halt_input,
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
	cs4231_ebus_trigger_output,
	cs4231_ebus_trigger_input,
	NULL,			/* dev_ioctl */
	ad1848_get_locks,
};

#ifdef AUDIO_DEBUG
static void	cs4231_ebus_regdump(char *, struct cs4231_ebus_softc *);
#endif

static int	cs4231_ebus_dma_reset(bus_space_tag_t, bus_space_handle_t);
static int	cs4231_ebus_trigger_transfer(struct cs4231_softc *,
			struct cs_transfer *,
			bus_space_tag_t, bus_space_handle_t,
			int, void *, void *, int, void (*)(void *), void *,
			const audio_params_t *);
static void	cs4231_ebus_dma_advance(struct cs_transfer *,
					bus_space_tag_t, bus_space_handle_t);
static int	cs4231_ebus_dma_intr(struct cs_transfer *,
				     bus_space_tag_t, bus_space_handle_t,
				     void *);
static int	cs4231_ebus_intr(void *);


int
cs4231_ebus_match(device_t parent, cfdata_t cf, void *aux)
{
	struct ebus_attach_args *ea;
	char *compat;
	int len, total_size;

	ea = aux;
	if (strcmp(ea->ea_name, AUDIOCS_PROM_NAME) == 0)
		return 1;

	compat = NULL;
	if (prom_getprop(ea->ea_node, "compatible", 1, &total_size, &compat) == 0) {
		do {
			if (strcmp(compat, AUDIOCS_PROM_NAME) == 0)
				return 1;
#ifdef __sparc__
			/* on KRUPS compatible lists: "cs4231", "ad1848",
			 * "mwave", and "pnpPNP,b007" */
			if (strcmp(compat, "cs4231") == 0)
				return 1;
#endif
			len = strlen(compat) + 1;
			total_size -= len;
			compat += len;
		} while (total_size > 0);
	}

	return 0;
}


void
cs4231_ebus_attach(device_t parent, device_t self, void *aux)
{
	struct cs4231_ebus_softc *ebsc;
	struct cs4231_softc *sc;
	struct ebus_attach_args *ea;
	bus_space_handle_t bh;
	int i;

	ebsc = device_private(self);
	sc = &ebsc->sc_cs4231;
	ea = aux;
	sc->sc_bustag = ebsc->sc_bt = ea->ea_bustag;
	sc->sc_dmatag = ea->ea_dmatag;

	ebsc->sc_pint = sparc_softintr_establish(IPL_VM,
	    (void *)cs4231_ebus_pint, sc);
	ebsc->sc_rint = sparc_softintr_establish(IPL_VM,
	    (void *)cs4231_ebus_rint, sc);

	/*
	 * These are the register we get from the prom:
	 *	- CS4231 registers
	 *	- Playback EBus DMA controller
	 *	- Capture EBus DMA controller
	 *	- AUXIO audio register (codec powerdown)
	 *
	 * Map my registers in, if they aren't already in virtual
	 * address space.
	 */
	if (bus_space_map(ea->ea_bustag, EBUS_ADDR_FROM_REG(&ea->ea_reg[0]),
		ea->ea_reg[0].size, 0, &bh) != 0) {
		printf(": unable to map registers\n");
		return;
	}

	if (bus_space_map(ea->ea_bustag,
#ifdef MSIIEP		/* XXX: Krups */
			  /*
			   * XXX: map playback DMA registers
			   * (we just know where they are)
			   */
			  BUS_ADDR(0x14, 0x702000), /* XXX: magic num */
			  EBUS_DMAC_SIZE,
#else
			  EBUS_ADDR_FROM_REG(&ea->ea_reg[1]),
			  ea->ea_reg[1].size,
#endif
			  0, &ebsc->sc_pdmareg) != 0)
	{
		printf(": unable to map playback DMA registers\n");
		return;
	}

	if (bus_space_map(ea->ea_bustag,
#ifdef MSIIEP		/* XXX: Krups */
			  /*
			   * XXX: map capture DMA registers
			   * (we just know where they are)
			   */
			  BUS_ADDR(0x14, 0x704000), /* XXX: magic num */
			  EBUS_DMAC_SIZE,
#else
			  EBUS_ADDR_FROM_REG(&ea->ea_reg[2]),
			  ea->ea_reg[2].size,
#endif
			  0, &ebsc->sc_cdmareg) != 0)
	{
		printf(": unable to map capture DMA registers\n");
		return;
	}

	ad1848_init_locks(&sc->sc_ad1848, IPL_SCHED);

	/* establish interrupt channels */
	for (i = 0; i < ea->ea_nintr; ++i)
		bus_intr_establish(ea->ea_bustag,
				   ea->ea_intr[i], IPL_SCHED,
				   cs4231_ebus_intr, ebsc);

	cs4231_common_attach(sc, self, bh);
	printf("\n");

	/* XXX: todo: move to cs4231_common_attach, pass hw_if as arg? */
	audio_attach_mi(&audiocs_ebus_hw_if, sc, sc->sc_ad1848.sc_dev);
}


static int
cs4231_ebus_round_blocksize(void *addr, int blk, int mode,
			    const audio_params_t *param)
{

	/* we want to use DMA burst size of 16 words */
	return blk & -64;
}


#ifdef AUDIO_DEBUG
static void
cs4231_ebus_regdump(char *label, struct cs4231_ebus_softc *ebsc)
{
	/* char bits[128]; */

	printf("cs4231regdump(%s): regs:", label);
	/* XXX: dump ebus DMA and aux registers */
	ad1848_dump_regs(&ebsc->sc_cs4231.sc_ad1848);
}
#endif /* AUDIO_DEBUG */


/* XXX: nothing CS4231-specific in this code... */
static int
cs4231_ebus_dma_reset(bus_space_tag_t dt, bus_space_handle_t dh)
{
	u_int32_t csr;
	int timo;

	/* reset, also clear TC, just in case */
	bus_space_write_4(dt, dh, EBUS_DMAC_DCSR, EBDMA_RESET | EBDMA_TC);

	for (timo = 50000; timo != 0; --timo) {
		csr = bus_space_read_4(dt, dh, EBUS_DMAC_DCSR);
		if ((csr & (EBDMA_CYC_PEND | EBDMA_DRAIN)) == 0)
			break;
	}

	if (timo == 0) {
		char bits[128];
		snprintb(bits, sizeof(bits), EBUS_DCSR_BITS, csr);
		printf("cs4231_ebus_dma_reset: timed out: csr=%s\n", bits);
		return ETIMEDOUT;
	}

	bus_space_write_4(dt, dh, EBUS_DMAC_DCSR, csr & ~EBDMA_RESET);
	return 0;
}


static void
cs4231_ebus_dma_advance(struct cs_transfer *t, bus_space_tag_t dt,
			bus_space_handle_t dh)
{
	bus_addr_t dmaaddr;
	bus_size_t dmasize;

	cs4231_transfer_advance(t, &dmaaddr, &dmasize);

	bus_space_write_4(dt, dh, EBUS_DMAC_DNBR, (u_int32_t)dmasize);
	bus_space_write_4(dt, dh, EBUS_DMAC_DNAR, (u_int32_t)dmaaddr);
}


/*
 * Trigger transfer "t" using DMA controller at "dt"/"dh".
 * "iswrite" defines direction of the transfer.
 */
static int
cs4231_ebus_trigger_transfer(
	struct cs4231_softc *sc,
	struct cs_transfer *t,
	bus_space_tag_t dt,
	bus_space_handle_t dh,
	int iswrite,
	void *start, void *end,
	int blksize,
	void (*intr)(void *),
	void *arg,
	const audio_params_t *param)
{
	uint32_t csr;
	bus_addr_t dmaaddr;
	bus_size_t dmasize;
	int ret;

	ret = cs4231_transfer_init(sc, t, &dmaaddr, &dmasize,
				   start, end, blksize, intr, arg);
	if (ret != 0)
		return ret;

	ret = cs4231_ebus_dma_reset(dt, dh);
	if (ret != 0)
		return ret;

	csr = bus_space_read_4(dt, dh, EBUS_DMAC_DCSR);
	bus_space_write_4(dt, dh, EBUS_DMAC_DCSR,
			  csr | EBDMA_EN_NEXT | (iswrite ? EBDMA_WRITE : 0)
			  | EBDMA_EN_DMA | EBDMA_EN_CNT | EBDMA_INT_EN
			  | EBDMA_BURST_SIZE_16);

	/* first load: propagated to DACR/DBCR */
	bus_space_write_4(dt, dh, EBUS_DMAC_DNBR, (uint32_t)dmasize);
	bus_space_write_4(dt, dh, EBUS_DMAC_DNAR, (uint32_t)dmaaddr);

	/* next load: goes to DNAR/DNBR */
	cs4231_ebus_dma_advance(t, dt, dh);

	return 0;
}


static int
cs4231_ebus_trigger_output(void *addr, void *start, void *end, int blksize,
			   void (*intr)(void *), void *arg,
			   const audio_params_t *param)
{
	struct cs4231_ebus_softc *ebsc;
	struct cs4231_softc *sc;
	int cfg, ret;

	ebsc = addr;
	sc = &ebsc->sc_cs4231;
	ret = cs4231_ebus_trigger_transfer(sc, &sc->sc_playback,
					   ebsc->sc_bt, ebsc->sc_pdmareg,
					   0, /* iswrite */
					   start, end, blksize,
					   intr, arg, param);
	if (ret != 0)
		return ret;

	ad_write(&sc->sc_ad1848, SP_LOWER_BASE_COUNT, 0xff);
	ad_write(&sc->sc_ad1848, SP_UPPER_BASE_COUNT, 0xff);

	cfg = ad_read(&sc->sc_ad1848, SP_INTERFACE_CONFIG);
	ad_write(&sc->sc_ad1848, SP_INTERFACE_CONFIG, cfg | PLAYBACK_ENABLE);

	return 0;
}


static int
cs4231_ebus_trigger_input(void *addr, void *start, void *end, int blksize,
			  void (*intr)(void *), void *arg,
			  const audio_params_t *param)
{
	struct cs4231_ebus_softc *ebsc;
	struct cs4231_softc *sc;
	int cfg, ret;

	ebsc = addr;
	sc = &ebsc->sc_cs4231;
	ret = cs4231_ebus_trigger_transfer(sc, &sc->sc_capture,
					   ebsc->sc_bt, ebsc->sc_cdmareg,
					   1, /* iswrite */
					   start, end, blksize,
					   intr, arg, param);
	if (ret != 0)
		return ret;

	ad_write(&sc->sc_ad1848, CS_LOWER_REC_CNT, 0xff);
	ad_write(&sc->sc_ad1848, CS_UPPER_REC_CNT, 0xff);

	cfg = ad_read(&sc->sc_ad1848, SP_INTERFACE_CONFIG);
	ad_write(&sc->sc_ad1848, SP_INTERFACE_CONFIG, cfg | CAPTURE_ENABLE);

	return 0;
}


static int
cs4231_ebus_halt_output(void *addr)
{
	struct cs4231_ebus_softc *ebsc;
	struct cs4231_softc *sc;
	u_int32_t csr;
	int cfg;

	ebsc = addr;
	sc = &ebsc->sc_cs4231;
	sc->sc_playback.t_active = 0;

	csr = bus_space_read_4(ebsc->sc_bt, ebsc->sc_pdmareg, EBUS_DMAC_DCSR);
	bus_space_write_4(ebsc->sc_bt, ebsc->sc_pdmareg, EBUS_DMAC_DCSR,
			  csr & ~EBDMA_EN_DMA);

	cfg = ad_read(&sc->sc_ad1848, SP_INTERFACE_CONFIG);
	ad_write(&sc->sc_ad1848, SP_INTERFACE_CONFIG,
		 cfg & ~PLAYBACK_ENABLE);

	return 0;
}


static int
cs4231_ebus_halt_input(void *addr)
{
	struct cs4231_ebus_softc *ebsc;
	struct cs4231_softc *sc;
	uint32_t csr;
	int cfg;

	ebsc = addr;
	sc = &ebsc->sc_cs4231;
	sc->sc_capture.t_active = 0;

	csr = bus_space_read_4(ebsc->sc_bt, ebsc->sc_cdmareg, EBUS_DMAC_DCSR);
	bus_space_write_4(ebsc->sc_bt, ebsc->sc_cdmareg, EBUS_DMAC_DCSR,
			  csr & ~EBDMA_EN_DMA);

	cfg = ad_read(&sc->sc_ad1848, SP_INTERFACE_CONFIG);
	ad_write(&sc->sc_ad1848, SP_INTERFACE_CONFIG,
		 cfg & ~CAPTURE_ENABLE);

	return 0;
}


static int
cs4231_ebus_dma_intr(struct cs_transfer *t, bus_space_tag_t dt,
		     bus_space_handle_t dh, void *sih)
{
	uint32_t csr;
#ifdef AUDIO_DEBUG
	char bits[128];
#endif

	/* read DMA status, clear TC bit by writing it back */
	csr = bus_space_read_4(dt, dh, EBUS_DMAC_DCSR);
	bus_space_write_4(dt, dh, EBUS_DMAC_DCSR, csr);
#ifdef AUDIO_DEBUG
	snprintb(bits, sizeof(bits), EBUS_DCSR_BITS, csr);
	DPRINTF(("audiocs: %s dcsr=%s\n", t->t_name, bits));
#endif

	if (csr & EBDMA_ERR_PEND) {
		++t->t_ierrcnt.ev_count;
		printf("audiocs: %s DMA error, resetting\n", t->t_name);
		cs4231_ebus_dma_reset(dt, dh);
		/* how to notify audio(9)??? */
		return 1;
	}

	if ((csr & EBDMA_INT_PEND) == 0)
		return 0;

	++t->t_intrcnt.ev_count;

	if ((csr & EBDMA_TC) == 0) { /* can this happen? */
		printf("audiocs: %s INT_PEND but !TC\n", t->t_name);
		return 1;
	}

	if (!t->t_active)
		return 1;

	cs4231_ebus_dma_advance(t, dt, dh);

	/* call audio(9) framework while DMA is chugging along */
	if (t->t_intr != NULL)
		sparc_softintr_schedule(sih);
	return 1;
}


static int
cs4231_ebus_intr(void *arg)
{
	struct cs4231_ebus_softc *ebsc;
	struct cs4231_softc *sc;
	int status;
	int ret;
#ifdef AUDIO_DEBUG
	char bits[128];
#endif

	ebsc = arg;
	sc = &ebsc->sc_cs4231;
	mutex_spin_enter(&sc->sc_ad1848.sc_intr_lock);

	status = ADREAD(&sc->sc_ad1848, AD1848_STATUS);

#ifdef AUDIO_DEBUG
	if (cs4231_ebus_debug > 1)
		cs4231_ebus_regdump("audiointr", ebsc);

	snprintb(bits, sizeof(bits), AD_R2_BITS, status);
	DPRINTF(("%s: status: %s\n", device_xname(sc->sc_ad1848.sc_dev),
	    bits));
#endif

	if (status & INTERRUPT_STATUS) {
#ifdef AUDIO_DEBUG
		int reason;

		reason = ad_read(&sc->sc_ad1848, CS_IRQ_STATUS);
	        snprintb(bits, sizeof(bits), CS_I24_BITS, reason);
		DPRINTF(("%s: i24: %s\n", device_xname(sc->sc_ad1848.sc_dev),
		    bits));
#endif
		/* clear interrupt from ad1848 */
		ADWRITE(&sc->sc_ad1848, AD1848_STATUS, 0);
	}

	ret = 0;

	if (cs4231_ebus_dma_intr(&sc->sc_capture, ebsc->sc_bt,
	    ebsc->sc_cdmareg, ebsc->sc_rint) != 0)
	{
		++sc->sc_intrcnt.ev_count;
		ret = 1;
	}

	if (cs4231_ebus_dma_intr(&sc->sc_playback, ebsc->sc_bt,
	    ebsc->sc_pdmareg, ebsc->sc_pint) != 0)
	{
		++sc->sc_intrcnt.ev_count;
		ret = 1;
	}

	mutex_spin_exit(&sc->sc_ad1848.sc_intr_lock);

	return ret;
}

static int
cs4231_ebus_pint(void *cookie)
{
	struct cs4231_softc *sc = cookie;
	struct cs_transfer *t = &sc->sc_playback;

	mutex_spin_enter(&sc->sc_ad1848.sc_intr_lock);
	if (t->t_intr != NULL)
		(*t->t_intr)(t->t_arg);
	mutex_spin_exit(&sc->sc_ad1848.sc_intr_lock);
	return 0;
}

static int
cs4231_ebus_rint(void *cookie)
{
	struct cs4231_softc *sc = cookie;
	struct cs_transfer *t = &sc->sc_capture;

	mutex_spin_enter(&sc->sc_ad1848.sc_intr_lock);
	if (t->t_intr != NULL)
		(*t->t_intr)(t->t_arg);
	mutex_spin_exit(&sc->sc_ad1848.sc_intr_lock);
	return 0;
}
