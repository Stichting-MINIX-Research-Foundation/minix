/*	$NetBSD: if_ep_pcmcia.c,v 1.63 2009/09/05 12:31:00 tsutsui Exp $	*/

/*-
 * Copyright (c) 1998, 2000, 2004 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center, and by Charles M. Hannum.
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
 * Copyright (c) 1997 Marc Horowitz.  All rights reserved.
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
 *	This product includes software developed by Marc Horowitz.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
__KERNEL_RCSID(0, "$NetBSD: if_ep_pcmcia.c,v 1.63 2009/09/05 12:31:00 tsutsui Exp $");

#include <sys/param.h>
#include <sys/systm.h>
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

#include <sys/cpu.h>
#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/mii/miivar.h>

#include <dev/ic/elink3var.h>
#include <dev/ic/elink3reg.h>

#include <dev/pcmcia/pcmciareg.h>
#include <dev/pcmcia/pcmciavar.h>
#include <dev/pcmcia/pcmciadevs.h>

int	ep_pcmcia_match(device_t, cfdata_t, void *);
void	ep_pcmcia_attach(device_t, device_t, void *);
int	ep_pcmcia_detach(device_t, int);

int	ep_pcmcia_get_enaddr(struct pcmcia_tuple *, void *);
int	ep_pcmcia_enable(struct ep_softc *);
void	ep_pcmcia_disable(struct ep_softc *);

void	ep_pcmcia_disable1(struct ep_softc *);

struct ep_pcmcia_softc {
	struct ep_softc sc_ep;			/* real "ep" softc */

	/* PCMCIA-specific goo */
	struct pcmcia_io_handle sc_pcioh;	/* PCMCIA i/o space info */
	int sc_io_window;			/* our i/o window */
	struct pcmcia_function *sc_pf;		/* our PCMCIA function */
};

CFATTACH_DECL_NEW(ep_pcmcia, sizeof(struct ep_pcmcia_softc),
    ep_pcmcia_match, ep_pcmcia_attach, ep_pcmcia_detach, ep_activate);

const struct ep_pcmcia_product {
	struct pcmcia_product epp_product;
	u_short		epp_chipset;	/* 3Com chipset used */
	int		epp_flags;	/* initial softc flags */
} ep_pcmcia_products[] = {
	{ { PCMCIA_VENDOR_3COM, PCMCIA_PRODUCT_3COM_3C562,
	    PCMCIA_CIS_INVALID },
	  ELINK_CHIPSET_3C509, 0 },

	{ { PCMCIA_VENDOR_3COM, PCMCIA_PRODUCT_3COM_3C589,
	    PCMCIA_CIS_INVALID },
	  ELINK_CHIPSET_3C509, 0 },

	{ { PCMCIA_VENDOR_3COM, PCMCIA_PRODUCT_3COM_3CXEM556,
	    PCMCIA_CIS_INVALID },
	  ELINK_CHIPSET_3C509, 0 },

	{ { PCMCIA_VENDOR_3COM, PCMCIA_PRODUCT_3COM_3CXEM556INT,
	    PCMCIA_CIS_INVALID },
	  ELINK_CHIPSET_3C509, 0 },

	{ { PCMCIA_VENDOR_3COM, PCMCIA_PRODUCT_3COM_3C574,
	    PCMCIA_CIS_INVALID },
	  ELINK_CHIPSET_ROADRUNNER, ELINK_FLAGS_MII },

	{ { PCMCIA_VENDOR_3COM, PCMCIA_PRODUCT_3COM_3CCFEM556BI,
	    PCMCIA_CIS_INVALID },
	  ELINK_CHIPSET_ROADRUNNER, ELINK_FLAGS_MII },

	{ { PCMCIA_VENDOR_3COM, PCMCIA_PRODUCT_3COM_3C1,
	    PCMCIA_CIS_INVALID },
	  ELINK_CHIPSET_3C509, 0 },
};
const size_t ep_pcmcia_nproducts =
    sizeof(ep_pcmcia_products) / sizeof(ep_pcmcia_products[0]);

