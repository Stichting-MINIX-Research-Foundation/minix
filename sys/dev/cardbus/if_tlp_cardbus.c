/*	$NetBSD: if_tlp_cardbus.c,v 1.70 2011/08/01 11:20:27 drochner Exp $	*/

/*-
 * Copyright (c) 1999, 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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
 * CardBus bus front-end for the Digital Semiconductor ``Tulip'' (21x4x)
 * Ethernet controller family driver.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_tlp_cardbus.c,v 1.70 2011/08/01 11:20:27 drochner Exp $");

#include "opt_inet.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/device.h>

#include <machine/endian.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_ether.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_inarp.h>
#endif


#include <sys/bus.h>
#include <sys/intr.h>

#include <dev/mii/miivar.h>
#include <dev/mii/mii_bitbang.h>

#include <dev/ic/tulipreg.h>
#include <dev/ic/tulipvar.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>

#include <dev/cardbus/cardbusvar.h>
#include <dev/pci/pcidevs.h>

/*
 * PCI configuration space registers used by the Tulip.
 */
#define TULIP_PCI_IOBA PCI_BAR(0)	/* i/o mapped base */
#define TULIP_PCI_MMBA PCI_BAR(1)	/* memory mapped base */
#define	TULIP_PCI_CFDA		0x40	/* configuration driver area */

#define	CFDA_SLEEP		0x80000000	/* sleep mode */
#define	CFDA_SNOOZE		0x40000000	/* snooze mode */

struct tulip_cardbus_softc {
	struct tulip_softc sc_tulip;	/* real Tulip softc */

	/* CardBus-specific goo. */
	void	*sc_ih;			/* interrupt handle */
	cardbus_devfunc_t sc_ct;	/* our CardBus devfuncs */
	pcitag_t sc_tag;		/* our CardBus tag */
	pcireg_t sc_csr;
	bus_size_t sc_mapsize;		/* the size of mapped bus space
					   region */

	int	sc_bar_reg;		/* which BAR to use */
	pcireg_t sc_bar_val;		/* value of the BAR */
};

int	tlp_cardbus_match(device_t, cfdata_t, void *);
void	tlp_cardbus_attach(device_t, device_t, void *);
int	tlp_cardbus_detach(device_t, int);

CFATTACH_DECL_NEW(tlp_cardbus, sizeof(struct tulip_cardbus_softc),
    tlp_cardbus_match, tlp_cardbus_attach, tlp_cardbus_detach, tlp_activate);

const struct tulip_cardbus_product {
	u_int32_t	tcp_vendor;	/* PCI vendor ID */
	u_int32_t	tcp_product;	/* PCI product ID */
	tulip_chip_t	tcp_chip;	/* base Tulip chip type */
} tlp_cardbus_products[] = {
	{ PCI_VENDOR_DEC,	PCI_PRODUCT_DEC_21142,
	  TULIP_CHIP_21142 },

	{ PCI_VENDOR_XIRCOM,	PCI_PRODUCT_XIRCOM_X3201_3_21143,
	  TULIP_CHIP_X3201_3 },

	{ PCI_VENDOR_ADMTEK,	PCI_PRODUCT_ADMTEK_AN983,
	  TULIP_CHIP_AN985 },

	{ PCI_VENDOR_ACCTON,	PCI_PRODUCT_ACCTON_EN2242,
	  TULIP_CHIP_AN985 },

	{ PCI_VENDOR_ABOCOM,	PCI_PRODUCT_ABOCOM_FE2500,
	  TULIP_CHIP_AN985 },

	{ PCI_VENDOR_ABOCOM,	PCI_PRODUCT_ABOCOM_PCM200,
	  TULIP_CHIP_AN985 },

	{ PCI_VENDOR_ABOCOM,	PCI_PRODUCT_ABOCOM_FE2500MX,
	  TULIP_CHIP_AN985 },

	{ PCI_VENDOR_HAWKING,	PCI_PRODUCT_HAWKING_PN672TX,
	  TULIP_CHIP_AN985 },

	{ PCI_VENDOR_ADMTEK,	PCI_PRODUCT_ADMTEK_AN985,
	  TULIP_CHIP_AN985 },

	{ PCI_VENDOR_MICROSOFT,	PCI_PRODUCT_MICROSOFT_MN120,
	  TULIP_CHIP_AN985 },

	{ PCI_VENDOR_LINKSYS, PCI_PRODUCT_LINKSYS_PCMPC200,
	  TULIP_CHIP_AN985 },

	{ 0,				0,
	  TULIP_CHIP_INVALID },
};

