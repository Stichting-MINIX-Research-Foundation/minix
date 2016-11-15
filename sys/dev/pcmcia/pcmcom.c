/*	$NetBSD: pcmcom.c,v 1.40 2012/10/27 17:18:37 chs Exp $	*/

/*-
 * Copyright (c) 1998, 2000, 2004 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by RedBack Networks, Inc.
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
 * Device driver for multi-port PCMCIA serial cards, written by
 * Jason R. Thorpe for RedBack Networks, Inc.
 *
 * Most of these cards are simply multiple UARTs sharing a single interrupt
 * line, and rely on the fact that PCMCIA level-triggered interrupts can
 * be shared.  There are no special interrupt registers on them, as there
 * are on most ISA multi-port serial cards.
 *
 * If there are other cards that have interrupt registers, they should not
 * be glued into this driver.  Rather, separate drivers should be written
 * for those devices, as we have in the ISA multi-port serial card case.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: pcmcom.c,v 1.40 2012/10/27 17:18:37 chs Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/termios.h>
#include <sys/malloc.h>

#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/ic/comreg.h>
#include <dev/ic/comvar.h>

#include <dev/pcmcia/pcmciavar.h>
#include <dev/pcmcia/pcmciareg.h>
#include <dev/pcmcia/pcmciadevs.h>

#include "com.h"
#include "pcmcom.h"

#include "locators.h"

struct pcmcom_softc {
	device_t sc_dev;			/* generic device glue */

	struct pcmcia_function *sc_pf;		/* our PCMCIA function */
	void *sc_ih;				/* interrupt handle */
	int sc_enabled_count;			/* enabled count */

#define	NSLAVES			8
	device_t sc_slaves[NSLAVES];	/* slave info */
	int sc_nslaves;				/* slave count */

	int sc_state;
#define	PCMCOM_ATTACHED		3
};

struct pcmcom_attach_args {
	bus_space_tag_t pca_iot;		/* I/O tag */
	bus_space_handle_t pca_ioh;		/* I/O handle */
	int pca_slave;				/* slave # */
};

int	pcmcom_match(device_t, cfdata_t, void *);
int	pcmcom_validate_config(struct pcmcia_config_entry *);
void	pcmcom_attach(device_t, device_t, void *);
int	pcmcom_detach(device_t, int);
void	pcmcom_childdet(device_t, device_t);

CFATTACH_DECL_NEW(pcmcom, sizeof(struct pcmcom_softc),
    pcmcom_match, pcmcom_attach, pcmcom_detach, NULL);

const struct pcmcia_product pcmcom_products[] = {
	{ PCMCIA_VENDOR_SOCKET, PCMCIA_PRODUCT_SOCKET_DUAL_RS232,
	  PCMCIA_CIS_INVALID },
#if 0	/* does not work */
	{ PCMCIA_VENDOR_SOCKET, PCMCIA_PRODUCT_SOCKET_DUAL_RS232_A,
	  PCMCIA_CIS_INVALID },
#endif
};
const size_t pcmcom_nproducts = __arraycount(pcmcom_products);

int	pcmcom_print(void *, const char *);

int	pcmcom_enable(struct pcmcom_softc *);
void	pcmcom_disable(struct pcmcom_softc *);

int	pcmcom_intr(void *);

int
pcmcom_match(device_t parent, cfdata_t cf, void *aux)
{
	struct pcmcia_attach_args *pa = aux;

	if (pcmcia_product_lookup(pa, pcmcom_products, pcmcom_nproducts,
	    sizeof(pcmcom_products[0]), NULL))
		return (2);	/* beat com_pcmcia */
	return (0);
}

int
pcmcom_validate_config(struct pcmcia_config_entry *cfe)
{
	if (cfe->iftype != PCMCIA_IFTYPE_IO ||
	    cfe->num_iospace < 1 || cfe->num_iospace > NSLAVES)
		return (EINVAL);
	return (0);
}

void
pcmcom_attach(device_t parent, device_t self, void *aux)
{
	struct pcmcom_softc *sc = device_private(self);
	struct pcmcia_attach_args *pa = aux;
	struct pcmcia_config_entry *cfe;
	int slave;
	int error;
	int locs[PCMCOMCF_NLOCS];

	sc->sc_dev = self;

	sc->sc_pf = pa->pf;

	error = pcmcia_function_configure(pa->pf, pcmcom_validate_config);
	if (error) {
		aprint_error_dev(self, "configure failed, error=%d\n", error);
		return;
	}

	cfe = pa->pf->cfe;
	sc->sc_nslaves = cfe->num_iospace;

	error = pcmcom_enable(sc);
	if (error)
		goto fail;

	/* Attach the children. */
	for (slave = 0; slave < sc->sc_nslaves; slave++) {
		struct pcmcom_attach_args pca;

		printf("%s: slave %d\n", device_xname(self), slave);

		pca.pca_iot = cfe->iospace[slave].handle.iot;
		pca.pca_ioh = cfe->iospace[slave].handle.ioh;
		pca.pca_slave = slave;

		locs[PCMCOMCF_SLAVE] = slave;

		sc->sc_slaves[slave] = config_found_sm_loc(sc->sc_dev,
			"pcmcom", locs,
			&pca, pcmcom_print, config_stdsubmatch);
	}

	pcmcom_disable(sc);
	sc->sc_state = PCMCOM_ATTACHED;
	return;

fail:
	pcmcia_function_unconfigure(pa->pf);
}

