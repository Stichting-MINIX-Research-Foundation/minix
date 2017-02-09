/*	$NetBSD: if_ex_cardbus.c,v 1.55 2011/08/01 11:20:27 drochner Exp $	*/

/*
 * Copyright (c) 1998 and 1999
 *       HAYAKAWA Koichi.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY HAYAKAWA KOICHI ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL TAKESHI OHASHI OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * CardBus specific routines for 3Com 3C575-family CardBus ethernet adapter
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_ex_cardbus.c,v 1.55 2011/08/01 11:20:27 drochner Exp $");

/* #define EX_DEBUG 4 */	/* define to report information for debugging */

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

#include <dev/cardbus/cardbusvar.h>
#include <dev/pci/pcidevs.h>

#include <dev/mii/miivar.h>

#include <dev/ic/elink3var.h>
#include <dev/ic/elink3reg.h>
#include <dev/ic/elinkxlreg.h>
#include <dev/ic/elinkxlvar.h>

#if defined DEBUG && !defined EX_DEBUG
#define EX_DEBUG
#endif

#if defined EX_DEBUG
#define DPRINTF(a) printf a
#else
#define DPRINTF(a)
#endif

#define CARDBUS_3C575BTX_FUNCSTAT_PCIREG  PCI_BAR2  /* means 0x18 */
#define EX_CB_INTR 4		/* intr acknowledge reg. CardBus only */
#define EX_CB_INTR_ACK 0x8000 /* intr acknowledge bit */

int ex_cardbus_match(device_t, cfdata_t, void *);
void ex_cardbus_attach(device_t, device_t, void *);
int ex_cardbus_detach(device_t, int);
void ex_cardbus_intr_ack(struct ex_softc *);

int ex_cardbus_enable(struct ex_softc *);
void ex_cardbus_disable(struct ex_softc *);

struct ex_cardbus_softc {
	struct ex_softc sc_softc;

	cardbus_devfunc_t sc_ct;
	uint8_t sc_cardbus_flags;
#define EX_REATTACH		0x01
#define EX_ABSENT		0x02
	uint8_t sc_cardtype;
#define EX_CB_BOOMERANG		1
#define EX_CB_CYCLONE		2

	/* CardBus function status space.  575B requests it. */
	bus_space_tag_t sc_funct;
	bus_space_handle_t sc_funch;
	bus_size_t sc_funcsize;

	bus_size_t sc_mapsize;		/* the size of mapped bus space region */

	pcitag_t sc_tag;

	pcireg_t	sc_csr;
	int	sc_bar_reg;		/* which BAR to use */
	pcireg_t sc_bar_val;		/* value of the BAR */
	int	sc_bar_reg1;		/* which BAR to use */
	pcireg_t sc_bar_val1;		/* value of the BAR */

};

CFATTACH_DECL3_NEW(ex_cardbus, sizeof(struct ex_cardbus_softc),
    ex_cardbus_match, ex_cardbus_attach, ex_cardbus_detach, ex_activate,
    NULL, NULL, DVF_DETACH_SHUTDOWN);

const struct ex_cardbus_product {
	uint32_t	ecp_prodid;	/* CardBus product ID */
	int		ecp_flags;	/* initial softc flags */
	pcireg_t	ecp_csr;	/* PCI CSR flags */
	int		ecp_cardtype;	/* card type */
	const char	*ecp_name;	/* device name */
} ex_cardbus_products[] = {
	{ PCI_PRODUCT_3COM_3C575TX,
	  EX_CONF_MII | EX_CONF_EEPROM_OFF | EX_CONF_EEPROM_8BIT,
	  PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MASTER_ENABLE,
	  EX_CB_BOOMERANG,
	  "3c575-TX Ethernet" },

	{ PCI_PRODUCT_3COM_3C575BTX,
	  EX_CONF_90XB|EX_CONF_MII|EX_CONF_INV_LED_POLARITY |
	    EX_CONF_EEPROM_OFF | EX_CONF_EEPROM_8BIT,
	  PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MEM_ENABLE |
	      PCI_COMMAND_MASTER_ENABLE,
	  EX_CB_CYCLONE,
	  "3c575B-TX Ethernet" },

	{ PCI_PRODUCT_3COM_3C575CTX,
	  EX_CONF_90XB | EX_CONF_PHY_POWER | EX_CONF_EEPROM_OFF |
	    EX_CONF_EEPROM_8BIT,
	  PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MEM_ENABLE |
	      PCI_COMMAND_MASTER_ENABLE,
	  EX_CB_CYCLONE,
	  "3c575CT Ethernet" },

	{ PCI_PRODUCT_3COM_3C656_E,
	  EX_CONF_90XB | EX_CONF_PHY_POWER | EX_CONF_EEPROM_OFF |
	    EX_CONF_EEPROM_8BIT | EX_CONF_INV_LED_POLARITY,
	  PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MEM_ENABLE |
	      PCI_COMMAND_MASTER_ENABLE,
	  EX_CB_CYCLONE,
	  "3c656-TX Ethernet" },

	{ PCI_PRODUCT_3COM_3C656B_E,
	  EX_CONF_90XB | EX_CONF_PHY_POWER | EX_CONF_EEPROM_OFF |
	    EX_CONF_EEPROM_8BIT | EX_CONF_INV_LED_POLARITY,
	  PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MEM_ENABLE |
	      PCI_COMMAND_MASTER_ENABLE,
	  EX_CB_CYCLONE,
	  "3c656B-TX Ethernet" },

	{ PCI_PRODUCT_3COM_3C656C_E,
	  EX_CONF_90XB | EX_CONF_PHY_POWER | EX_CONF_EEPROM_OFF |
	    EX_CONF_EEPROM_8BIT,
	  PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MEM_ENABLE |
	      PCI_COMMAND_MASTER_ENABLE,
	  EX_CB_CYCLONE,
	  "3c656C-TX Ethernet" },

	{ 0,
	  0,
	  0,
	  0,
	  NULL },
};


