/*	$NetBSD: puc.c,v 1.38 2015/05/04 21:21:39 ryo Exp $	*/

/*
 * Copyright (c) 1996, 1998, 1999
 *	Christopher G. Demetriou.  All rights reserved.
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
 *      This product includes software developed by Christopher G. Demetriou
 *	for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 * PCI "universal" communication card device driver, glues com, lpt,
 * and similar ports to PCI via bridge chip often much larger than
 * the devices being glued.
 *
 * Author: Christopher G. Demetriou, May 14, 1998 (derived from NetBSD
 * sys/dev/pci/pciide.c, revision 1.6).
 *
 * These devices could be (and some times are) described as
 * communications/{serial,parallel}, etc. devices with known
 * programming interfaces, but those programming interfaces (in
 * particular the BAR assignments for devices, etc.) in fact are not
 * particularly well defined.
 *
 * After I/we have seen more of these devices, it may be possible
 * to generalize some of these bits.  In particular, devices which
 * describe themselves as communications/serial/16[45]50, and
 * communications/parallel/??? might be attached via direct
 * 'com' and 'lpt' attachments to pci.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: puc.c,v 1.38 2015/05/04 21:21:39 ryo Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pucvar.h>
#include <dev/pci/pcidevs.h>
#include <sys/termios.h>
#include <dev/ic/comreg.h>
#include <dev/ic/comvar.h>

#include "locators.h"
#include "com.h"

struct puc_softc {
	/* static configuration data */
	const struct puc_device_description *sc_desc;

	/* card-global dynamic data */
	void			*sc_ih;
	struct {
		int		mapped;
		bus_addr_t	a;
		bus_size_t	s;
		bus_space_tag_t	t;
		bus_space_handle_t h;
	} sc_bar_mappings[6];				/* XXX constant */

	/* per-port dynamic data */
        struct {
		device_t 	dev;

                /* filled in by port attachments */
                int             (*ihand)(void *);
                void            *ihandarg;
        } sc_ports[PUC_MAX_PORTS];
};

static int	puc_print(void *, const char *);

static const char *puc_port_type_name(int);

static int
puc_match(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa = aux;
	const struct puc_device_description *desc;
	pcireg_t bhlc, subsys;

	bhlc = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_BHLC_REG);
	if (PCI_HDRTYPE_TYPE(bhlc) != 0)
		return (0);

	subsys = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_SUBSYS_ID_REG);

	desc = puc_find_description(PCI_VENDOR(pa->pa_id),
	    PCI_PRODUCT(pa->pa_id), PCI_VENDOR(subsys), PCI_PRODUCT(subsys));
	if (desc != NULL)
		return (10);

#if 0
	/*
	 * XXX this is obviously bogus.  eventually, we might want
	 * XXX to match communications/modem, etc., but that needs some
	 * XXX special work in the match fn.
	 */
	/*
	 * Match class/subclass, so we can tell people to compile kernel
	 * with options that cause this driver to spew.
	 */
	if (PCI_CLASS(pa->pa_class) == PCI_CLASS_COMMUNICATIONS) {
		switch (PCI_SUBCLASS(pa->pa_class)) {
		case PCI_SUBCLASS_COMMUNICATIONS_SERIAL:
		case PCI_SUBCLASS_COMMUNICATIONS_MODEM:
			return (1);
		}
	}
#endif

	return (0);
}

