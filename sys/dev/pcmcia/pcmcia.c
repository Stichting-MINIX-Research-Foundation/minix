/*	$NetBSD: pcmcia.c,v 1.94 2011/07/26 22:24:36 dyoung Exp $	*/

/*
 * Copyright (c) 2004 Charles M. Hannum.  All rights reserved.
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
 *      This product includes software developed by Charles M. Hannum.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
__KERNEL_RCSID(0, "$NetBSD: pcmcia.c,v 1.94 2011/07/26 22:24:36 dyoung Exp $");

#include "opt_pcmciaverbose.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <net/if.h>

#include <dev/pcmcia/pcmciareg.h>
#include <dev/pcmcia/pcmciachip.h>
#include <dev/pcmcia/pcmciavar.h>
#ifdef IT8368E_LEGACY_MODE /* XXX -uch */
#include <arch/hpcmips/dev/it8368var.h>
#endif

#include "locators.h"

#ifdef PCMCIADEBUG
int	pcmcia_debug = 0;
#define	DPRINTF(arg) if (pcmcia_debug) printf arg
#else
#define	DPRINTF(arg)
#endif

#ifdef PCMCIAVERBOSE
int	pcmcia_verbose = 1;
#else
int	pcmcia_verbose = 0;
#endif

int	pcmcia_match(device_t, cfdata_t, void *);
void	pcmcia_attach(device_t, device_t, void *);
int	pcmcia_detach(device_t, int);
int	pcmcia_rescan(device_t, const char *, const int *);
void	pcmcia_childdetached(device_t, device_t);
int	pcmcia_print(void *, const char *);

CFATTACH_DECL3_NEW(pcmcia, sizeof(struct pcmcia_softc),
    pcmcia_match, pcmcia_attach, pcmcia_detach, NULL,
    pcmcia_rescan, pcmcia_childdetached, DVF_DETACH_SHUTDOWN);

int
pcmcia_ccr_read(struct pcmcia_function *pf, int ccr)
{

	return (bus_space_read_1(pf->pf_ccrt, pf->pf_ccrh,
	    pf->pf_ccr_offset + ccr * 2));
}

void
pcmcia_ccr_write(struct pcmcia_function *pf, int ccr, int val)
{

	if (pf->ccr_mask & (1 << ccr)) {
		bus_space_write_1(pf->pf_ccrt, pf->pf_ccrh,
		    pf->pf_ccr_offset + ccr * 2, val);
	}
}

int
pcmcia_match(device_t parent, cfdata_t match, void *aux)
{
	struct pcmciabus_attach_args *paa = aux;

	if (strcmp(paa->paa_busname, match->cf_name)) {
	    return 0;
	}
	/* if the autoconfiguration got this far, there's a socket here */
	return (1);
}

void
pcmcia_attach(device_t parent, device_t self, void *aux)
{
	struct pcmciabus_attach_args *paa = aux;
	struct pcmcia_softc *sc = device_private(self);

	aprint_naive("\n");
	aprint_normal("\n");

	sc->dev = self;
	sc->pct = paa->pct;
	sc->pch = paa->pch;

	sc->ih = NULL;

	if (!pmf_device_register(self, NULL, NULL))
		aprint_error_dev(self, "couldn't establish power handler\n");
}

int
pcmcia_detach(device_t self, int flags)
{
	int rc;

	if ((rc = config_detach_children(self, flags)) != 0)
		return rc;

	pmf_device_deregister(self);
	return 0;
}