void ex_cardbus_setup(struct ex_cardbus_softc *);

const struct ex_cardbus_product *ex_cardbus_lookup
   (const struct cardbus_attach_args *);

const struct ex_cardbus_product *
ex_cardbus_lookup(const struct cardbus_attach_args *ca)
{
	const struct ex_cardbus_product *ecp;

	if (PCI_VENDOR(ca->ca_id) != PCI_VENDOR_3COM)
		return (NULL);

	for (ecp = ex_cardbus_products; ecp->ecp_name != NULL; ecp++)
		if (PCI_PRODUCT(ca->ca_id) == ecp->ecp_prodid)
			return (ecp);
	return (NULL);
}

int
ex_cardbus_match(device_t parent, cfdata_t cf, void *aux)
{
	struct cardbus_attach_args *ca = aux;

	if (ex_cardbus_lookup(ca) != NULL)
		return (1);

	return (0);
}

void
ex_cardbus_attach(device_t parent, device_t self, void *aux)
{
	struct ex_cardbus_softc *csc = device_private(self);
	struct ex_softc *sc = &csc->sc_softc;
	struct cardbus_attach_args *ca = aux;
	cardbus_devfunc_t ct = ca->ca_ct;
	const struct ex_cardbus_product *ecp;
	bus_addr_t adr, adr1;

	sc->sc_dev = self;

	sc->sc_dmat = ca->ca_dmat;
	csc->sc_ct = ca->ca_ct;
	csc->sc_tag = ca->ca_tag;

	ecp = ex_cardbus_lookup(ca);
	if (ecp == NULL) {
		printf("\n");
		panic("ex_cardbus_attach: impossible");
	}

	aprint_normal(": 3Com %s\n", ecp->ecp_name);

	sc->ex_conf = ecp->ecp_flags;
	csc->sc_cardtype = ecp->ecp_cardtype;
	csc->sc_csr = ecp->ecp_csr;

	if (Cardbus_mapreg_map(ct, PCI_BAR0, PCI_MAPREG_TYPE_IO, 0,
		&sc->sc_iot, &sc->sc_ioh, &adr, &csc->sc_mapsize) == 0) {
		csc->sc_bar_reg = PCI_BAR0;
		csc->sc_bar_val = adr | PCI_MAPREG_TYPE_IO;

		if (csc->sc_cardtype == EX_CB_CYCLONE) {
			/* Map CardBus function status window. */
			if (Cardbus_mapreg_map(ct,
				CARDBUS_3C575BTX_FUNCSTAT_PCIREG,
		    		PCI_MAPREG_TYPE_MEM, 0,
				 &csc->sc_funct, &csc->sc_funch,
				 &adr1, &csc->sc_funcsize) == 0) {

				csc->sc_bar_reg1 =
					CARDBUS_3C575BTX_FUNCSTAT_PCIREG;
				csc->sc_bar_val1 =
					adr1 | PCI_MAPREG_TYPE_MEM;

			} else {
				aprint_error_dev(self, "unable to map function "
					"status window\n");
				return;
			}

			/* Setup interrupt acknowledge hook */
			sc->intr_ack = ex_cardbus_intr_ack;
		}
	}
	else {
		aprint_naive(": can't map i/o space\n");
		return;
	}

	/* Power management hooks. */
	sc->enable = ex_cardbus_enable;
	sc->disable = ex_cardbus_disable;

	/*
	 *  Handle power management nonsense and
	 * initialize the configuration registers.
	 */
	ex_cardbus_setup(csc);

	ex_config(sc);

	if (csc->sc_cardtype == EX_CB_CYCLONE)
		bus_space_write_4(csc->sc_funct, csc->sc_funch,
		    EX_CB_INTR, EX_CB_INTR_ACK);

	Cardbus_function_disable(csc->sc_ct);
}

