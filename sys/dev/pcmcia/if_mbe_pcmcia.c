/*	$NetBSD: if_mbe_pcmcia.c,v 1.46 2009/05/12 14:42:18 cegger Exp $	*/

/*-
 * Copyright (c) 1998, 2000, 2004 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Enami Tsugutomo.
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
__KERNEL_RCSID(0, "$NetBSD: if_mbe_pcmcia.c,v 1.46 2009/05/12 14:42:18 cegger Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_ether.h>
#include <net/if_media.h>

#include <sys/intr.h>
#include <sys/bus.h>

#include <dev/ic/mb86960reg.h>
#include <dev/ic/mb86960var.h>

#include <dev/pcmcia/pcmciareg.h>
#include <dev/pcmcia/pcmciavar.h>
#include <dev/pcmcia/pcmciadevs.h>

int	mbe_pcmcia_match(device_t, cfdata_t, void *);
int	mbe_pcmcia_validate_config(struct pcmcia_config_entry *);
void	mbe_pcmcia_attach(device_t, device_t, void *);
int	mbe_pcmcia_detach(device_t, int);

struct mbe_pcmcia_softc {
	struct	mb86960_softc sc_mb86960;	/* real "mb" softc */

	struct	pcmcia_function *sc_pf;		/* our PCMCIA function */
	void	*sc_ih;				/* interrupt cookie */

	int	sc_state;
#define	MBE_PCMCIA_ATTACHED	3
};

CFATTACH_DECL_NEW(mbe_pcmcia, sizeof(struct mbe_pcmcia_softc),
    mbe_pcmcia_match, mbe_pcmcia_attach, mbe_pcmcia_detach, mb86960_activate);

int	mbe_pcmcia_enable(struct mb86960_softc *);
void	mbe_pcmcia_disable(struct mb86960_softc *);

struct mbe_pcmcia_get_enaddr_args {
	uint8_t enaddr[ETHER_ADDR_LEN];
	int maddr;
};
int	mbe_pcmcia_get_enaddr_from_cis(struct pcmcia_tuple *, void *);
int	mbe_pcmcia_get_enaddr_from_mem(struct mbe_pcmcia_softc *,
	    struct mbe_pcmcia_get_enaddr_args *);
int	mbe_pcmcia_get_enaddr_from_io(struct mbe_pcmcia_softc *,
	    struct mbe_pcmcia_get_enaddr_args *);

static const struct mbe_pcmcia_product {
	struct pcmcia_product mpp_product;
	int		mpp_enet_maddr;
	int		mpp_flags;
#define MBH10302	0x0001			/* FUJITSU MBH10302 */
} mbe_pcmcia_products[] = {
	{ { PCMCIA_VENDOR_TDK, PCMCIA_PRODUCT_TDK_LAK_CD021BX,
	    PCMCIA_CIS_TDK_LAK_CD021BX },
	  -1, 0 },

	{ { PCMCIA_VENDOR_TDK, PCMCIA_PRODUCT_TDK_LAK_CF010,
	    PCMCIA_CIS_TDK_LAK_CF010 },
	  -1, 0 },

#if 0 /* XXX 86960-based? */
	{ { PCMCIA_VENDOR_TDK, PCMCIA_PRODUCT_TDK_LAK_DFL9610,
	    PCMCIA_CIS_TDK_DFL9610 },
	  -1, MBH10302 /* XXX */ },
#endif

	{ { PCMCIA_VENDOR_CONTEC, PCMCIA_PRODUCT_CONTEC_CNETPC,
	    PCMCIA_CIS_CONTEC_CNETPC },
	  -1, 0 },

	{ { PCMCIA_VENDOR_FUJITSU, PCMCIA_PRODUCT_FUJITSU_LA501,
	    PCMCIA_CIS_FUJITSU_LA501 },
	  -1, 0 },

	{ { PCMCIA_VENDOR_FUJITSU, PCMCIA_PRODUCT_FUJITSU_FMV_J181,
	    PCMCIA_CIS_FUJITSU_FMV_J181 },
	  -1, MBH10302 },

	{ { PCMCIA_VENDOR_FUJITSU, PCMCIA_PRODUCT_FUJITSU_FMV_J182,
	    PCMCIA_CIS_FUJITSU_FMV_J182 },
	  0xf2c, 0 },

	{ { PCMCIA_VENDOR_FUJITSU, PCMCIA_PRODUCT_FUJITSU_FMV_J182A,
	    PCMCIA_CIS_FUJITSU_FMV_J182A },
	  0x1cc, 0 },

	{ { PCMCIA_VENDOR_FUJITSU, PCMCIA_PRODUCT_FUJITSU_ITCFJ182A,
	    PCMCIA_CIS_FUJITSU_ITCFJ182A },
	  0x1cc, 0 },

	{ { PCMCIA_VENDOR_FUJITSU, PCMCIA_PRODUCT_FUJITSU_LA10S,
	    PCMCIA_CIS_FUJITSU_LA10S },
	  -1, 0 },

	{ { PCMCIA_VENDOR_RATOC, PCMCIA_PRODUCT_RATOC_REX_R280,
	    PCMCIA_CIS_RATOC_REX_R280 },
	  0x1fc, 0 },
};
static const size_t mbe_pcmcia_nproducts = __arraycount(mbe_pcmcia_products);