int
pcmcia_card_attach(device_t dev)
{
	struct pcmcia_softc *sc = device_private(dev);
	struct pcmcia_function *pf;
	int error;
	static const int wildcard[PCMCIACF_NLOCS] = {
		PCMCIACF_FUNCTION_DEFAULT
	};

	/*
	 * this is here so that when socket_enable calls gettype, trt happens
	 */
	SIMPLEQ_FIRST(&sc->card.pf_head) = NULL;

	pcmcia_socket_enable(dev);

	pcmcia_read_cis(sc);
	pcmcia_check_cis_quirks(sc);

#if 1 /* XXX remove this, done below ??? */
	/*
	 * bail now if the card has no functions, or if there was an error in
	 * the cis.
	 */
	if (sc->card.error ||
	    SIMPLEQ_EMPTY(&sc->card.pf_head)) {
		printf("%s: card appears to have bogus CIS\n",
		    device_xname(sc->dev));
		error = EIO;
		goto done;
	}
#endif

	if (pcmcia_verbose)
		pcmcia_print_cis(sc);

	SIMPLEQ_FOREACH(pf, &sc->card.pf_head, pf_list) {
		if (SIMPLEQ_EMPTY(&pf->cfe_head))
			continue;

#ifdef DIAGNOSTIC
		if (pf->child != NULL) {
			printf("%s: %s still attached to function %d!\n",
			    device_xname(sc->dev), device_xname(pf->child),
			    pf->number);
			panic("pcmcia_card_attach");
		}
#endif
		pf->sc = sc;
		pf->child = NULL;
		pf->cfe = NULL;
		pf->pf_ih = NULL;
	}

	error = pcmcia_rescan(dev, "pcmcia", wildcard);
done:
	pcmcia_socket_disable(dev);
	return (error);
}

int
pcmcia_rescan(device_t self, const char *ifattr,
    const int *locators)
{
	struct pcmcia_softc *sc = device_private(self);
	struct pcmcia_function *pf;
	struct pcmcia_attach_args paa;
	int locs[PCMCIACF_NLOCS];

	if (sc->card.error ||
	    SIMPLEQ_EMPTY(&sc->card.pf_head)) {
		/* XXX silently ignore if no card present? */
		return (EIO);
	}

	SIMPLEQ_FOREACH(pf, &sc->card.pf_head, pf_list) {
		if (SIMPLEQ_EMPTY(&pf->cfe_head))
			continue;

		if ((locators[PCMCIACF_FUNCTION] != PCMCIACF_FUNCTION_DEFAULT)
		    && (locators[PCMCIACF_FUNCTION] != pf->number))
			continue;

		if (pf->child)
			continue;

		locs[PCMCIACF_FUNCTION] = pf->number;

		paa.manufacturer = sc->card.manufacturer;
		paa.product = sc->card.product;
		paa.card = &sc->card;
		paa.pf = pf;

		pf->child = config_found_sm_loc(self, "pcmcia", locs, &paa,
						pcmcia_print,
						config_stdsubmatch);
	}

	return (0);
}

void
pcmcia_card_detach(device_t dev, int flags)
	/* flags:		 DETACH_* flags */
{
	struct pcmcia_softc *sc = device_private(dev);
	struct pcmcia_function *pf;
	int error;

	/*
	 * We are running on either the PCMCIA socket's event thread
	 * or in user context detaching a device by user request.
	 */
	SIMPLEQ_FOREACH(pf, &sc->card.pf_head, pf_list) {
		pf->pf_flags |= PFF_DETACHED;
		if (SIMPLEQ_EMPTY(&pf->cfe_head))
			continue;
		if (pf->child == NULL)
			continue;
		DPRINTF(("%s: detaching %s (function %d)\n",
		    device_xname(sc->dev), device_xname(pf->child), pf->number));
		if ((error = config_detach(pf->child, flags)) != 0) {
			printf("%s: error %d detaching %s (function %d)\n",
			    device_xname(sc->dev), error, device_xname(pf->child),
			    pf->number);
		}
	}

	if (sc->sc_enabled_count != 0) {
#ifdef DIAGNOSTIC
		printf("pcmcia_card_detach: enabled_count should be 0 here??\n");
#endif
		pcmcia_chip_socket_disable(sc->pct, sc->pch);
		sc->sc_enabled_count = 0;
	}
}

void
pcmcia_childdetached(device_t self, device_t child)
{
	struct pcmcia_softc *sc = device_private(self);
	struct pcmcia_function *pf;

	SIMPLEQ_FOREACH(pf, &sc->card.pf_head, pf_list) {
		if (SIMPLEQ_EMPTY(&pf->cfe_head))
			continue;
		if (pf->child == child) {
			KASSERT(device_locator(child, PCMCIACF_FUNCTION)
				== pf->number);
			pf->child = NULL;
			return;
		}
	}

	aprint_error_dev(self, "pcmcia_childdetached: %s not found\n",
	       device_xname(child));
}

