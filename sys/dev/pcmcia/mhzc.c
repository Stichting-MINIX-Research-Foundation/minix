/*	$NetBSD: mhzc.c,v 1.50 2012/10/27 17:18:37 chs Exp $	*/

/*-
 * Copyright (c) 1999, 2000, 2004 The NetBSD Foundation, Inc.
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
 * Device driver for the Megaherz X-JACK Ethernet/Modem combo cards.
 *
 * Many thanks to Chuck Cranor for having the patience to sift through
 * the Linux smc91c92_cs.c driver to find the magic details to get this
 * working!
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: mhzc.c,v 1.50 2012/10/27 17:18:37 chs Exp $");

#include "opt_inet.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/syslog.h>
#include <sys/select.h>
#include <sys/tty.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/proc.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_ether.h>
#include <net/if_media.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_inarp.h>
#endif


#include <net/bpf.h>
#include <net/bpfdesc.h>

#include <sys/intr.h>
#include <sys/bus.h>

#include <dev/ic/comreg.h>
#include <dev/ic/comvar.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <dev/ic/smc91cxxreg.h>
#include <dev/ic/smc91cxxvar.h>

#include <dev/pcmcia/pcmciareg.h>
#include <dev/pcmcia/pcmciavar.h>
#include <dev/pcmcia/pcmciadevs.h>

#include "mhzc.h"

struct mhzc_softc {
	device_t sc_dev;		/* generic device glue */

	struct pcmcia_function *sc_pf;	/* our PCMCIA function */
	void *sc_ih;			/* interrupt handle */

	const struct mhzc_product *sc_product;

	/*
	 * Data for the Modem portion.
	 */
	device_t sc_modem;
	struct pcmcia_io_handle sc_modem_pcioh;
	int sc_modem_io_window;

	/*
	 * Data for the Ethernet portion.
	 */
	device_t sc_ethernet;
	struct pcmcia_io_handle sc_ethernet_pcioh;
	int sc_ethernet_io_window;

	int sc_flags;
};

/* sc_flags */
#define	MHZC_MODEM_MAPPED	0x01
#define	MHZC_ETHERNET_MAPPED	0x02
#define	MHZC_MODEM_ENABLED	0x04
#define	MHZC_ETHERNET_ENABLED	0x08
#define	MHZC_MODEM_ALLOCED	0x10
#define	MHZC_ETHERNET_ALLOCED	0x20

int	mhzc_match(device_t, cfdata_t, void *);
void	mhzc_attach(device_t, device_t, void *);
void	mhzc_childdet(device_t, device_t);
int	mhzc_detach(device_t, int);

CFATTACH_DECL2_NEW(mhzc, sizeof(struct mhzc_softc),
    mhzc_match, mhzc_attach, mhzc_detach, NULL, NULL, mhzc_childdet);

int	mhzc_em3336_enaddr(struct mhzc_softc *, u_int8_t *);
int	mhzc_em3336_enable(struct mhzc_softc *);

const struct mhzc_product {
	struct pcmcia_product mp_product;

	/* Get the Ethernet address for this card. */
	int		(*mp_enaddr)(struct mhzc_softc *, u_int8_t *);

	/* Perform any special `enable' magic. */
	int		(*mp_enable)(struct mhzc_softc *);
} mhzc_products[] = {
	{ { PCMCIA_VENDOR_MEGAHERTZ, PCMCIA_PRODUCT_MEGAHERTZ_EM3336,
	    PCMCIA_CIS_INVALID },
	  mhzc_em3336_enaddr,		mhzc_em3336_enable },
};
static const size_t mhzc_nproducts =
    sizeof(mhzc_products) / sizeof(mhzc_products[0]);

int	mhzc_print(void *, const char *);

int	mhzc_check_cfe(struct mhzc_softc *, struct pcmcia_config_entry *);
int	mhzc_alloc_ethernet(struct mhzc_softc *, struct pcmcia_config_entry *);

int	mhzc_enable(struct mhzc_softc *, int);
void	mhzc_disable(struct mhzc_softc *, int);

int	mhzc_intr(void *);

int
mhzc_match(device_t parent, cfdata_t match, void *aux)
{
	struct pcmcia_attach_args *pa = aux;

	if (pcmcia_product_lookup(pa, mhzc_products, mhzc_nproducts,
	    sizeof(mhzc_products[0]), NULL))
		return (2);		/* beat `com' */
	return (0);
}