int
mbe_pcmcia_match(device_t parent, cfdata_t cf, void *aux)
{
	struct pcmcia_attach_args *pa = aux;

	if (pcmcia_product_lookup(pa, mbe_pcmcia_products, mbe_pcmcia_nproducts,
	    sizeof(mbe_pcmcia_products[0]), NULL))
		return 1;
	return 0;
}

int
mbe_pcmcia_validate_config(struct pcmcia_config_entry *cfe)
{

	if (cfe->iftype != PCMCIA_IFTYPE_IO ||
	    cfe->num_iospace < 1)
		return EINVAL;
	return 0;
}

void
mbe_pcmcia_attach(device_t parent, device_t self, void *aux)
{
	struct mbe_pcmcia_softc *psc = device_private(self);
	struct mb86960_softc *sc = &psc->sc_mb86960;
	struct pcmcia_attach_args *pa = aux;
	struct pcmcia_config_entry *cfe;
	struct mbe_pcmcia_get_enaddr_args pgea;
	const struct mbe_pcmcia_product *mpp;
	int error;

	sc->sc_dev = self;
	psc->sc_pf = pa->pf;

	error = pcmcia_function_configure(pa->pf, mbe_pcmcia_validate_config);
	if (error) {
		aprint_error_dev(self, "configure failed, error=%d\n",
		    error);
		return;
	}

	cfe = pa->pf->cfe;
	sc->sc_bst = cfe->iospace[0].handle.iot;
	sc->sc_bsh = cfe->iospace[0].handle.ioh;

	mpp = pcmcia_product_lookup(pa, mbe_pcmcia_products,
	    mbe_pcmcia_nproducts, sizeof(mbe_pcmcia_products[0]), NULL);
	if (!mpp)
		panic("%s: impossible", __func__);

	/* Read station address from io/mem or CIS. */
	if (mpp->mpp_enet_maddr >= 0) {
		pgea.maddr = mpp->mpp_enet_maddr;
		if (mbe_pcmcia_get_enaddr_from_mem(psc, &pgea) != 0) {
			aprint_error_dev(self, "couldn't get ethernet address "
			    "from memory\n");
			goto fail;
		}
	} else if (mpp->mpp_flags & MBH10302) {
		bus_space_write_1(sc->sc_bst, sc->sc_bsh, FE_MBH0 ,
				  FE_MBH0_MASK | FE_MBH0_INTR_ENABLE);
		if (mbe_pcmcia_get_enaddr_from_io(psc, &pgea) != 0) {
			aprint_error_dev(self,
			    "couldn't get ethernet address from i/o\n");
			goto fail;
		}
	} else {
		if (pa->pf->pf_funce_lan_nidlen != ETHER_ADDR_LEN) {
			aprint_error_dev(self,
			    "couldn't get ethernet address from CIS\n");
			goto fail;
		}
		memcpy(pgea.enaddr, pa->pf->pf_funce_lan_nid, ETHER_ADDR_LEN);
	}

	/* Perform generic initialization. */
	if (mpp->mpp_flags & MBH10302)
		sc->sc_flags |= FE_FLAGS_MB86960;

	sc->sc_enable = mbe_pcmcia_enable;
	sc->sc_disable = mbe_pcmcia_disable;

	error = mbe_pcmcia_enable(sc);
	if (error)
		goto fail;

	mb86960_attach(sc, pgea.enaddr);
	mb86960_config(sc, NULL, 0, 0);

	mbe_pcmcia_disable(sc);
	psc->sc_state = MBE_PCMCIA_ATTACHED;
	return;

fail:
	pcmcia_function_unconfigure(pa->pf);
}