static void
puc_attach(device_t parent, device_t self, void *aux)
{
	struct puc_softc *sc = device_private(self);
	struct pci_attach_args *pa = aux;
	struct puc_attach_args paa;
	pci_intr_handle_t intrhandle;
	pcireg_t subsys;
	int i, barindex;
	int locs[PUCCF_NLOCS];

	subsys = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_SUBSYS_ID_REG);
	sc->sc_desc = puc_find_description(PCI_VENDOR(pa->pa_id),
	    PCI_PRODUCT(pa->pa_id), PCI_VENDOR(subsys), PCI_PRODUCT(subsys));
	if (sc->sc_desc == NULL) {
		/*
		 * This was a class/subclass match, so tell people to compile
		 * kernel with options that cause this driver to spew.
		 */
#ifdef PUC_PRINT_REGS
		printf(":\n");
		pci_conf_print(pa->pa_pc, pa->pa_tag, NULL);
#else
		printf(": unknown PCI communications device\n");
		printf("%s: compile kernel with PUC_PRINT_REGS and larger\n",
		    device_xname(self));
		printf("%s: message buffer (via 'options MSGBUFSIZE=...'),\n",
		    device_xname(self));
		printf("%s: and report the result with send-pr\n",
		    device_xname(self));
#endif
		return;
	}

	aprint_naive("\n");
	aprint_normal(": %s (", sc->sc_desc->name);
	for (i = 0; PUC_PORT_VALID(sc->sc_desc, i); i++)
		aprint_normal("%s%s", i ? ", " : "",
		    puc_port_type_name(sc->sc_desc->ports[i].type));
	aprint_normal(")\n");

	for (i = 0; i < 6; i++) {
		pcireg_t bar, type;

		sc->sc_bar_mappings[i].mapped = 0;

		bar = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_BAR(i));
		if (bar == 0)			/* BAR not implemented(?) */
			continue;

		type = (PCI_MAPREG_TYPE(bar) == PCI_MAPREG_TYPE_IO ?
		    PCI_MAPREG_TYPE_IO : PCI_MAPREG_MEM_TYPE(bar));

		if (type == PCI_MAPREG_TYPE_IO) {
			sc->sc_bar_mappings[i].t = pa->pa_iot;
			sc->sc_bar_mappings[i].a = PCI_MAPREG_IO_ADDR(bar);
			sc->sc_bar_mappings[i].s = PCI_MAPREG_IO_SIZE(bar);
		} else {
			sc->sc_bar_mappings[i].t = pa->pa_memt;
			sc->sc_bar_mappings[i].a = PCI_MAPREG_MEM_ADDR(bar);
			sc->sc_bar_mappings[i].s = PCI_MAPREG_MEM_SIZE(bar);
		}

		sc->sc_bar_mappings[i].mapped = (pci_mapreg_map(pa,
		    PCI_BAR(i), type, 0,
		    &sc->sc_bar_mappings[i].t, &sc->sc_bar_mappings[i].h,
		    &sc->sc_bar_mappings[i].a, &sc->sc_bar_mappings[i].s)
		      == 0);
		if (sc->sc_bar_mappings[i].mapped)
			continue;

		aprint_debug_dev(self, "couldn't map BAR at offset 0x%lx\n",
		    (long)(PCI_MAPREG_START + 4 * i));
	}

	/* Map interrupt. */
	if (pci_intr_map(pa, &intrhandle)) {
		aprint_error_dev(self, "couldn't map interrupt\n");
		return;
	}
	/*
	 * XXX the sub-devices establish the interrupts, for the
	 * XXX following reasons:
	 * XXX
	 * XXX    * we can't really know what IPLs they'd want
	 * XXX
	 * XXX    * the MD dispatching code can ("should") dispatch
	 * XXX      chained interrupts better than we can.
	 * XXX
	 * XXX It would be nice if we could indicate to the MD interrupt
	 * XXX handling code that the interrupt line used by the device
	 * XXX was a PCI (level triggered) interrupt.
	 * XXX
	 * XXX It's not pretty, but hey, what is?
	 */

	/* SB16C10xx board specific initialization */
	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_SYSTEMBASE &&
	    (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_SYSTEMBASE_SB16C1050 ||
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_SYSTEMBASE_SB16C1054 ||
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_SYSTEMBASE_SB16C1058)) {
		if (!sc->sc_bar_mappings[1].mapped) {
			aprint_error_dev(self,
			    "optional register is not mapped\n");
			return;
		}
#define SB16C105X_OPT_IMRREG0 0x0000000c
		/* enable port 0-7 interrupt */
		bus_space_write_1(sc->sc_bar_mappings[1].t,
		    sc->sc_bar_mappings[1].h, SB16C105X_OPT_IMRREG0, 0xff);
	} else {
		if (!pmf_device_register(self, NULL, NULL))
	                aprint_error_dev(self,
			    "couldn't establish power handler\n");
	}

	/* Configure each port. */
	for (i = 0; PUC_PORT_VALID(sc->sc_desc, i); i++) {
		barindex = PUC_PORT_BAR_INDEX(sc->sc_desc->ports[i].bar);
		bus_space_handle_t subregion_handle;
		int is_console = 0;

		/* make sure the base address register is mapped */
#if NCOM > 0
		is_console = com_is_console(sc->sc_bar_mappings[barindex].t,
		    sc->sc_bar_mappings[barindex].a +
		    sc->sc_desc->ports[i].offset, &subregion_handle);
		if (is_console) {
			sc->sc_bar_mappings[barindex].mapped = 1;
#if defined(amd64) || defined(i386)
			sc->sc_bar_mappings[barindex].h = subregion_handle -
			    sc->sc_desc->ports[i].offset;	/* XXX hack */
#endif
		}
#endif
		if (!sc->sc_bar_mappings[barindex].mapped) {
			aprint_error_dev(self,
			    "%s port uses unmapped BAR (0x%x)\n",
			    puc_port_type_name(sc->sc_desc->ports[i].type),
			    sc->sc_desc->ports[i].bar);
			continue;
		}

		/* set up to configure the child device */
		paa.port = i;
		paa.type = sc->sc_desc->ports[i].type;
		paa.flags = sc->sc_desc->ports[i].flags;
		paa.pc = pa->pa_pc;
		paa.tag = pa->pa_tag;
		paa.intrhandle = intrhandle;
		paa.a = sc->sc_bar_mappings[barindex].a +
		    sc->sc_desc->ports[i].offset;
		paa.t = sc->sc_bar_mappings[barindex].t;
		paa.dmat = pa->pa_dmat;
		paa.dmat64 = pa->pa_dmat64;

		if (!is_console &&
		    bus_space_subregion(sc->sc_bar_mappings[barindex].t,
		    sc->sc_bar_mappings[barindex].h,
		    sc->sc_desc->ports[i].offset,
		    sc->sc_bar_mappings[barindex].s -
		      sc->sc_desc->ports[i].offset,
		    &subregion_handle) != 0) {
			aprint_error_dev(self, "couldn't get subregion for port %d\n", i);
			continue;
		}
		paa.h = subregion_handle;

#if 0
		printf("%s: port %d: %s @ (index %d) 0x%x (0x%lx, 0x%lx)\n",
		    device_xname(self), paa.port,
		    puc_port_type_name(paa.type), barindex, (int)paa.a,
		    (long)paa.t, (long)paa.h);
#endif

		locs[PUCCF_PORT] = i;

		/* and configure it */
		sc->sc_ports[i].dev = config_found_sm_loc(self, "puc", locs,
			&paa, puc_print, config_stdsubmatch);
	}
}