void
pcmcia_card_deactivate(device_t dev)
{
	struct pcmcia_softc *sc = device_private(dev);
	struct pcmcia_function *pf;

	/*
	 * We're in the chip's card removal interrupt handler.
	 * Deactivate the child driver.  The PCMCIA socket's
	 * event thread will run later to finish the detach.
	 */
	SIMPLEQ_FOREACH(pf, &sc->card.pf_head, pf_list) {
		if (SIMPLEQ_EMPTY(&pf->cfe_head))
			continue;
		if (pf->child == NULL)
			continue;
		DPRINTF(("%s: deactivating %s (function %d)\n",
		    device_xname(sc->dev), device_xname(pf->child), pf->number));
		config_deactivate(pf->child);
	}
}

int
pcmcia_print(void *arg, const char *pnp)
{
	struct pcmcia_attach_args *pa = arg;
	struct pcmcia_softc *sc = pa->pf->sc;
	struct pcmcia_card *card = &sc->card;
	char devinfo[256];

	if (pnp)
		aprint_normal("%s", pnp);

	pcmcia_devinfo(card, !!pnp, devinfo, sizeof(devinfo));

	aprint_normal(" function %d: %s\n", pa->pf->number, devinfo);

	return (UNCONF);
}

void
pcmcia_devinfo(struct pcmcia_card *card, int showhex, char *cp, size_t cplen)
{
	int i, n;

	if (cplen > 1) {
		*cp++ = '<';
		*cp = '\0';
		cplen--;
	}

	for (i = 0; i < 4 && card->cis1_info[i] != NULL && cplen > 1; i++) {
		n = snprintf(cp, cplen, "%s%s", i ? ", " : "",
		        card->cis1_info[i]);
		cp += n;
		if (cplen < n)
			return;
		cplen -= n;
	}

	if (cplen > 1) {
		*cp++ = '>';
		*cp = '\0';
		cplen--;
	}

	if (showhex && cplen > 1)
		snprintf(cp, cplen, " (manufacturer 0x%04x, product 0x%04x)",
		    card->manufacturer, card->product);
}

const void *
pcmcia_product_lookup(struct pcmcia_attach_args *pa, const void *tab, size_t nent, size_t ent_size, pcmcia_product_match_fn matchfn)
{
        const struct pcmcia_product *pp;
	int n;
	int matches;

#ifdef DIAGNOSTIC
	if (sizeof *pp > ent_size)
		panic("pcmcia_product_lookup: bogus ent_size %ld",
		      (long) ent_size);
#endif

        for (pp = tab, n = nent; n; pp = (const struct pcmcia_product *)
	      ((const char *)pp + ent_size), n--) {
		/* see if it matches vendor/product */
		matches = 0;
		if ((pp->pp_vendor != PCMCIA_VENDOR_INVALID &&
		     pp->pp_vendor == pa->manufacturer) &&
		    (pp->pp_product != PCMCIA_PRODUCT_INVALID &&
		     pp->pp_product == pa->product))
			matches = 1;
		if ((pp->pp_cisinfo[0] && pa->card->cis1_info[0] &&
		          !strcmp(pp->pp_cisinfo[0], pa->card->cis1_info[0])) &&
		         (pp->pp_cisinfo[1] && pa->card->cis1_info[1] &&
		          !strcmp(pp->pp_cisinfo[1], pa->card->cis1_info[1])) &&
		         (!pp->pp_cisinfo[2] || (pa->card->cis1_info[2] &&
		           !strcmp(pp->pp_cisinfo[2], pa->card->cis1_info[2]))) &&
		         (!pp->pp_cisinfo[3] || (pa->card->cis1_info[3] &&
		           !strcmp(pp->pp_cisinfo[3], pa->card->cis1_info[3]))))
			matches = 1;

		/* if a separate match function is given, let it override */
		if (matchfn)
			matches = (*matchfn)(pa, pp, matches);

		if (matches)
                        return (pp);
        }
        return (0);
}

void
pcmcia_socket_settype(device_t dev, int type)
{
	struct pcmcia_softc *sc = device_private(dev);

	pcmcia_chip_socket_settype(sc->pct, sc->pch, type);
}

/*
 * Initialize a PCMCIA function.  May be called as long as the function is
 * disabled.
 */
void
pcmcia_function_init(struct pcmcia_function *pf, struct pcmcia_config_entry *cfe)
{
	if (pf->pf_flags & PFF_ENABLED)
		panic("pcmcia_function_init: function is enabled");

	/* Remember which configuration entry we are using. */
	pf->cfe = cfe;
}

