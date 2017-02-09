/*	$NetBSD: if_tr_pcmcia.c,v 1.25 2012/10/27 17:18:37 chs Exp $	*/

/*
 * Copyright (c) 2000 Soren S. Jorvang.  All rights reserved.
 * Copyright (c) 2000 Onno van der Linden.  All rights reserved.
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
 * PCMCIA attachment for the following Tropic-based cards.
 *
 * o IBM Token Ring 16/4 Credit Card Adapter
 * o IBM Token Ring Auto 16/4 Credit Card Adapter
 * o IBM Turbo 16/4 Token Ring PC Card
 * o 3Com TokenLink Velocity PC Card
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_tr_pcmcia.c,v 1.25 2012/10/27 17:18:37 chs Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/syslog.h>
#include <sys/select.h>
#include <sys/device.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_ether.h>
#include <net/if_media.h>

#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/ic/tropicreg.h>
#include <dev/ic/tropicvar.h>

#include <dev/pcmcia/pcmciareg.h>
#include <dev/pcmcia/pcmciavar.h>
#include <dev/pcmcia/pcmciadevs.h>

#define TR_SRAM_SIZE	(16 * 1024)	/* Really 64KB, but conserve iomem. */

/*
 * XXX How do host/PCMCIA/cbb memory spaces actually relate?
 */
#ifndef TR_PCMCIA_SRAM_ADDR
#define TR_PCMCIA_SRAM_ADDR	0xc8000
#endif
#ifndef TR_PCMCIA_MMIO_ADDR
#define TR_PCMCIA_MMIO_ADDR	0xcc000
#endif

struct tr_pcmcia_softc {
	struct	tr_softc sc_tr;

	struct	pcmcia_io_handle sc_pioh;
	int	sc_pio_window;
	struct	pcmcia_mem_handle sc_sramh;
	int	sc_sram_window;
	struct	pcmcia_mem_handle sc_mmioh;
	int	sc_mmio_window;
	struct	pcmcia_function *sc_pf;
};

static int	tr_pcmcia_match(device_t, cfdata_t, void *);
static void	tr_pcmcia_attach(device_t, device_t, void *);
static int	tr_pcmcia_detach(device_t, int);
static int	tr_pcmcia_enable(struct tr_softc *);
static int	tr_pcmcia_mediachange(struct tr_softc *);
static void	tr_pcmcia_mediastatus(struct tr_softc *, struct ifmediareq *);
static void	tr_pcmcia_disable(struct tr_softc *);
static void	tr_pcmcia_setup(struct tr_softc *);

CFATTACH_DECL_NEW(tr_pcmcia, sizeof(struct tr_pcmcia_softc),
    tr_pcmcia_match, tr_pcmcia_attach, tr_pcmcia_detach, tr_activate);

static int
tr_pcmcia_match(device_t parent, cfdata_t match,
    void *aux)
{
	struct pcmcia_attach_args *pa = aux;

	if (pa->manufacturer == PCMCIA_VENDOR_IBM)
		switch (pa->product) {
		case PCMCIA_PRODUCT_IBM_TROPIC:
			return 1;
		}

	return 0;
}