void
mhzc_attach(device_t parent, device_t self, void *aux)
{
	struct mhzc_softc *sc = device_private(self);
	struct pcmcia_attach_args *pa = aux;
	struct pcmcia_config_entry *cfe;
	int error;

	sc->sc_dev = self;
	sc->sc_pf = pa->pf;

	sc->sc_product = pcmcia_product_lookup(pa, mhzc_products,
	    mhzc_nproducts, sizeof(mhzc_products[0]), NULL);
	if (!sc->sc_product)
		panic("mhzc_attach: impossible");

	/*
	 * The address decoders on these cards are wacky.  The configuration
	 * entries are set up to look like serial ports, and have no
	 * information about the Ethernet portion.  In order to talk to
	 * the Modem portion, the I/O address must have bit 0x80 set.
	 * In order to talk to the Ethernet portion, the I/O address must
	 * have the 0x80 bit clear.
	 *
	 * The standard configuration entries conveniently have 0x80 set
	 * in them, and have a length of 8 (a 16550's size, convenient!),
	 * so we use those to set up the Modem portion.
	 *
	 * Once we have the Modem's address established, we search for
	 * an address suitable for the Ethernet portion.  We do this by
	 * rounding up to the next 16-byte aligned address where 0x80
	 * isn't set (the SMC Ethernet chip has a 16-byte address size)
	 * and attemping to allocate a 16-byte region until we succeed.
	 *
	 * Sure would have been nice if Megahertz had made the card a
	 * proper multi-function device.
	 */
	SIMPLEQ_FOREACH(cfe, &pa->pf->cfe_head, cfe_list) {
		if (mhzc_check_cfe(sc, cfe)) {
			/* Found one! */
			break;
		}
	}
	if (cfe == NULL) {
		aprint_error_dev(self, "unable to find suitable config table entry\n");
		goto fail;
	}

	if (mhzc_alloc_ethernet(sc, cfe) == 0) {
		aprint_error_dev(self, "unable to allocate space for Ethernet portion\n");
		goto fail;
	}

	/* Enable the card. */
	pcmcia_function_init(pa->pf, cfe);

	if (pcmcia_io_map(sc->sc_pf, PCMCIA_WIDTH_IO8, &sc->sc_modem_pcioh,
	    &sc->sc_modem_io_window)) {
		aprint_error_dev(sc->sc_dev, "unable to map I/O space\n");
		goto fail;
	}
	sc->sc_flags |= MHZC_MODEM_MAPPED;

	if (pcmcia_io_map(sc->sc_pf, PCMCIA_WIDTH_AUTO, &sc->sc_ethernet_pcioh,
	    &sc->sc_ethernet_io_window)) {
		aprint_error_dev(sc->sc_dev, "unable to map I/O space\n");
		goto fail;
	}
	sc->sc_flags |= MHZC_ETHERNET_MAPPED;

	error = mhzc_enable(sc, MHZC_MODEM_ENABLED|MHZC_ETHERNET_ENABLED);
	if (error)
		goto fail;

	/*XXXUNCONST*/
	sc->sc_modem = config_found(self, __UNCONST("com"), mhzc_print);
	/*XXXUNCONST*/
	sc->sc_ethernet = config_found(self, __UNCONST("sm"), mhzc_print);

	mhzc_disable(sc, MHZC_MODEM_ENABLED|MHZC_ETHERNET_ENABLED);
	return;

fail:
	/* I/O spaces will be freed by detach. */
	;
}

int
mhzc_check_cfe(struct mhzc_softc *sc, struct pcmcia_config_entry *cfe)
{

	if (cfe->num_memspace != 0)
		return (0);

	if (cfe->num_iospace != 1)
		return (0);

	if (pcmcia_io_alloc(sc->sc_pf,
	    cfe->iospace[0].start,
	    cfe->iospace[0].length,
	    cfe->iospace[0].length,
	    &sc->sc_modem_pcioh) == 0) {
		/* Found one for the modem! */
		sc->sc_flags |= MHZC_MODEM_ALLOCED;
		return (1);
	}

	return (0);
}