void
pcmcia_socket_enable(device_t dev)
{
	struct pcmcia_softc *sc = device_private(dev);

	if (sc->sc_enabled_count++ == 0)
		pcmcia_chip_socket_enable(sc->pct, sc->pch);
	DPRINTF(("%s: ++enabled_count = %d\n", device_xname(sc->dev),
		 sc->sc_enabled_count));
}

void
pcmcia_socket_disable(device_t dev)
{
	struct pcmcia_softc *sc = device_private(dev);

	if (--sc->sc_enabled_count == 0)
		pcmcia_chip_socket_disable(sc->pct, sc->pch);
	DPRINTF(("%s: --enabled_count = %d\n", device_xname(sc->dev),
		 sc->sc_enabled_count));
}

/* Enable a PCMCIA function */
int
pcmcia_function_enable(struct pcmcia_function *pf)
{
	struct pcmcia_softc *sc = pf->sc;
	struct pcmcia_function *tmp;
	int reg;
	int error;

	if (pf->cfe == NULL)
		panic("pcmcia_function_enable: function not initialized");

	/*
	 * Increase the reference count on the socket, enabling power, if
	 * necessary.
	 */
	pcmcia_socket_enable(sc->dev);
	pcmcia_socket_settype(sc->dev, pf->cfe->iftype);

	if (pf->pf_flags & PFF_ENABLED) {
		/*
		 * Don't do anything if we're already enabled.
		 */
		return (0);
	}

	/*
	 * it's possible for different functions' CCRs to be in the same
	 * underlying page.  Check for that.
	 */

	SIMPLEQ_FOREACH(tmp, &sc->card.pf_head, pf_list) {
		if ((tmp->pf_flags & PFF_ENABLED) &&
		    (pf->ccr_base >= (tmp->ccr_base - tmp->pf_ccr_offset)) &&
		    ((pf->ccr_base + PCMCIA_CCR_SIZE) <=
		     (tmp->ccr_base - tmp->pf_ccr_offset +
		      tmp->pf_ccr_realsize))) {
			pf->pf_ccrt = tmp->pf_ccrt;
			pf->pf_ccrh = tmp->pf_ccrh;
			pf->pf_ccr_realsize = tmp->pf_ccr_realsize;

			/*
			 * pf->pf_ccr_offset = (tmp->pf_ccr_offset -
			 * tmp->ccr_base) + pf->ccr_base;
			 */
			pf->pf_ccr_offset =
			    (tmp->pf_ccr_offset + pf->ccr_base) -
			    tmp->ccr_base;
			pf->pf_ccr_window = tmp->pf_ccr_window;
			break;
		}
	}

	if (tmp == NULL) {
		error = pcmcia_mem_alloc(pf, PCMCIA_CCR_SIZE, &pf->pf_pcmh);
		if (error)
			goto bad;

		error = pcmcia_mem_map(pf, PCMCIA_MEM_ATTR, pf->ccr_base,
		    PCMCIA_CCR_SIZE, &pf->pf_pcmh, &pf->pf_ccr_offset,
		    &pf->pf_ccr_window);
		if (error) {
			pcmcia_mem_free(pf, &pf->pf_pcmh);
			goto bad;
		}
	}

	if (pcmcia_mfc(sc) || 1) {
		pcmcia_ccr_write(pf, PCMCIA_CCR_IOBASE0,
				 (pf->pf_mfc_iobase >>  0) & 0xff);
		pcmcia_ccr_write(pf, PCMCIA_CCR_IOBASE1,
				 (pf->pf_mfc_iobase >>  8) & 0xff);
		pcmcia_ccr_write(pf, PCMCIA_CCR_IOBASE2,
				 (pf->pf_mfc_iobase >> 16) & 0xff);
		pcmcia_ccr_write(pf, PCMCIA_CCR_IOBASE3,
				 (pf->pf_mfc_iobase >> 24) & 0xff);
		pcmcia_ccr_write(pf, PCMCIA_CCR_IOLIMIT,
				 pf->pf_mfc_iomax - pf->pf_mfc_iobase);
	}

	reg = 0;
	if (pf->cfe->flags & PCMCIA_CFE_AUDIO)
		reg |= PCMCIA_CCR_STATUS_AUDIO;
	pcmcia_ccr_write(pf, PCMCIA_CCR_STATUS, reg);

	pcmcia_ccr_write(pf, PCMCIA_CCR_SOCKETCOPY, 0);

	reg = (pf->cfe->number & PCMCIA_CCR_OPTION_CFINDEX);
	reg |= PCMCIA_CCR_OPTION_LEVIREQ;
	if (pcmcia_mfc(sc)) {
		reg |= (PCMCIA_CCR_OPTION_FUNC_ENABLE |
			PCMCIA_CCR_OPTION_ADDR_DECODE);
		if (pf->pf_ih)
			reg |= PCMCIA_CCR_OPTION_IREQ_ENABLE;

	}
	pcmcia_ccr_write(pf, PCMCIA_CCR_OPTION, reg);

#ifdef PCMCIADEBUG
	if (pcmcia_debug) {
		SIMPLEQ_FOREACH(tmp, &sc->card.pf_head, pf_list) {
			printf("%s: function %d CCR at %d offset %lx: "
			       "%x %x %x %x, %x %x %x %x, %x\n",
			       device_xname(tmp->sc->dev), tmp->number,
			       tmp->pf_ccr_window,
			       (unsigned long) tmp->pf_ccr_offset,
			       pcmcia_ccr_read(tmp, 0),
			       pcmcia_ccr_read(tmp, 1),
			       pcmcia_ccr_read(tmp, 2),
			       pcmcia_ccr_read(tmp, 3),

			       pcmcia_ccr_read(tmp, 5),
			       pcmcia_ccr_read(tmp, 6),
			       pcmcia_ccr_read(tmp, 7),
			       pcmcia_ccr_read(tmp, 8),

			       pcmcia_ccr_read(tmp, 9));
		}
	}
#endif

#ifdef IT8368E_LEGACY_MODE
	/* return to I/O mode */
	it8368_mode(pf, IT8368_IO_MODE, IT8368_WIDTH_16);
#endif

	pf->pf_flags |= PFF_ENABLED;
	return (0);

bad:
	/*
	 * Decrement the reference count, and power down the socket, if
	 * necessary.
	 */
	printf("%s: couldn't map the CCR\n", device_xname(pf->child));
	pcmcia_socket_disable(sc->dev);

	return (error);
}