int
ep_pcmcia_match(device_t parent, cfdata_t match, void *aux)
{
	struct pcmcia_attach_args *pa = aux;

	/* This is to differentiate the serial function of some cards. */
	if (pa->pf->function != PCMCIA_FUNCTION_NETWORK)
		return 0;

	if (pcmcia_product_lookup(pa, ep_pcmcia_products, ep_pcmcia_nproducts,
	    sizeof(ep_pcmcia_products[0]), NULL))
		return 1;
	return 0;
}

int
ep_pcmcia_enable(struct ep_softc *sc)
{
	struct ep_pcmcia_softc *psc = (struct ep_pcmcia_softc *) sc;
	struct pcmcia_function *pf = psc->sc_pf;
	int error;

	/* establish the interrupt. */
	sc->sc_ih = pcmcia_intr_establish(pf, IPL_NET, epintr, sc);
	if (!sc->sc_ih)
		return EIO;

	error = pcmcia_function_enable(pf);
	if (error) {
		pcmcia_intr_disestablish(pf, sc->sc_ih);
		sc->sc_ih = 0;
		return error;
	}

	if ((psc->sc_pf->sc->card.product == PCMCIA_PRODUCT_3COM_3C562) ||
	    (psc->sc_pf->sc->card.product == PCMCIA_PRODUCT_3COM_3CXEM556) ||
	    (psc->sc_pf->sc->card.product == PCMCIA_PRODUCT_3COM_3CXEM556INT)) {
		int reg;

		/* turn off the serial-disable bit */

		reg = pcmcia_ccr_read(pf, PCMCIA_CCR_OPTION);
		if (reg & 0x08) {
			reg &= ~0x08;
			pcmcia_ccr_write(pf, PCMCIA_CCR_OPTION, reg);
		}

	}

	return 0;
}

void
ep_pcmcia_disable(struct ep_softc *sc)
{
	struct ep_pcmcia_softc *psc = (struct ep_pcmcia_softc *) sc;

	pcmcia_function_disable(psc->sc_pf);
	pcmcia_intr_disestablish(psc->sc_pf, sc->sc_ih);
	sc->sc_ih = 0;
}