int
mbe_pcmcia_detach(device_t self, int flags)
{
	struct mbe_pcmcia_softc *psc = device_private(self);
	int error;

	if (psc->sc_state != MBE_PCMCIA_ATTACHED)
		return 0;

	error = mb86960_detach(&psc->sc_mb86960);
	if (error)
		return error;

	pcmcia_function_unconfigure(psc->sc_pf);

	return 0;
}

int
mbe_pcmcia_enable(struct mb86960_softc *sc)
{
	struct mbe_pcmcia_softc *psc = (struct mbe_pcmcia_softc *)sc;
	int error;

	/* Establish the interrupt handler. */
	psc->sc_ih = pcmcia_intr_establish(psc->sc_pf, IPL_NET, mb86960_intr,
	    sc);
	if (!psc->sc_ih)
		return EIO;

	error = pcmcia_function_enable(psc->sc_pf);
	if (error) {
		pcmcia_intr_disestablish(psc->sc_pf, psc->sc_ih);
		psc->sc_ih = 0;
	}

	return error;
}

void
mbe_pcmcia_disable(struct mb86960_softc *sc)
{
	struct mbe_pcmcia_softc *psc = (struct mbe_pcmcia_softc *)sc;

	pcmcia_function_disable(psc->sc_pf);
	pcmcia_intr_disestablish(psc->sc_pf, psc->sc_ih);
	psc->sc_ih = 0;
}

int
mbe_pcmcia_get_enaddr_from_io(struct mbe_pcmcia_softc *psc,
    struct mbe_pcmcia_get_enaddr_args *ea)
{
	struct mb86960_softc *sc = &psc->sc_mb86960;
	int i;

	for (i = 0; i < ETHER_ADDR_LEN; i++)
		ea->enaddr[i] = bus_space_read_1(sc->sc_bst, sc->sc_bsh,
		    FE_MBH_ENADDR + i);
	return 0;
}

int
mbe_pcmcia_get_enaddr_from_mem(struct mbe_pcmcia_softc *psc,
    struct mbe_pcmcia_get_enaddr_args *ea)
{
	struct mb86960_softc *sc = &psc->sc_mb86960;
	struct pcmcia_mem_handle pcmh;
	bus_size_t offset;
	int i, mwindow, rv = 1;

	if (ea->maddr < 0)
		goto bad_memaddr;

	if (pcmcia_mem_alloc(psc->sc_pf, ETHER_ADDR_LEN * 2, &pcmh)) {
		aprint_error_dev(sc->sc_dev, "can't alloc mem for enet addr\n");
		goto memalloc_failed;
	}

	if (pcmcia_mem_map(psc->sc_pf, PCMCIA_MEM_ATTR, ea->maddr,
	    ETHER_ADDR_LEN * 2, &pcmh, &offset, &mwindow)) {
		aprint_error_dev(sc->sc_dev, "can't map mem for enet addr\n");
		goto memmap_failed;
	}

	for (i = 0; i < ETHER_ADDR_LEN; i++)
		ea->enaddr[i] = bus_space_read_1(pcmh.memt, pcmh.memh,
		    offset + (i * 2));

	rv = 0;
	pcmcia_mem_unmap(psc->sc_pf, mwindow);
memmap_failed:
	pcmcia_mem_free(psc->sc_pf, &pcmh);
memalloc_failed:
bad_memaddr:

	return rv;
}