/* Disable PCMCIA function. */
void
pcmcia_function_disable(struct pcmcia_function *pf)
{
	struct pcmcia_softc *sc = pf->sc;
	struct pcmcia_function *tmp;
	int reg;

	if (pf->cfe == NULL)
		panic("pcmcia_function_enable: function not initialized");

	if ((pf->pf_flags & PFF_ENABLED) == 0) {
		/*
		 * Don't do anything but decrement if we're already disabled.
		 */
		goto out;
	}

	if (pcmcia_mfc(sc) &&
	    (pf->pf_flags & PFF_DETACHED) == 0) {
		reg = pcmcia_ccr_read(pf, PCMCIA_CCR_OPTION);
		reg &= ~(PCMCIA_CCR_OPTION_FUNC_ENABLE|
			 PCMCIA_CCR_OPTION_ADDR_DECODE|
		         PCMCIA_CCR_OPTION_IREQ_ENABLE);
		pcmcia_ccr_write(pf, PCMCIA_CCR_OPTION, reg);
	}

	/*
	 * it's possible for different functions' CCRs to be in the same
	 * underlying page.  Check for that.  Note we mark us as disabled
	 * first to avoid matching ourself.
	 */

	pf->pf_flags &= ~PFF_ENABLED;
	SIMPLEQ_FOREACH(tmp, &sc->card.pf_head, pf_list) {
		if ((tmp->pf_flags & PFF_ENABLED) &&
		    (pf->ccr_base >= (tmp->ccr_base - tmp->pf_ccr_offset)) &&
		    ((pf->ccr_base + PCMCIA_CCR_SIZE) <=
		(tmp->ccr_base - tmp->pf_ccr_offset + tmp->pf_ccr_realsize)))
			break;
	}

	/* Not used by anyone else; unmap the CCR. */
	if (tmp == NULL) {
		pcmcia_mem_unmap(pf, pf->pf_ccr_window);
		pcmcia_mem_free(pf, &pf->pf_pcmh);
	}

out:
	/*
	 * Decrement the reference count, and power down the socket, if
	 * necessary.
	 */
	pcmcia_socket_disable(sc->dev);
}