CFATTACH_DECL_NEW(puc, sizeof(struct puc_softc),
    puc_match, puc_attach, NULL, NULL);

static int
puc_print(void *aux, const char *pnp)
{
	struct puc_attach_args *paa = aux;

	if (pnp)
		aprint_normal("%s at %s", puc_port_type_name(paa->type), pnp);
	aprint_normal(" port %d", paa->port);
	return (UNCONF);
}

const struct puc_device_description *
puc_find_description(pcireg_t vend, pcireg_t prod, pcireg_t svend,
    pcireg_t sprod)
{
	int i;

#define checkreg(val, index) \
    (((val) & puc_devices[i].rmask[(index)]) == puc_devices[i].rval[(index)])

	for (i = 0; puc_devices[i].name != NULL; i++) {
		if (checkreg(vend, PUC_REG_VEND) &&
		    checkreg(prod, PUC_REG_PROD) &&
		    checkreg(svend, PUC_REG_SVEND) &&
		    checkreg(sprod, PUC_REG_SPROD))
			return (&puc_devices[i]);
	}

#undef checkreg

	return (NULL);
}

static const char *
puc_port_type_name(int type)
{

	switch (type) {
	case PUC_PORT_TYPE_COM:
		return "com";
	case PUC_PORT_TYPE_LPT:
		return "lpt";
	default:
		panic("puc_port_type_name %d", type);
	}
}