static void
tr_pcmcia_attach(device_t parent, device_t self, void *aux)
{
	struct tr_pcmcia_softc *psc = device_private(self);
	struct tr_softc *sc = &psc->sc_tr;
	struct pcmcia_attach_args *pa = aux;
	struct pcmcia_config_entry *cfe;
	bus_size_t offset;

	psc->sc_pf = pa->pf;

	cfe = SIMPLEQ_FIRST(&pa->pf->cfe_head);

	pcmcia_function_init(pa->pf, cfe);
	if (pcmcia_function_enable(pa->pf) != 0) {
		aprint_error_dev(self, "function enable failed\n");
		return;
	}

	if (pcmcia_io_alloc(pa->pf, cfe->iospace[0].start,
	    cfe->iospace[0].length, cfe->iospace[0].length, &psc->sc_pioh) != 0) {
		aprint_error_dev(self, "can't allocate pio space\n");
		goto fail1;
	}
	if (pcmcia_io_map(psc->sc_pf, PCMCIA_WIDTH_IO8,	/* XXX _AUTO? */
	    &psc->sc_pioh, &psc->sc_pio_window) != 0) {
		aprint_error_dev(self, "can't map pio space\n");
		goto fail2;
	}

	if (pcmcia_mem_alloc(psc->sc_pf, TR_SRAM_SIZE, &psc->sc_sramh) != 0) {
		aprint_error_dev(self, "can't allocate sram space\n");
		goto fail3;
	}
        if (pcmcia_mem_map(psc->sc_pf, PCMCIA_MEM_COMMON, TR_PCMCIA_SRAM_ADDR,
	    TR_SRAM_SIZE, &psc->sc_sramh, &offset, &psc->sc_sram_window) != 0) {
		aprint_error_dev(self, "can't map sram space\n");
		goto fail4;
        }

	if (pcmcia_mem_alloc(psc->sc_pf, TR_MMIO_SIZE, &psc->sc_mmioh) != 0) {
		aprint_error_dev(self, "can't allocate mmio space\n");
		goto fail5;
		return;
	}
        if (pcmcia_mem_map(psc->sc_pf, PCMCIA_MEM_COMMON, TR_PCMCIA_MMIO_ADDR,
	    TR_MMIO_SIZE, &psc->sc_mmioh, &offset, &psc->sc_mmio_window) != 0) {
		aprint_error_dev(self, "can't map mmio space\n");
		goto fail6;
        }

	sc->sc_piot = psc->sc_pioh.iot;
	sc->sc_pioh = psc->sc_pioh.ioh;
	sc->sc_memt = psc->sc_sramh.memt;
	sc->sc_sramh = psc->sc_sramh.memh;
	sc->sc_mmioh = psc->sc_mmioh.memh;
	sc->sc_init_status = RSP_16;
	sc->sc_memwinsz = TR_SRAM_SIZE;
	sc->sc_memsize = TR_SRAM_SIZE;
	sc->sc_memreserved = 0;
	sc->sc_aca = TR_ACA_OFFSET;
	sc->sc_maddr = TR_PCMCIA_SRAM_ADDR;
	sc->sc_mediastatus = tr_pcmcia_mediastatus;
	sc->sc_mediachange = tr_pcmcia_mediachange;
	sc->sc_enable = tr_pcmcia_enable;
	sc->sc_disable = tr_pcmcia_disable;

	tr_pcmcia_setup(sc);
	if (tr_reset(sc) == 0)
		(void)tr_attach(sc);

	pcmcia_function_disable(pa->pf);
	sc->sc_enabled = 0;
	return;

fail6:
	pcmcia_mem_free(psc->sc_pf, &psc->sc_mmioh);
fail5:
	pcmcia_mem_unmap(psc->sc_pf, psc->sc_sram_window);
fail4:
	pcmcia_mem_free(psc->sc_pf, &psc->sc_sramh);
fail3:
	pcmcia_io_unmap(psc->sc_pf, psc->sc_pio_window);
fail2:
	pcmcia_io_free(psc->sc_pf, &psc->sc_pioh);
fail1:
	pcmcia_function_disable(pa->pf);
	sc->sc_enabled = 0;
}

static int
tr_pcmcia_enable(struct tr_softc *sc)
{
	struct tr_pcmcia_softc *psc = (struct tr_pcmcia_softc *) sc;
	int ret;

	sc->sc_ih = pcmcia_intr_establish(psc->sc_pf, IPL_NET, tr_intr, psc);
	if (sc->sc_ih == NULL) {
		aprint_error_dev(psc->sc_tr.sc_dev, "couldn't establish interrupt\n");
		return 1;
	}

        ret = pcmcia_function_enable(psc->sc_pf);
	if (ret != 0)
		return ret;

	tr_pcmcia_setup(sc);

	if (tr_reset(sc))
		return 1;
	if (tr_config(sc))
		return 1;

	return 0;
}