void
ex_cardbus_intr_ack(struct ex_softc *sc)
{
	struct ex_cardbus_softc *csc = (struct ex_cardbus_softc *)sc;

	bus_space_write_4(csc->sc_funct, csc->sc_funch, EX_CB_INTR,
	    EX_CB_INTR_ACK);
}

int
ex_cardbus_detach(device_t self, int flags)
{
	struct ex_cardbus_softc *csc = device_private(self);
	struct ex_softc *sc = &csc->sc_softc;
	struct cardbus_devfunc *ct = csc->sc_ct;
	int rv;

#if defined(DIAGNOSTIC)
	if (ct == NULL) {
		panic("%s: data structure lacks", device_xname(self));
	}
#endif

	if ((rv = ex_detach(sc)) != 0)
		return rv;

	/*
	 * Unhook the interrupt handler.
	 */
	Cardbus_intr_disestablish(ct, sc->sc_ih);

	if (csc->sc_cardtype == EX_CB_CYCLONE) {
		Cardbus_mapreg_unmap(ct,
		    CARDBUS_3C575BTX_FUNCSTAT_PCIREG,
		    csc->sc_funct, csc->sc_funch, csc->sc_funcsize);
	}

	Cardbus_mapreg_unmap(ct, PCI_BAR0, sc->sc_iot,
	    sc->sc_ioh, csc->sc_mapsize);
	return 0;
}

int
ex_cardbus_enable(struct ex_softc *sc)
{
	struct ex_cardbus_softc *csc = (struct ex_cardbus_softc *)sc;

	Cardbus_function_enable(csc->sc_ct);
	ex_cardbus_setup(csc);

	sc->sc_ih = Cardbus_intr_establish(csc->sc_ct, IPL_NET, ex_intr, sc);
	if (NULL == sc->sc_ih) {
		aprint_error_dev(sc->sc_dev, "couldn't establish interrupt\n");
		return (1);
	}

	return (0);
}

void
ex_cardbus_disable(struct ex_softc *sc)
{
	struct ex_cardbus_softc *csc = (struct ex_cardbus_softc *)sc;

	if (sc->sc_ih != NULL) {
		Cardbus_intr_disestablish(csc->sc_ct, sc->sc_ih);
		sc->sc_ih = NULL;
	}

 	Cardbus_function_disable(csc->sc_ct);

}

void
ex_cardbus_setup(struct ex_cardbus_softc *csc)
{
	cardbus_devfunc_t ct = csc->sc_ct;
	pcireg_t reg;

	(void)cardbus_set_powerstate(ct, csc->sc_tag, PCI_PWR_D0);

	/* Program the BAR */
	Cardbus_conf_write(ct, csc->sc_tag, csc->sc_bar_reg, csc->sc_bar_val);
	/* Make sure the right access type is on the CardBus bridge. */
	if (csc->sc_cardtype == EX_CB_CYCLONE) {
		/* Program the BAR */
		Cardbus_conf_write(ct, csc->sc_tag,
			csc->sc_bar_reg1, csc->sc_bar_val1);
		/*
		 * Make sure CardBus brigde can access memory space.  Usually
		 * memory access is enabled by BIOS, but some BIOSes do not
		 * enable it.
		 */
	}

	/* Enable the appropriate bits in the CARDBUS CSR. */
	reg = Cardbus_conf_read(ct, csc->sc_tag, PCI_COMMAND_STATUS_REG);
	reg |= csc->sc_csr;
	Cardbus_conf_write(ct, csc->sc_tag, PCI_COMMAND_STATUS_REG, reg);

 	/*
	 * set latency timer
	 */
	reg = Cardbus_conf_read(ct, csc->sc_tag, PCI_BHLC_REG);
	if (PCI_LATTIMER(reg) < 0x20) {
		/* at least the value of latency timer should 0x20. */
		DPRINTF(("if_ex_cardbus: lattimer 0x%x -> 0x20\n",
		    PCI_LATTIMER(reg)));
		reg &= ~(PCI_LATTIMER_MASK << PCI_LATTIMER_SHIFT);
		reg |= (0x20 << PCI_LATTIMER_SHIFT);
		Cardbus_conf_write(ct, csc->sc_tag, PCI_BHLC_REG, reg);
	}
}