int
pcmcia_io_map(struct pcmcia_function *pf, int width, struct pcmcia_io_handle *pcihp, int *windowp)
{
	struct pcmcia_softc *sc = pf->sc;
	int error;

	if (pf->pf_flags & PFF_ENABLED)
		printf("pcmcia_io_map: function is enabled!\n");

	error = pcmcia_chip_io_map(sc->pct, sc->pch,
	    width, 0, pcihp->size, pcihp, windowp);
	if (error)
		return (error);

	/*
	 * XXX in the multifunction multi-iospace-per-function case, this
	 * needs to cooperate with io_alloc to make sure that the spaces
	 * don't overlap, and that the ccr's are set correctly
	 */

	if (pcmcia_mfc(sc) || 1) {
		bus_addr_t iobase = pcihp->addr;
		bus_addr_t iomax = pcihp->addr + pcihp->size - 1;

		DPRINTF(("window iobase %lx iomax %lx\n", (long)iobase,
		    (long)iomax));
		if (pf->pf_mfc_iobase == 0) {
			pf->pf_mfc_iobase = iobase;
			pf->pf_mfc_iomax = iomax;
		} else {
			if (iobase < pf->pf_mfc_iobase)
				pf->pf_mfc_iobase = iobase;
			if (iomax > pf->pf_mfc_iomax)
				pf->pf_mfc_iomax = iomax;
		}
		DPRINTF(("function iobase %lx iomax %lx\n",
		    (long)pf->pf_mfc_iobase, (long)pf->pf_mfc_iomax));
	}

	return (0);
}

void
pcmcia_io_unmap(struct pcmcia_function *pf, int window)
{
	struct pcmcia_softc *sc = pf->sc;

	if (pf->pf_flags & PFF_ENABLED)
		printf("pcmcia_io_unmap: function is enabled!\n");

	pcmcia_chip_io_unmap(sc->pct, sc->pch, window);
}

void *
pcmcia_intr_establish(struct pcmcia_function *pf, int ipl,
	int (*ih_fct)(void *), void *ih_arg)
{

	if (pf->pf_flags & PFF_ENABLED)
		printf("pcmcia_intr_establish: function is enabled!\n");
	if (pf->pf_ih)
		panic("pcmcia_intr_establish: already done\n");

	pf->pf_ih = pcmcia_chip_intr_establish(pf->sc->pct, pf->sc->pch,
	    pf, ipl, ih_fct, ih_arg);
	if (!pf->pf_ih)
		aprint_error_dev(pf->child, "interrupt establish failed\n");
	return (pf->pf_ih);
}

void
pcmcia_intr_disestablish(struct pcmcia_function *pf, void *ih)
{

	if (pf->pf_flags & PFF_ENABLED)
		printf("pcmcia_intr_disestablish: function is enabled!\n");
	if (!pf->pf_ih)
		panic("pcmcia_intr_distestablish: already done\n");

	pcmcia_chip_intr_disestablish(pf->sc->pct, pf->sc->pch, ih);
	pf->pf_ih = 0;
}

int
pcmcia_config_alloc(struct pcmcia_function *pf, struct pcmcia_config_entry *cfe)
{
	int error = 0;
	int n, m;

	for (n = 0; n < cfe->num_iospace; n++) {
		bus_addr_t start = cfe->iospace[n].start;
		bus_size_t length = cfe->iospace[n].length;
		bus_size_t align = cfe->iomask ? (1 << cfe->iomask) :
		    length;
		bus_size_t skew = start & (align - 1);

		if ((start - skew) == 0 && align < 0x400) {
			if (skew)
				printf("Drats!  I need a skew!\n");
			start = 0;
		}

		DPRINTF(("pcmcia_config_alloc: io %d start=%lx length=%lx align=%lx skew=%lx\n",
		    n, (long)start, (long)length, (long)align, (long)skew));

		error = pcmcia_io_alloc(pf, start, length, align,
		    &cfe->iospace[n].handle);
		if (error)
			break;
	}
	if (n < cfe->num_iospace) {
		for (m = 0; m < n; m++)
			pcmcia_io_free(pf, &cfe->iospace[m].handle);
		return (error);
	}

	for (n = 0; n < cfe->num_memspace; n++) {
		bus_size_t length = cfe->memspace[n].length;

		DPRINTF(("pcmcia_config_alloc: mem %d length %lx\n", n,
		    (long)length));

		error = pcmcia_mem_alloc(pf, length, &cfe->memspace[n].handle);
		if (error)
			break;
	}
	if (n < cfe->num_memspace) {
		for (m = 0; m < cfe->num_iospace; m++)
			pcmcia_io_free(pf, &cfe->iospace[m].handle);
		for (m = 0; m < n; m++)
			pcmcia_mem_free(pf, &cfe->memspace[m].handle);
		return (error);
	}

	/* This one's good! */
	return (error);
}