void
pcmcom_childdet(device_t self, device_t child)
{
	struct pcmcom_softc *sc = device_private(self);
	int slave;

	for (slave = sc->sc_nslaves - 1; slave >= 0; slave--) {
		if (sc->sc_slaves[slave] == child)
			sc->sc_slaves[slave] = NULL;
	}
}

int
pcmcom_detach(device_t self, int flags)
{
	struct pcmcom_softc *sc = device_private(self);
	int slave, error;

	if (sc->sc_state != PCMCOM_ATTACHED)
		return (0);

	for (slave = sc->sc_nslaves - 1; slave >= 0; slave--) {
		if (sc->sc_slaves[slave]) {
			/* Detach the child. */
			error = config_detach(sc->sc_slaves[slave], flags);
			if (error)
				return (error);
		}
	}

	pcmcia_function_unconfigure(sc->sc_pf);

	return (0);
}

int
pcmcom_print(void *aux, const char *pnp)
{
	struct pcmcom_attach_args *pca = aux;

	/* only com's can attach to pcmcom's; easy... */
	if (pnp)
		aprint_normal("com at %s", pnp);

	aprint_normal(" slave %d", pca->pca_slave);

	return (UNCONF);
}

int
pcmcom_intr(void *arg)
{
#if NCOM > 0
	struct pcmcom_softc *sc = arg;
	int i, rval = 0;

	if (sc->sc_enabled_count == 0)
		return (0);

	for (i = 0; i < sc->sc_nslaves; i++) {
		if (sc->sc_slaves[i])
			rval |= comintr(sc->sc_slaves[i]);
	}

	return (rval);
#else
	return (0);
#endif /* NCOM > 0 */
}

int
pcmcom_enable(struct pcmcom_softc *sc)
{
	int error;

	if (sc->sc_enabled_count++ != 0)
		return (0);

	/* Establish the interrupt. */
	sc->sc_ih = pcmcia_intr_establish(sc->sc_pf, IPL_SERIAL,
	    pcmcom_intr, sc);
	if (!sc->sc_ih)
		return (EIO);

	error = pcmcia_function_enable(sc->sc_pf);
	if (error) {
		pcmcia_intr_disestablish(sc->sc_pf, sc->sc_ih);
		sc->sc_ih = 0;
	}

	return (error);
}

void
pcmcom_disable(struct pcmcom_softc *sc)
{

	if (--sc->sc_enabled_count != 0)
		return;

	pcmcia_function_disable(sc->sc_pf);
	pcmcia_intr_disestablish(sc->sc_pf, sc->sc_ih);
	sc->sc_ih = 0;
}

/****** Here begins the com attachment code. ******/

#if NCOM_PCMCOM > 0
int	com_pcmcom_match(device_t, cfdata_t , void *);
void	com_pcmcom_attach(device_t, device_t, void *);

/* No pcmcom-specific goo in the softc; it's all in the parent. */
CFATTACH_DECL_NEW(com_pcmcom, sizeof(struct com_softc),
    com_pcmcom_match, com_pcmcom_attach, com_detach, NULL);

int	com_pcmcom_enable(struct com_softc *);
void	com_pcmcom_disable(struct com_softc *);

int
com_pcmcom_match(device_t parent, cfdata_t cf, void *aux)
{

	/* Device is always present. */
	return (1);
}

void
com_pcmcom_attach(device_t parent, device_t self, void *aux)
{
	struct com_softc *sc = device_private(self);
	struct pcmcom_attach_args *pca = aux;

	sc->sc_dev = self;
	COM_INIT_REGS(sc->sc_regs, pca->pca_iot, pca->pca_ioh, -1);
	sc->enabled = 1;

	sc->sc_frequency = COM_FREQ;

	sc->enable = com_pcmcom_enable;
	sc->disable = com_pcmcom_disable;

	com_attach_subr(sc);

	sc->enabled = 0;
}

int
com_pcmcom_enable(struct com_softc *sc)
{
	return pcmcom_enable(device_private(device_parent(sc->sc_dev)));
}

void
com_pcmcom_disable(struct com_softc *sc)
{

	pcmcom_disable(device_private(device_parent(sc->sc_dev)));
}
#endif /* NCOM_PCMCOM > 0 */