void
ep_pcmcia_attach(device_t parent, device_t self, void *aux)
{
	struct ep_pcmcia_softc *psc = device_private(self);
	struct ep_softc *sc = &psc->sc_ep;
	struct pcmcia_attach_args *pa = aux;
	struct pcmcia_config_entry *cfe;
	const struct ep_pcmcia_product *epp;
	u_int8_t myla[ETHER_ADDR_LEN];
	u_int8_t *enaddr = NULL;
	int i;
	int error;

	sc->sc_dev = self;
	psc->sc_pf = pa->pf;

	SIMPLEQ_FOREACH(cfe, &pa->pf->cfe_head, cfe_list) {
		if (cfe->num_memspace != 0)
			continue;
		if (cfe->num_iospace != 1)
			continue;

		if (pa->product == PCMCIA_PRODUCT_3COM_3C562) {
			/*
			 * the 3c562 can only use 0x??00-0x??7f
			 * according to the Linux driver
			 */

			/*
			 * 3c562 i/o may decodes address line not only A0-3
			 * but also A7.  Anyway, we must sweep at most
			 * [0x0000, 0x0100).  The address higher is given by a
			 * pcmcia bridge.  But pcmcia bus-space allocation
			 * function implies cards will decode 10-bit address
			 * line.  So we must search [0x0000, 0x0400).
			 *
			 * XXX: We must not check the bunch of I/O space range
			 * [0x400*n, 0x300 + 0x400*n) because they are
			 * reserved for legacy ISA devices and their alias
			 * images on PC/AT architecture.
			 */
			for (i = 0x0300; i < 0x0380; i += 0x10) {
				if (pcmcia_io_alloc(pa->pf, i,
				    cfe->iospace[0].length,
				    cfe->iospace[0].length,
				    &psc->sc_pcioh) == 0)
					break;
			}
			if (i != 0x0380)
				break;
		} else {
			if (pcmcia_io_alloc(pa->pf, cfe->iospace[0].start,
			    cfe->iospace[0].length, cfe->iospace[0].length,
			    &psc->sc_pcioh) == 0)
				break;
		}
	}
	if (!cfe) {
		aprint_error_dev(self, "failed to allocate I/O space\n");
		goto ioalloc_failed;
	}

	sc->sc_iot = psc->sc_pcioh.iot;
	sc->sc_ioh = psc->sc_pcioh.ioh;

	/* Enable the card. */
	pcmcia_function_init(pa->pf, cfe);

	if (pcmcia_io_map(pa->pf, ((cfe->flags & PCMCIA_CFE_IO16) ?
	    PCMCIA_WIDTH_AUTO : PCMCIA_WIDTH_IO8), &psc->sc_pcioh,
	    &psc->sc_io_window)) {
		aprint_error_dev(self, "can't map i/o space\n");
		goto iomap_failed;
	}

	error = ep_pcmcia_enable(sc);
	if (error)
		goto enable_failed;
	sc->enabled = 1;

	switch (pa->product) {
	case PCMCIA_PRODUCT_3COM_3C562:
		/*
		 * 3c562a-c use this; 3c562d does it in the regular way.
		 * we might want to check the revision and produce a warning
		 * in the future.
		 */
		/* FALLTHROUGH */
	case PCMCIA_PRODUCT_3COM_3C574:
	case PCMCIA_PRODUCT_3COM_3CCFEM556BI:
		/*
		 * Apparently, some 3c574s do it this way, as well.
		 */
		if (pcmcia_scan_cis(parent, ep_pcmcia_get_enaddr, myla))
			enaddr = myla;
		break;
	}

	epp = pcmcia_product_lookup(pa, ep_pcmcia_products, ep_pcmcia_nproducts,
	    sizeof(ep_pcmcia_products[0]), NULL);
	if (!epp)
		panic("ep_pcmcia_attach: impossible");

	sc->bustype = ELINK_BUS_PCMCIA;
	sc->ep_flags = epp->epp_flags;

	sc->enable = ep_pcmcia_enable;
	sc->disable = ep_pcmcia_disable;

	if (epconfig(sc, epp->epp_chipset, enaddr))
		aprint_error_dev(self, "couldn't configure controller\n");

	sc->enabled = 0;
	ep_pcmcia_disable(sc);
	return;

enable_failed:
	pcmcia_io_unmap(psc->sc_pf, psc->sc_io_window);
iomap_failed:
	pcmcia_io_free(psc->sc_pf, &psc->sc_pcioh);
ioalloc_failed:
	psc->sc_io_window = -1;
}

int
ep_pcmcia_detach(device_t self, int flags)
{
	struct ep_pcmcia_softc *psc = device_private(self);
	int rv;

	if (psc->sc_io_window == -1)
		/* Nothing to detach. */
		return 0;

	rv = ep_detach(self, flags);
	if (rv != 0)
		return rv;

	/* Unmap our i/o window. */
	pcmcia_io_unmap(psc->sc_pf, psc->sc_io_window);

	/* Free our i/o space. */
	pcmcia_io_free(psc->sc_pf, &psc->sc_pcioh);

	return 0;
}

int
ep_pcmcia_get_enaddr(struct pcmcia_tuple *tuple, void *arg)
{
	u_int8_t *myla = arg;
	int i;

	/* this is 3c562a-c magic */
	if (tuple->code == 0x88) {
		if (tuple->length < ETHER_ADDR_LEN)
			return 0;

		for (i = 0; i < ETHER_ADDR_LEN; i += 2) {
			myla[i] = pcmcia_tuple_read_1(tuple, i + 1);
			myla[i + 1] = pcmcia_tuple_read_1(tuple, i);
		}

		return 1;
	}
	return 0;
}
