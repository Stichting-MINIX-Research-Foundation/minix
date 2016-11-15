/*	$NetBSD: if_sm_pcmcia.c,v 1.55 2012/10/27 17:18:37 chs Exp $	*/

/*-
 * Copyright (c) 1997, 1998, 2000, 2004 The NetBSD Foundation, Inc.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_sm_pcmcia.c,v 1.55 2012/10/27 17:18:37 chs Exp $");

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

#include <sys/intr.h>
#include <sys/bus.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <dev/ic/smc91cxxreg.h>
#include <dev/ic/smc91cxxvar.h>

#include <dev/pcmcia/pcmciareg.h>
#include <dev/pcmcia/pcmciavar.h>
#include <dev/pcmcia/pcmciadevs.h>

int	sm_pcmcia_match(device_t, cfdata_t, void *);
int	sm_pcmcia_validate_config(struct pcmcia_config_entry *);
void	sm_pcmcia_attach(device_t, device_t, void *);
int	sm_pcmcia_detach(device_t, int);

struct sm_pcmcia_softc {
	struct	smc91cxx_softc sc_smc;		/* real "smc" softc */

	struct	pcmcia_function *sc_pf;		/* our PCMCIA function */
	void	*sc_ih;				/* interrupt cookie */

	int	sc_state;
#define	SM_PCMCIA_ATTACHED	3
};

CFATTACH_DECL_NEW(sm_pcmcia, sizeof(struct sm_pcmcia_softc),
    sm_pcmcia_match, sm_pcmcia_attach, sm_pcmcia_detach, smc91cxx_activate);

int	sm_pcmcia_enable(struct smc91cxx_softc *);
void	sm_pcmcia_disable(struct smc91cxx_softc *);

int	sm_pcmcia_ascii_enaddr(const char *, u_int8_t *);

const struct pcmcia_product sm_pcmcia_products[] = {
	{ PCMCIA_VENDOR_MEGAHERTZ2, PCMCIA_PRODUCT_MEGAHERTZ2_EM1144,
	  PCMCIA_CIS_INVALID },

	{ PCMCIA_VENDOR_MEGAHERTZ2, PCMCIA_PRODUCT_MEGAHERTZ2_XJACK,
	  PCMCIA_CIS_INVALID },

	{ PCMCIA_VENDOR_NEWMEDIA, PCMCIA_PRODUCT_NEWMEDIA_BASICS,
	  PCMCIA_CIS_INVALID },

#if 0
	{ PCMCIA_VENDOR_SMC, PCMCIA_PRODUCT_SMC_8020BT,
	  PCMCIA_CIS_INVALID },
#endif
	{ PCMCIA_VENDOR_PSION, PCMCIA_PRODUCT_PSION_GOLDCARD,
	  PCMCIA_CIS_INVALID },
};
const size_t sm_pcmcia_nproducts =
    sizeof(sm_pcmcia_products) / sizeof(sm_pcmcia_products[0]);

int
sm_pcmcia_match(device_t parent, cfdata_t match,
    void *aux)
{
	struct pcmcia_attach_args *pa = aux;

	/* This is to differentiate the serial function of some cards. */
	if (pa->pf->function != PCMCIA_FUNCTION_NETWORK)
		return (0);

	if (pcmcia_product_lookup(pa, sm_pcmcia_products, sm_pcmcia_nproducts,
	    sizeof(sm_pcmcia_products[0]), NULL))
		return (1);
	return (0);
}

int
sm_pcmcia_validate_config(struct pcmcia_config_entry *cfe)
{
	if (cfe->iftype != PCMCIA_IFTYPE_IO ||
	    cfe->num_memspace != 0 ||
	    cfe->num_iospace != 1 ||
	    cfe->iospace[0].length < SMC_IOSIZE)
		return (EINVAL);
	return (0);
}