static void
tr_pcmcia_disable(struct tr_softc *sc)
{
	struct tr_pcmcia_softc *psc = (struct tr_pcmcia_softc *) sc;

	pcmcia_function_disable(psc->sc_pf);
	pcmcia_intr_disestablish(psc->sc_pf, sc->sc_ih);
}

static int
tr_pcmcia_mediachange(struct tr_softc *sc)
{
	int setspeed = 0;

	if (IFM_TYPE(sc->sc_media.ifm_media) != IFM_TOKEN)
		return EINVAL;

	switch (IFM_SUBTYPE(sc->sc_media.ifm_media)) {
	case IFM_TOK_STP16:
	case IFM_TOK_UTP16:
		if ((sc->sc_init_status & RSP_16) == 0)
			setspeed = 1;
		break;
	case IFM_TOK_STP4:
	case IFM_TOK_UTP4:
		if ((sc->sc_init_status & RSP_16) != 0)
			setspeed = 1;
		break;
	}
	if (setspeed != 0) {
		tr_stop(sc);
		if (sc->sc_enabled)
			tr_pcmcia_disable(sc);
		sc->sc_init_status ^= RSP_16;		/* XXX 100 Mbit/s */
		if (sc->sc_enabled)
			tr_pcmcia_enable(sc);
	}
	/*
	 * XXX Handle Early Token Release !!!!
	 */

	return 0;
}

/*
 * XXX Copy of tropic_mediastatus()
 */
static void
tr_pcmcia_mediastatus(struct tr_softc *sc, struct ifmediareq *ifmr)
{
	struct ifmedia	*ifm = &sc->sc_media;

	ifmr->ifm_active = ifm->ifm_cur->ifm_media;
}

int
tr_pcmcia_detach(device_t self, int flags)
{
	struct tr_pcmcia_softc *psc = device_private(self);
	int rv;

	rv = tr_detach(self, flags);

	if (rv == 0) {
		pcmcia_mem_unmap(psc->sc_pf, psc->sc_mmio_window);
		pcmcia_mem_free(psc->sc_pf, &psc->sc_mmioh);
		pcmcia_mem_unmap(psc->sc_pf, psc->sc_sram_window);
		pcmcia_mem_free(psc->sc_pf, &psc->sc_sramh);
		pcmcia_io_unmap(psc->sc_pf, psc->sc_pio_window);
		pcmcia_io_free(psc->sc_pf, &psc->sc_pioh);
	}

	return rv;
}

static void
tr_pcmcia_setup(struct tr_softc *sc)
{
	int s;

	bus_space_write_1(sc->sc_piot, sc->sc_pioh, 0,
	    (TR_PCMCIA_MMIO_ADDR >> 16) & 0x0f);

	bus_space_write_1(sc->sc_piot, sc->sc_pioh, 0,
	    0x10 | ((TR_PCMCIA_MMIO_ADDR >> 12) & 0x0e));

	/* XXX Magick */
	bus_space_write_1(sc->sc_piot, sc->sc_pioh, 0, 0x20 | 0x06);

	/* 0 << 2 for 8K, 1 << 2 for 16K, 2 << 2 for 32K, 3 << 2 for 64K */
	/* 0 << 1 for 4Mbit/s, 1 << 1 for 16Mbit/s */
	/* 0 for primary, 1 for alternate */
	s = sc->sc_init_status & RSP_16 ? (1 << 1) : (0 << 1);
	bus_space_write_1(sc->sc_piot, sc->sc_pioh, 0, 0x30 | 0x04 | s);

	/* Release the card. */
	bus_space_write_1(sc->sc_piot, sc->sc_pioh, 0, 0x40);
}