struct tlp_cardbus_quirks {
	void		(*tpq_func)(struct tulip_cardbus_softc *,
			    const u_int8_t *);
	u_int8_t	tpq_oui[3];
};

void	tlp_cardbus_lxt_quirks(struct tulip_cardbus_softc *,
	    const u_int8_t *);

const struct tlp_cardbus_quirks tlp_cardbus_21142_quirks[] = {
	{ tlp_cardbus_lxt_quirks,	{ 0x00, 0x40, 0x05 } },
	{ NULL,				{ 0, 0, 0 } }
};

void	tlp_cardbus_setup(struct tulip_cardbus_softc *);

int	tlp_cardbus_enable(struct tulip_softc *);
void	tlp_cardbus_disable(struct tulip_softc *);
void	tlp_cardbus_power(struct tulip_softc *, int);

void	tlp_cardbus_x3201_reset(struct tulip_softc *);

const struct tulip_cardbus_product *tlp_cardbus_lookup
   (const struct cardbus_attach_args *);
void tlp_cardbus_get_quirks(struct tulip_cardbus_softc *,
    const u_int8_t *, const struct tlp_cardbus_quirks *);

const struct tulip_cardbus_product *
tlp_cardbus_lookup(const struct cardbus_attach_args *ca)
{
	const struct tulip_cardbus_product *tcp;

	for (tcp = tlp_cardbus_products; tcp->tcp_chip != TULIP_CHIP_INVALID;
	     tcp++) {
		if (PCI_VENDOR(ca->ca_id) == tcp->tcp_vendor &&
		    PCI_PRODUCT(ca->ca_id) == tcp->tcp_product)
			return (tcp);
	}
	return (NULL);
}

void
tlp_cardbus_get_quirks(struct tulip_cardbus_softc *csc, const u_int8_t *enaddr, const struct tlp_cardbus_quirks *tpq)
{

	for (; tpq->tpq_func != NULL; tpq++) {
		if (tpq->tpq_oui[0] == enaddr[0] &&
		    tpq->tpq_oui[1] == enaddr[1] &&
		    tpq->tpq_oui[2] == enaddr[2]) {
			(*tpq->tpq_func)(csc, enaddr);
			return;
		}
	}
}

int
tlp_cardbus_match(device_t parent, cfdata_t match,
    void *aux)
{
	struct cardbus_attach_args *ca = aux;

	if (tlp_cardbus_lookup(ca) != NULL)
		return (1);

	return (0);
}