int
mhzc_alloc_ethernet(struct mhzc_softc *sc, struct pcmcia_config_entry *cfe)
{
	bus_addr_t addr, maxaddr;

	addr = cfe->iospace[0].start + cfe->iospace[0].length;
	maxaddr = 0x1000;

	/*
	 * Now round it up so that it starts on a 16-byte boundary.
	 */
	addr = roundup(addr, 0x10);

	for (; (addr + 0x10) < maxaddr; addr += 0x10) {
		if (addr & 0x80)
			continue;
		if (pcmcia_io_alloc(sc->sc_pf, addr, 0x10, 0x10,
		    &sc->sc_ethernet_pcioh) == 0) {
			/* Found one for the ethernet! */
			sc->sc_flags |= MHZC_ETHERNET_ALLOCED;
			return (1);
		}
	}

	return (0);
}

int
mhzc_print(void *aux, const char *pnp)
{
	const char *name = aux;

	if (pnp)
		aprint_normal("%s at %s(*)",  name, pnp);

	return (UNCONF);
}

void
mhzc_childdet(device_t self, device_t child)
{
	struct mhzc_softc *sc = device_private(self);

	if (sc->sc_ethernet == child)
		sc->sc_ethernet = NULL;
	if (sc->sc_modem == child)
		sc->sc_modem = NULL;
}

int
mhzc_detach(device_t self, int flags)
{
	struct mhzc_softc *sc = device_private(self);
	int rv;

	if (sc->sc_ethernet != NULL) {
		if ((rv = config_detach(sc->sc_ethernet, flags)) != 0)
			return rv;
	}

	if (sc->sc_modem != NULL) {
		if ((rv = config_detach(sc->sc_modem, flags)) != 0)
			return rv;
	}

	/* Unmap our i/o windows. */
	if (sc->sc_flags & MHZC_MODEM_MAPPED)
		pcmcia_io_unmap(sc->sc_pf, sc->sc_modem_io_window);
	if (sc->sc_flags & MHZC_ETHERNET_MAPPED)
		pcmcia_io_unmap(sc->sc_pf, sc->sc_ethernet_io_window);

	/* Free our i/o spaces. */
	if (sc->sc_flags & MHZC_ETHERNET_ALLOCED)
		pcmcia_io_free(sc->sc_pf, &sc->sc_modem_pcioh);
	if (sc->sc_flags & MHZC_MODEM_ALLOCED)
		pcmcia_io_free(sc->sc_pf, &sc->sc_ethernet_pcioh);

	sc->sc_flags = 0;

	return 0;
}

int
mhzc_intr(void *arg)
{
	struct mhzc_softc *sc = arg;
	int rval = 0;

#if NCOM_MHZC > 0
	if (sc->sc_modem != NULL &&
	    (sc->sc_flags & MHZC_MODEM_ENABLED) != 0)
		rval |= comintr(sc->sc_modem);
#endif

#if NSM_MHZC > 0
	if (sc->sc_ethernet != NULL &&
	    (sc->sc_flags & MHZC_ETHERNET_ENABLED) != 0)
		rval |= smc91cxx_intr(sc->sc_ethernet);
#endif

	return (rval);
}

int
mhzc_enable(struct mhzc_softc *sc, int flag)
{
	int error;

	if ((sc->sc_flags & flag) == flag) {
		printf("%s: already enabled\n", device_xname(sc->sc_dev));
		return (0);
	}

	if ((sc->sc_flags & (MHZC_MODEM_ENABLED|MHZC_ETHERNET_ENABLED)) != 0) {
		sc->sc_flags |= flag;
		return (0);
	}

	/*
	 * Establish our interrupt handler.
	 *
	 * XXX Note, we establish this at IPL_NET.  This is suboptimal
	 * XXX the Modem portion, but is necessary to make the Ethernet
	 * XXX portion have the correct interrupt level semantics.
	 *
	 * XXX Eventually we should use the `enabled' bits in the
	 * XXX flags word to determine which level we should be at.
	 */
	sc->sc_ih = pcmcia_intr_establish(sc->sc_pf, IPL_NET,
	    mhzc_intr, sc);
	if (!sc->sc_ih)
		return (EIO);

	error = pcmcia_function_enable(sc->sc_pf);
	if (error) {
		pcmcia_intr_disestablish(sc->sc_pf, sc->sc_ih);
		sc->sc_ih = 0;
		return (error);
	}

	/*
	 * Perform any special enable magic necessary.
	 */
	if (sc->sc_product->mp_enable != NULL &&
	    (*sc->sc_product->mp_enable)(sc) != 0) {
		pcmcia_function_disable(sc->sc_pf);
		pcmcia_intr_disestablish(sc->sc_pf, sc->sc_ih);
		return (1);
	}

	sc->sc_flags |= flag;
	return (0);
}

