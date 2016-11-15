/* $NetBSD: com_cardbus.c,v 1.30 2011/08/01 11:20:27 drochner Exp $ */

/*
 * Copyright (c) 2000 Johan Danielsson
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of author nor the names of any contributors may
 *    be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* A driver for CardBus based serial devices.

   If the CardBus device only has one BAR (that is not also the CIS
   BAR) listed in the CIS, it is assumed to be the one to use. For
   devices with more than one BAR, the list of known devices has to be
   updated below.  */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: com_cardbus.c,v 1.30 2011/08/01 11:20:27 drochner Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/tty.h>
#include <sys/device.h>

#include <dev/cardbus/cardbusvar.h>
#include <dev/pci/pcidevs.h>

#include <dev/pcmcia/pcmciareg.h>

#include <dev/ic/comreg.h>
#include <dev/ic/comvar.h>

struct com_cardbus_softc {
	struct com_softc	cc_com;
	void			*cc_ih;
	cardbus_devfunc_t	cc_ct;
	bus_addr_t		cc_addr;
	pcireg_t		cc_base;
	bus_size_t		cc_size;
	pcireg_t		cc_csr;
	pcitag_t		cc_tag;
	pcireg_t		cc_reg;
	int			cc_type;
};

#define DEVICET(CSC) ((CSC)->cc_com.sc_dev)

static int com_cardbus_match (device_t, cfdata_t, void*);
static void com_cardbus_attach (device_t, device_t, void*);
static int com_cardbus_detach (device_t, int);

static void com_cardbus_setup(struct com_cardbus_softc*);
static int com_cardbus_enable (struct com_softc*);
static void com_cardbus_disable(struct com_softc*);

CFATTACH_DECL_NEW(com_cardbus, sizeof(struct com_cardbus_softc),
    com_cardbus_match, com_cardbus_attach, com_cardbus_detach, NULL);

static struct csdev {
	int		vendor;
	int		product;
	pcireg_t	reg;
	int		type;
} csdevs[] = {
	{ PCI_VENDOR_XIRCOM, PCI_PRODUCT_XIRCOM_MODEM56,
	  PCI_BAR0, PCI_MAPREG_TYPE_IO },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_MODEM56,
	  PCI_BAR0, PCI_MAPREG_TYPE_IO },
	{ PCI_VENDOR_3COM, PCI_PRODUCT_3COM_3C656_M,
	  PCI_BAR0, PCI_MAPREG_TYPE_IO },
	{ PCI_VENDOR_3COM, PCI_PRODUCT_3COM_3C656B_M,
	  PCI_BAR0, PCI_MAPREG_TYPE_IO },
	{ PCI_VENDOR_3COM, PCI_PRODUCT_3COM_3C656C_M,
	  PCI_BAR0, PCI_MAPREG_TYPE_IO },
};

static const int ncsdevs = sizeof(csdevs) / sizeof(csdevs[0]);

static struct csdev*
find_csdev(struct cardbus_attach_args *ca)
{
	struct csdev *cp;

	for(cp = csdevs; cp < csdevs + ncsdevs; cp++)
		if(cp->vendor == PCI_VENDOR(ca->ca_id) &&
		   cp->product == PCI_PRODUCT(ca->ca_id))
			return cp;
	return NULL;
}

static int
com_cardbus_match(device_t parent, cfdata_t match, void *aux)
{
	struct cardbus_attach_args *ca = aux;

	/* known devices are ok */
	if(find_csdev(ca) != NULL)
	    return 10;

	/* as are serial devices with a known UART */
	if(ca->ca_cis.funcid == PCMCIA_FUNCTION_SERIAL &&
	   ca->ca_cis.funce.serial.uart_present != 0 &&
	   (ca->ca_cis.funce.serial.uart_type == 0 ||	/* 8250 */
	    ca->ca_cis.funce.serial.uart_type == 1 ||	/* 16450 */
	    ca->ca_cis.funce.serial.uart_type == 2))	/* 16550 */
	    return 1;

	return 0;
}

static int
gofigure(struct cardbus_attach_args *ca, struct com_cardbus_softc *csc)
{
	int i, index = -1;
	pcireg_t cis_ptr;
	struct csdev *cp;

	/* If this device is listed above, use the known values, */
	cp = find_csdev(ca);
	if(cp != NULL) {
		csc->cc_reg = cp->reg;
		csc->cc_type = cp->type;
		return 0;
	}

	cis_ptr = Cardbus_conf_read(csc->cc_ct, csc->cc_tag, CARDBUS_CIS_REG);

	/* otherwise try to deduce which BAR and type to use from CIS.  If
	   there is only one BAR, it must be the one we should use, if
	   there are more, we're out of luck.  */
	for(i = 0; i < 7; i++) {
		/* ignore zero sized BARs */
		if(ca->ca_cis.bar[i].size == 0)
			continue;
		/* ignore the CIS BAR */
		if(CARDBUS_CIS_ASI_BAR(cis_ptr) ==
		   CARDBUS_CIS_ASI_BAR(ca->ca_cis.bar[i].flags))
			continue;
		if(index != -1)
			goto multi_bar;
		index = i;
	}
	if(index == -1) {
		aprint_error(": couldn't find any base address tuple\n");
		return 1;
	}
	csc->cc_reg = CARDBUS_CIS_ASI_BAR(ca->ca_cis.bar[index].flags);
	if ((ca->ca_cis.bar[index].flags & 0x10) == 0)
		csc->cc_type = PCI_MAPREG_TYPE_MEM;
	else
		csc->cc_type = PCI_MAPREG_TYPE_IO;
	return 0;

  multi_bar:
	aprint_error(": there are more than one possible base\n");

	aprint_error_dev(DEVICET(csc), "address for this device, "
	       "please report the following information\n");
	aprint_error_dev(DEVICET(csc), "vendor 0x%x product 0x%x\n",
	       PCI_VENDOR(ca->ca_id), PCI_PRODUCT(ca->ca_id));
	for(i = 0; i < 7; i++) {
		/* ignore zero sized BARs */
		if(ca->ca_cis.bar[i].size == 0)
			continue;
		/* ignore the CIS BAR */
		if(CARDBUS_CIS_ASI_BAR(cis_ptr) ==
		   CARDBUS_CIS_ASI_BAR(ca->ca_cis.bar[i].flags))
			continue;
		aprint_error_dev(DEVICET(csc),
		       "base address %x type %s size %x\n",
		       CARDBUS_CIS_ASI_BAR(ca->ca_cis.bar[i].flags),
		       (ca->ca_cis.bar[i].flags & 0x10) ? "i/o" : "mem",
		       ca->ca_cis.bar[i].size);
	}
	return 1;
}