void
tlp_cardbus_attach(device_t parent, device_t self,
    void *aux)
{
	struct tulip_cardbus_softc *csc = device_private(self);
	struct tulip_softc *sc = &csc->sc_tulip;
	struct cardbus_attach_args *ca = aux;
	cardbus_devfunc_t ct = ca->ca_ct;
	const struct tulip_cardbus_product *tcp;
	u_int8_t enaddr[ETHER_ADDR_LEN];
	bus_addr_t adr;
	pcireg_t reg;

	sc->sc_dev = self;
	sc->sc_devno = 0;
	sc->sc_dmat = ca->ca_dmat;
	csc->sc_ct = ct;
	csc->sc_tag = ca->ca_tag;

	tcp = tlp_cardbus_lookup(ca);
	if (tcp == NULL) {
		printf("\n");
		panic("tlp_cardbus_attach: impossible");
	}
	sc->sc_chip = tcp->tcp_chip;

	/*
	 * By default, Tulip registers are 8 bytes long (4 bytes
	 * followed by a 4 byte pad).
	 */
	sc->sc_regshift = 3;

	/*
	 * Power management hooks.
	 */
	sc->sc_enable = tlp_cardbus_enable;
	sc->sc_disable = tlp_cardbus_disable;
	sc->sc_power = tlp_cardbus_power;

	/*
	 * Get revision info, and set some chip-specific variables.
	 */
	sc->sc_rev = PCI_REVISION(ca->ca_class);
	switch (sc->sc_chip) {
	case TULIP_CHIP_21142:
		if (sc->sc_rev >= 0x20)
			sc->sc_chip = TULIP_CHIP_21143;
		break;

	case TULIP_CHIP_AN985:
		/*
		 * The AN983 and AN985 are very similar, and are
		 * differentiated by a "signature" register that
		 * is like, but not identical, to a PCI ID register.
		 */
		reg = Cardbus_conf_read(ct, csc->sc_tag, 0x80);
		switch (reg) {
		case 0x09811317:
			sc->sc_chip = TULIP_CHIP_AN985;
			break;

		case 0x09851317:
			sc->sc_chip = TULIP_CHIP_AN983;
			break;

		}
		break;

	default:
		/* Nothing. -- to make gcc happy */
		break;
	}

	printf(": %s Ethernet, pass %d.%d\n",
	    tlp_chip_name(sc->sc_chip),
	    (sc->sc_rev >> 4) & 0xf, sc->sc_rev & 0xf);

	/*
	 * Map the device.
	 */
	csc->sc_csr = PCI_COMMAND_MASTER_ENABLE;
	if (Cardbus_mapreg_map(ct, TULIP_PCI_MMBA,
	    PCI_MAPREG_TYPE_MEM, 0, &sc->sc_st, &sc->sc_sh, &adr,
	    &csc->sc_mapsize) == 0) {
		csc->sc_csr |= PCI_COMMAND_MEM_ENABLE;
		csc->sc_bar_reg = TULIP_PCI_MMBA;
		csc->sc_bar_val = adr | PCI_MAPREG_TYPE_MEM;
	} else if (Cardbus_mapreg_map(ct, TULIP_PCI_IOBA,
	    PCI_MAPREG_TYPE_IO, 0, &sc->sc_st, &sc->sc_sh, &adr,
	    &csc->sc_mapsize) == 0) {
		csc->sc_csr |= PCI_COMMAND_IO_ENABLE;
		csc->sc_bar_reg = TULIP_PCI_IOBA;
		csc->sc_bar_val = adr | PCI_MAPREG_TYPE_IO;
	} else {
		aprint_error_dev(self, "unable to map device registers\n");
		return;
	}

	/*
	 * Bring the chip out of powersave mode and initialize the
	 * configuration registers.
	 */
	tlp_cardbus_setup(csc);

	/*
	 * Read the contents of the Ethernet Address ROM/SROM.
	 */
	switch (sc->sc_chip) {
	case TULIP_CHIP_X3201_3:
		/*
		 * No SROM on this chip.
		 */
		break;

	default:
		if (tlp_read_srom(sc) == 0)
			goto cant_cope;
		break;
	}

	/*
	 * Deal with chip/board quirks.  This includes setting up
	 * the mediasw, and extracting the Ethernet address from
	 * the rombuf.
	 */
	switch (sc->sc_chip) {
	case TULIP_CHIP_21142:
	case TULIP_CHIP_21143:
		/* Check for new format SROM. */
		if (tlp_isv_srom_enaddr(sc, enaddr) != 0) {
			/*
			 * We start out with the 2114x ISV media switch.
			 * When we search for quirks, we may change to
			 * a different switch.
			 */
			sc->sc_mediasw = &tlp_2114x_isv_mediasw;
		} else if (tlp_parse_old_srom(sc, enaddr) == 0) {
			/*
			 * Not an ISV SROM, and not in old DEC Address
			 * ROM format.  Try to snarf it out of the CIS.
			 */
			if (ca->ca_cis.funce.network.netid_present == 0)
				goto cant_cope;

			/* Grab the MAC address from the CIS. */
			memcpy(enaddr, ca->ca_cis.funce.network.netid,
			    sizeof(enaddr));
		}

		/*
		 * Deal with any quirks this board might have.
		 */
		tlp_cardbus_get_quirks(csc, enaddr, tlp_cardbus_21142_quirks);

		/*
		 * If we don't already have a media switch, default to
		 * MII-over-SIO, with no special reset routine.
		 */
		if (sc->sc_mediasw == NULL) {
			printf("%s: defaulting to MII-over-SIO; no bets...\n",
			    device_xname(self));
			sc->sc_mediasw = &tlp_sio_mii_mediasw;
		}
		break;

	case TULIP_CHIP_AN983:
	case TULIP_CHIP_AN985:
		/*
		 * The ADMtek AN985's Ethernet address is located
		 * at offset 8 of its EEPROM.
		 */
		memcpy(enaddr, &sc->sc_srom[8], ETHER_ADDR_LEN);

		/*
		 * The ADMtek AN985 can be configured in Single-Chip
		 * mode or MAC-only mode.  Single-Chip uses the built-in
		 * PHY, MAC-only has an external PHY (usually HomePNA).
		 * The selection is based on an EEPROM setting, and both
		 * PHYs are access via MII attached to SIO.
		 *
		 * The AN985 "ghosts" the internal PHY onto all
		 * MII addresses, so we have to use a media init
		 * routine that limits the search.
		 * XXX How does this work with MAC-only mode?
		 */
		sc->sc_mediasw = &tlp_an985_mediasw;
		break;

	case TULIP_CHIP_X3201_3:
		/*
		 * The X3201 doesn't have an SROM.  Lift the MAC address
		 * from the CIS.  Also, we have a special media switch:
		 * MII-on-SIO, plus some special GPIO setup.
		 */
		memcpy(enaddr, ca->ca_cis.funce.network.netid, sizeof(enaddr));
		sc->sc_reset = tlp_cardbus_x3201_reset;
		sc->sc_mediasw = &tlp_sio_mii_mediasw;
		break;

	default:
 cant_cope:
		printf("%s: sorry, unable to handle your board\n",
		    device_xname(self));
		return;
	}

	/*
	 * Finish off the attach.
	 */
	tlp_attach(sc, enaddr);

	/*
	 * Power down the socket.
	 */
	Cardbus_function_disable(csc->sc_ct);
}