void
pcmcia_config_free(struct pcmcia_function *pf)
{
	struct pcmcia_config_entry *cfe = pf->cfe;
	int m;

	for (m = 0; m < cfe->num_iospace; m++)
		pcmcia_io_free(pf, &cfe->iospace[m].handle);
	for (m = 0; m < cfe->num_memspace; m++)
		pcmcia_mem_free(pf, &cfe->memspace[m].handle);
}

int
pcmcia_config_map(struct pcmcia_function *pf)
{
	struct pcmcia_config_entry *cfe = pf->cfe;
	int error = 0;
	int n, m;

	for (n = 0; n < cfe->num_iospace; n++) {
		int width;

		if (cfe->flags & PCMCIA_CFE_IO16)
			width = PCMCIA_WIDTH_AUTO;
		else
			width = PCMCIA_WIDTH_IO8;
		error = pcmcia_io_map(pf, width, &cfe->iospace[n].handle,
		    &cfe->iospace[n].window);
		if (error)
			break;
	}
	if (n < cfe->num_iospace) {
		for (m = 0; m < n; m++)
			pcmcia_io_unmap(pf, cfe->iospace[m].window);
		return (error);
	}

	for (n = 0; n < cfe->num_memspace; n++) {
		bus_size_t length = cfe->memspace[n].length;
		int width;

		DPRINTF(("pcmcia_config_alloc: mem %d length %lx\n", n,
		    (long)length));

		/*XXX*/
		width = PCMCIA_WIDTH_MEM8|PCMCIA_MEM_COMMON;
		error = pcmcia_mem_map(pf, width, 0, length,
		    &cfe->memspace[n].handle, &cfe->memspace[n].offset,
		    &cfe->memspace[n].window);
		if (error)
			break;
	}
	if (n < cfe->num_memspace) {
		for (m = 0; m < cfe->num_iospace; m++)
			pcmcia_io_unmap(pf, cfe->iospace[m].window);
		for (m = 0; m < n; m++)
			pcmcia_mem_unmap(pf, cfe->memspace[m].window);
		return (error);
	}

	/* This one's good! */
	return (error);
}

void
pcmcia_config_unmap(struct pcmcia_function *pf)
{
	struct pcmcia_config_entry *cfe = pf->cfe;
	int m;

	for (m = 0; m < cfe->num_iospace; m++)
		pcmcia_io_unmap(pf, cfe->iospace[m].window);
	for (m = 0; m < cfe->num_memspace; m++)
		pcmcia_mem_unmap(pf, cfe->memspace[m].window);
}

int
pcmcia_function_configure(struct pcmcia_function *pf,
	int (*validator)(struct pcmcia_config_entry *))
{
	struct pcmcia_config_entry *cfe;
	int error = ENOENT;

	SIMPLEQ_FOREACH(cfe, &pf->cfe_head, cfe_list) {
		error = validator(cfe);
		if (error)
			continue;
		error = pcmcia_config_alloc(pf, cfe);
		if (!error)
			break;
	}
	if (!cfe) {
		DPRINTF(("pcmcia_function_configure: no config entry found, error=%d\n",
		    error));
		return (error);
	}

	/* Remember which configuration entry we are using. */
	pf->cfe = cfe;

	error = pcmcia_config_map(pf);
	if (error) {
		DPRINTF(("pcmcia_function_configure: map failed, error=%d\n",
		    error));
		return (error);
	}

	return (0);
}

void
pcmcia_function_unconfigure(struct pcmcia_function *pf)
{

	pcmcia_config_unmap(pf);
	pcmcia_config_free(pf);
}