static void
com_cardbus_attach (device_t parent, device_t self, void *aux)
{
	struct com_softc *sc = device_private(self);
	struct com_cardbus_softc *csc = device_private(self);
	struct cardbus_attach_args *ca = aux;
	bus_space_handle_t	ioh;
	bus_space_tag_t		iot;

	sc->sc_dev = self;
	csc->cc_ct = ca->ca_ct;
	csc->cc_tag = ca->ca_tag;

	if(gofigure(ca, csc) != 0)
		return;

	if(Cardbus_mapreg_map(ca->ca_ct,
			      csc->cc_reg,
			      csc->cc_type,
			      0,
			      &iot,
			      &ioh,
			      &csc->cc_addr,
			      &csc->cc_size) != 0) {
		aprint_error("failed to map memory");
		return;
	}

	COM_INIT_REGS(sc->sc_regs, iot, ioh, csc->cc_addr);

	csc->cc_base = csc->cc_addr;
	csc->cc_csr = PCI_COMMAND_MASTER_ENABLE;
	if(csc->cc_type == PCI_MAPREG_TYPE_IO) {
		csc->cc_base |= PCI_MAPREG_TYPE_IO;
		csc->cc_csr |= PCI_COMMAND_IO_ENABLE;
	} else {
		csc->cc_csr |= PCI_COMMAND_MEM_ENABLE;
	}

	sc->sc_frequency = COM_FREQ;

	sc->enable = com_cardbus_enable;
	sc->disable = com_cardbus_disable;
	sc->enabled = 0;

	if (ca->ca_cis.cis1_info[0] && ca->ca_cis.cis1_info[1]) {
		aprint_normal(": %s %s\n", ca->ca_cis.cis1_info[0],
		    ca->ca_cis.cis1_info[1]);
		aprint_normal("%s", device_xname(DEVICET(csc)));
	}

	com_cardbus_setup(csc);

	com_attach_subr(sc);

	Cardbus_function_disable(csc->cc_ct);
}

static void
com_cardbus_setup(struct com_cardbus_softc *csc)
{
        cardbus_devfunc_t ct = csc->cc_ct;
	pcireg_t reg;

	Cardbus_conf_write(ct, csc->cc_tag, csc->cc_reg, csc->cc_base);

	/* and the card itself */
	reg = Cardbus_conf_read(ct, csc->cc_tag, PCI_COMMAND_STATUS_REG);
	reg &= ~(PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MEM_ENABLE);
	reg |= csc->cc_csr;
	Cardbus_conf_write(ct, csc->cc_tag, PCI_COMMAND_STATUS_REG, reg);

        /*
         * Make sure the latency timer is set to some reasonable
         * value.
         */
        reg = Cardbus_conf_read(ct, csc->cc_tag, PCI_BHLC_REG);
        if (PCI_LATTIMER(reg) < 0x20) {
                reg &= ~(PCI_LATTIMER_MASK << PCI_LATTIMER_SHIFT);
                reg |= (0x20 << PCI_LATTIMER_SHIFT);
                Cardbus_conf_write(ct, csc->cc_tag, PCI_BHLC_REG, reg);
        }
}

static int
com_cardbus_enable(struct com_softc *sc)
{
	struct com_cardbus_softc *csc = (struct com_cardbus_softc*)sc;
	cardbus_devfunc_t ct = csc->cc_ct;

	Cardbus_function_enable(ct);

	com_cardbus_setup(csc);

	/* establish the interrupt. */
	csc->cc_ih = Cardbus_intr_establish(ct, IPL_SERIAL, comintr, sc);
	if (csc->cc_ih == NULL) {
		aprint_error_dev(DEVICET(csc),
		    "couldn't establish interrupt\n");
		return 1;
	}

	return 0;
}

static void
com_cardbus_disable(struct com_softc *sc)
{
	struct com_cardbus_softc *csc = (struct com_cardbus_softc*)sc;
	cardbus_devfunc_t ct = csc->cc_ct;

	Cardbus_intr_disestablish(ct, csc->cc_ih);
	csc->cc_ih = NULL;

	Cardbus_function_disable(ct);
}

static int
com_cardbus_detach(device_t self, int flags)
{
	struct com_cardbus_softc *csc = device_private(self);
	struct com_softc *sc = device_private(self);
	cardbus_devfunc_t ct = csc->cc_ct;
	int error;

	if ((error = com_detach(self, flags)) != 0)
		return error;

	if (csc->cc_ih != NULL)
		Cardbus_intr_disestablish(ct, csc->cc_ih);

	Cardbus_mapreg_unmap(csc->cc_ct, csc->cc_reg, sc->sc_regs.cr_iot,
	    sc->sc_regs.cr_ioh, csc->cc_size);

	return 0;
}