void
mhzc_disable(struct mhzc_softc *sc, int flag)
{

	if ((sc->sc_flags & flag) == 0) {
		printf("%s: already disabled\n", device_xname(sc->sc_dev));
		return;
	}

	sc->sc_flags &= ~flag;
	if ((sc->sc_flags & (MHZC_MODEM_ENABLED|MHZC_ETHERNET_ENABLED)) != 0)
		return;

	pcmcia_function_disable(sc->sc_pf);
	pcmcia_intr_disestablish(sc->sc_pf, sc->sc_ih);
	sc->sc_ih = 0;
}

/*****************************************************************************
 * Megahertz EM3336 (and compatibles) support
 *****************************************************************************/

int	mhzc_em3336_lannid_ciscallback(struct pcmcia_tuple *, void *);
int	mhzc_em3336_ascii_enaddr(const char *cisstr, u_int8_t *);

int
mhzc_em3336_enaddr(struct mhzc_softc *sc, u_int8_t *myla)
{

	/* Get the station address from CIS tuple 0x81. */
	if (pcmcia_scan_cis(device_parent(sc->sc_dev),
	    mhzc_em3336_lannid_ciscallback, myla) != 1) {
		printf("%s: unable to get Ethernet address from CIS\n",
		    device_xname(sc->sc_dev));
		return (0);
	}

	return (1);
}

int
mhzc_em3336_enable(struct mhzc_softc *sc)
{
	struct pcmcia_mem_handle memh;
	bus_size_t memoff;
	int memwin, reg;

	/*
	 * Bring the chip to live by touching its registers in the correct
	 * way (as per my reference... the Linux smc91c92_cs.c driver by
	 * David A. Hinds).
	 */

	/* Map the ISRPOWEREG. */
	if (pcmcia_mem_alloc(sc->sc_pf, 0x1000, &memh) != 0) {
		aprint_error_dev(sc->sc_dev, "unable to allocate memory space\n");
		return (1);
	}

	if (pcmcia_mem_map(sc->sc_pf, PCMCIA_MEM_ATTR, 0, 0x1000,
	    &memh, &memoff, &memwin)) {
		aprint_error_dev(sc->sc_dev, "unable to map memory space\n");
		pcmcia_mem_free(sc->sc_pf, &memh);
		return (1);
	}

	/*
	 * The magic sequence:
	 *
	 *	- read/write the CCR option register.
	 *	- read the ISRPOWEREG 2 times.
	 *	- read/write the CCR option register again.
	 */

	reg = pcmcia_ccr_read(sc->sc_pf, PCMCIA_CCR_OPTION);
	pcmcia_ccr_write(sc->sc_pf, PCMCIA_CCR_OPTION, reg);

	reg = bus_space_read_1(memh.memt, memh.memh, 0x380);
	delay(5);
	reg = bus_space_read_1(memh.memt, memh.memh, 0x380);

	tsleep(&mhzc_em3336_enable, PWAIT, "mhz3en", hz * 200 / 1000);

	reg = pcmcia_ccr_read(sc->sc_pf, PCMCIA_CCR_OPTION);
	delay(5);
	pcmcia_ccr_write(sc->sc_pf, PCMCIA_CCR_OPTION, reg);

	pcmcia_mem_unmap(sc->sc_pf, memwin);
	pcmcia_mem_free(sc->sc_pf, &memh);

	return (0);
}

int
mhzc_em3336_lannid_ciscallback(struct pcmcia_tuple *tuple, void *arg)
{
	u_int8_t *myla = arg, addr_str[ETHER_ADDR_LEN * 2];
	int i;

	if (tuple->code == 0x81) {
		/*
		 * We have a string-encoded address.  Length includes
		 * terminating 0xff.
		 */
		if (tuple->length != (ETHER_ADDR_LEN * 2) + 1)
			return (0);

		for (i = 0; i < tuple->length - 1; i++)
			addr_str[i] = pcmcia_tuple_read_1(tuple, i);

		/*
		 * Decode the string into `myla'.
		 */
		return (mhzc_em3336_ascii_enaddr(addr_str, myla));
	}
	return (0);
}