void
sm_pcmcia_attach(device_t parent, device_t self, void *aux)
{
	struct sm_pcmcia_softc *psc = device_private(self);
	struct smc91cxx_softc *sc = &psc->sc_smc;
	struct pcmcia_attach_args *pa = aux;
	struct pcmcia_config_entry *cfe;
	u_int8_t enaddr[ETHER_ADDR_LEN];
	int error;

	sc->sc_dev = self;
	psc->sc_pf = pa->pf;

	error = pcmcia_function_configure(pa->pf, sm_pcmcia_validate_config);
	if (error) {
		aprint_error_dev(self, "configure failed, error=%d\n",
		    error);
		return;
	}

	cfe = pa->pf->cfe;
	sc->sc_bst = cfe->iospace[0].handle.iot;
	sc->sc_bsh = cfe->iospace[0].handle.ioh;

	error = sm_pcmcia_enable(sc);
	if (error)
		goto fail;

	sc->sc_enable = sm_pcmcia_enable;
	sc->sc_disable = sm_pcmcia_disable;

	/*
	 * First try to get the Ethernet address from FUNCE/LANNID tuple.
	 * If that fails, try one of the CIS info strings.
	 */
	if (pa->pf->pf_funce_lan_nidlen == ETHER_ADDR_LEN) {
		memcpy(enaddr, pa->pf->pf_funce_lan_nid, ETHER_ADDR_LEN);
	} else {
		if (!sm_pcmcia_ascii_enaddr(pa->pf->sc->card.cis1_info[3], enaddr) &&
		    !sm_pcmcia_ascii_enaddr(pa->pf->sc->card.cis1_info[2], enaddr))
			aprint_error_dev(self, "unable to get Ethernet address\n");
	}

	/* Perform generic intialization. */
	smc91cxx_attach(sc, enaddr);

	psc->sc_state = SM_PCMCIA_ATTACHED;
	sm_pcmcia_disable(sc);
	return;

fail:
	pcmcia_function_unconfigure(pa->pf);
}

int
sm_pcmcia_detach(device_t self, int flags)
{
	struct sm_pcmcia_softc *psc = device_private(self);
	int error;

	if (psc->sc_state != SM_PCMCIA_ATTACHED)
		return (0);

	error = smc91cxx_detach((device_t)&psc->sc_smc, flags);
	if (error)
		return (error);

	pcmcia_function_unconfigure(psc->sc_pf);

	return (0);
}

int
sm_pcmcia_ascii_enaddr(const char *cisstr, u_int8_t *myla)
{
	u_int8_t digit;
	int i;

	/* No CIS string. */
	if (cisstr == 0)
		return (0);

	memset(myla, 0, ETHER_ADDR_LEN);

	for (i = 0, digit = 0; i < (ETHER_ADDR_LEN * 2); i++) {
		if (cisstr[i] >= '0' && cisstr[i] <= '9')
			digit |= cisstr[i] - '0';
		else if (cisstr[i] >= 'a' && cisstr[i] <= 'f')
			digit |= (cisstr[i] - 'a') + 10;
		else if (cisstr[i] >= 'A' && cisstr[i] <= 'F')
			digit |= (cisstr[i] - 'A') + 10;
		else {
			/* Bogus digit!! */
			return (0);
		}

		/* Compensate for ordering of digits. */
		if (i & 1) {
			myla[i >> 1] = digit;
			digit = 0;
		} else
			digit <<= 4;
	}

	return (1);
}

int
sm_pcmcia_enable(struct smc91cxx_softc *sc)
{
	struct sm_pcmcia_softc *psc = (struct sm_pcmcia_softc *)sc;
	int error;

	/* Establish the interrupt handler. */
	psc->sc_ih = pcmcia_intr_establish(psc->sc_pf, IPL_NET, smc91cxx_intr,
	    sc);
	if (!psc->sc_ih)
		return (EIO);

	error = pcmcia_function_enable(psc->sc_pf);
	if (error) {
		pcmcia_intr_disestablish(psc->sc_pf, psc->sc_ih);
		psc->sc_ih = 0;
	}

	return (error);
}

void
sm_pcmcia_disable(struct smc91cxx_softc *sc)
{
	struct sm_pcmcia_softc *psc = (struct sm_pcmcia_softc *)sc;

	pcmcia_function_disable(psc->sc_pf);
	pcmcia_intr_disestablish(psc->sc_pf, psc->sc_ih);
	psc->sc_ih = 0;
}