int
tlp_cardbus_detach(device_t self, int flags)
{
	struct tulip_cardbus_softc *csc = device_private(self);
	struct tulip_softc *sc = &csc->sc_tulip;
	struct cardbus_devfunc *ct = csc->sc_ct;
	int rv;

#if defined(DIAGNOSTIC)
	if (ct == NULL)
		panic("%s: data structure lacks", device_xname(self));
#endif

	rv = tlp_detach(sc);
	if (rv)
		return (rv);

	/*
	 * Unhook the interrupt handler.
	 */
	if (csc->sc_ih != NULL)
		Cardbus_intr_disestablish(ct, csc->sc_ih);

	/*
	 * Release bus space and close window.
	 */
	if (csc->sc_bar_reg != 0)
		Cardbus_mapreg_unmap(ct, csc->sc_bar_reg,
		    sc->sc_st, sc->sc_sh, csc->sc_mapsize);

	return (0);
}

int
tlp_cardbus_enable(struct tulip_softc *sc)
{
	struct tulip_cardbus_softc *csc = (void *) sc;
	cardbus_devfunc_t ct = csc->sc_ct;

	/*
	 * Power on the socket.
	 */
	Cardbus_function_enable(ct);

	/*
	 * Set up the PCI configuration registers.
	 */
	tlp_cardbus_setup(csc);

	/*
	 * Map and establish the interrupt.
	 */
	csc->sc_ih = Cardbus_intr_establish(ct, IPL_NET, tlp_intr, sc);
	if (csc->sc_ih == NULL) {
		aprint_error_dev(sc->sc_dev,
				 "unable to establish interrupt\n");
		Cardbus_function_disable(csc->sc_ct);
		return (1);
	}
	return (0);
}