/* XXX This should be shared w/ if_sm_pcmcia.c */
int
mhzc_em3336_ascii_enaddr(const char *cisstr, u_int8_t *myla)
{
	u_int8_t digit;
	int i;

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

/****** Here begins the com attachment code. ******/

#if NCOM_MHZC > 0
int	com_mhzc_match(device_t, cfdata_t , void *);
void	com_mhzc_attach(device_t, device_t, void *);
int	com_mhzc_detach(device_t, int);

/* No mhzc-specific goo in the softc; it's all in the parent. */
CFATTACH_DECL_NEW(com_mhzc, sizeof(struct com_softc),
    com_mhzc_match, com_mhzc_attach, com_detach, NULL);

int	com_mhzc_enable(struct com_softc *);
void	com_mhzc_disable(struct com_softc *);

int
com_mhzc_match(device_t parent, cfdata_t match, void *aux)
{
	extern struct cfdriver com_cd;
	const char *name = aux;

	/* Device is always present. */
	if (strcmp(name, com_cd.cd_name) == 0)
		return (1);

	return (0);
}

void
com_mhzc_attach(device_t parent, device_t self, void *aux)
{
	struct com_softc *sc = device_private(self);
	struct mhzc_softc *msc = device_private(parent);

	sc->sc_dev = self;
	aprint_normal("\n");

	COM_INIT_REGS(sc->sc_regs, 
	    msc->sc_modem_pcioh.iot,
	    msc->sc_modem_pcioh.ioh,
	    -1);

	sc->enabled = 1;

	sc->sc_frequency = COM_FREQ;

	sc->enable = com_mhzc_enable;
	sc->disable = com_mhzc_disable;

	aprint_normal("%s", device_xname(self));

	com_attach_subr(sc);

	sc->enabled = 0;
}

int
com_mhzc_enable(struct com_softc *sc)
{

	return (mhzc_enable(device_private(device_parent(sc->sc_dev)),
	    MHZC_MODEM_ENABLED));
}

void
com_mhzc_disable(struct com_softc *sc)
{

	mhzc_disable(device_private(device_parent(sc->sc_dev)),
	    MHZC_MODEM_ENABLED);
}

#endif /* NCOM_MHZC > 0 */

/****** Here begins the sm attachment code. ******/

#if NSM_MHZC > 0
int	sm_mhzc_match(device_t, cfdata_t, void *);
void	sm_mhzc_attach(device_t, device_t, void *);

/* No mhzc-specific goo in the softc; it's all in the parent. */
CFATTACH_DECL_NEW(sm_mhzc, sizeof(struct smc91cxx_softc),
    sm_mhzc_match, sm_mhzc_attach, smc91cxx_detach, smc91cxx_activate);

int	sm_mhzc_enable(struct smc91cxx_softc *);
void	sm_mhzc_disable(struct smc91cxx_softc *);

int
sm_mhzc_match(device_t parent, cfdata_t match, void *aux)
{
	extern struct cfdriver sm_cd;
	const char *name = aux;

	/* Device is always present. */
	if (strcmp(name, sm_cd.cd_name) == 0)
		return (1);

	return (0);
}

void
sm_mhzc_attach(device_t parent, device_t self, void *aux)
{
	struct smc91cxx_softc *sc = device_private(self);
	struct mhzc_softc *msc = device_private(parent);
	u_int8_t myla[ETHER_ADDR_LEN];

	aprint_normal("\n");

	sc->sc_dev = self;
	sc->sc_bst = msc->sc_ethernet_pcioh.iot;
	sc->sc_bsh = msc->sc_ethernet_pcioh.ioh;

	sc->sc_enable = sm_mhzc_enable;
	sc->sc_disable = sm_mhzc_disable;

	if ((*msc->sc_product->mp_enaddr)(msc, myla) != 1)
		return;

	/* Perform generic initialization. */
	smc91cxx_attach(sc, myla);
}

int
sm_mhzc_enable(struct smc91cxx_softc *sc)
{
	struct mhzc_softc *xsc = device_private(device_parent(sc->sc_dev));

	return mhzc_enable(xsc, MHZC_ETHERNET_ENABLED);
}

void
sm_mhzc_disable(struct smc91cxx_softc *sc)
{
	struct mhzc_softc *xsc = device_private(device_parent(sc->sc_dev));

	mhzc_disable(xsc, MHZC_ETHERNET_ENABLED);
}

#endif /* NSM_MHZC > 0 */