void
tlp_cardbus_disable(struct tulip_softc *sc)
{
	struct tulip_cardbus_softc *csc = (void *) sc;
	cardbus_devfunc_t ct = csc->sc_ct;

	/* Unhook the interrupt handler. */
	Cardbus_intr_disestablish(ct, csc->sc_ih);
	csc->sc_ih = NULL;

	/* Power down the socket. */
	Cardbus_function_disable(ct);
}

void
tlp_cardbus_power(struct tulip_softc *sc, int why)
{

	switch (why) {
	case PWR_RESUME:
		tlp_cardbus_enable(sc);
		break;
	case PWR_SUSPEND:
		tlp_cardbus_disable(sc);
		break;
	}
}

void
tlp_cardbus_setup(struct tulip_cardbus_softc *csc)
{
	struct tulip_softc *sc = &csc->sc_tulip;
	cardbus_devfunc_t ct = csc->sc_ct;
	pcireg_t reg;

	/*
	 * Check to see if the device is in power-save mode, and
	 * bring it out if necessary.
	 */
	switch (sc->sc_chip) {
	case TULIP_CHIP_21142:
	case TULIP_CHIP_21143:
	case TULIP_CHIP_X3201_3:
		/*
		 * Clear the "sleep mode" bit in the CFDA register.
		 */
		reg = Cardbus_conf_read(ct, csc->sc_tag, TULIP_PCI_CFDA);
		if (reg & (CFDA_SLEEP|CFDA_SNOOZE))
			Cardbus_conf_write(ct, csc->sc_tag, TULIP_PCI_CFDA,
			    reg & ~(CFDA_SLEEP|CFDA_SNOOZE));
		break;

	default:
		/* Nothing. -- to make gcc happy */
		break;
	}

	(void)cardbus_set_powerstate(ct, csc->sc_tag, PCI_PWR_D0);

	/* Program the BAR. */
	Cardbus_conf_write(ct, csc->sc_tag, csc->sc_bar_reg, csc->sc_bar_val);

	/* Enable the appropriate bits in the PCI CSR. */
	reg = Cardbus_conf_read(ct, csc->sc_tag, PCI_COMMAND_STATUS_REG);
	reg &= ~(PCI_COMMAND_IO_ENABLE|PCI_COMMAND_MEM_ENABLE);
	reg |= csc->sc_csr;
	Cardbus_conf_write(ct, csc->sc_tag, PCI_COMMAND_STATUS_REG, reg);

	/*
	 * Make sure the latency timer is set to some reasonable
	 * value.
	 */
	reg = Cardbus_conf_read(ct, csc->sc_tag, PCI_BHLC_REG);
	if (PCI_LATTIMER(reg) < 0x20) {
		reg &= ~(PCI_LATTIMER_MASK << PCI_LATTIMER_SHIFT);
		reg |= (0x20 << PCI_LATTIMER_SHIFT);
		Cardbus_conf_write(ct, csc->sc_tag, PCI_BHLC_REG, reg);
	}
}

void
tlp_cardbus_x3201_reset(struct tulip_softc *sc)
{
	u_int32_t reg;

	reg = TULIP_READ(sc, CSR_SIAGEN);

	/* make GP[2,0] outputs */
	TULIP_WRITE(sc, CSR_SIAGEN, (reg & ~SIAGEN_MD) | SIAGEN_CWE |
	    0x00050000);
	TULIP_WRITE(sc, CSR_SIAGEN, (reg & ~SIAGEN_CWE) | SIAGEN_MD);
}

void
tlp_cardbus_lxt_quirks(struct tulip_cardbus_softc *csc,
    const u_int8_t *enaddr)
{
	struct tulip_softc *sc = &csc->sc_tulip;

	sc->sc_mediasw = &tlp_sio_mii_mediasw;
}
