/*	$NetBSD: if_re_pci.c,v 1.44 2015/05/03 00:04:06 matt Exp $	*/

/*
 * Copyright (c) 1997, 1998-2003
 *	Bill Paul <wpaul@windriver.com>.  All rights reserved.
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
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

/* $FreeBSD: /repoman/r/ncvs/src/sys/dev/re/if_re.c,v 1.20 2004/04/11 20:34:08 ru Exp $ */

/*
 * RealTek 8139C+/8169/8169S/8110S PCI NIC driver
 *
 * Written by Bill Paul <wpaul@windriver.com>
 * Senior Networking Software Engineer
 * Wind River Systems
 *
 * NetBSD bus-specific frontends for written by
 * Jonathan Stone <jonathan@netbsd.org>
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: if_re_pci.c,v 1.44 2015/05/03 00:04:06 matt Exp $");

#include <sys/types.h>

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <net/if.h>
#include <net/if_ether.h>
#include <net/if_media.h>

#include <sys/bus.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/ic/rtl81x9reg.h>
#include <dev/ic/rtl81x9var.h>
#include <dev/ic/rtl8169var.h>

struct re_pci_softc {
	struct rtk_softc sc_rtk;

	void *sc_ih;
	pci_chipset_tag_t sc_pc;
};

static int	re_pci_match(device_t, cfdata_t, void *);
static void	re_pci_attach(device_t, device_t, void *);
static int	re_pci_detach(device_t, int);

CFATTACH_DECL_NEW(re_pci, sizeof(struct re_pci_softc),
    re_pci_match, re_pci_attach, re_pci_detach, NULL);

/*
 * Various supported device vendors/types and their names.
 */
static const struct rtk_type re_devs[] = {
	{ PCI_VENDOR_REALTEK, PCI_PRODUCT_REALTEK_RT8139,
	    RTK_8139CPLUS,
	    "RealTek 8139C+ 10/100BaseTX" },
	{ PCI_VENDOR_REALTEK, PCI_PRODUCT_REALTEK_RT8101E,
	    RTK_8101E,
	    "RealTek 8100E/8101E/8102E/8102EL PCIe 10/100BaseTX" },
	{ PCI_VENDOR_REALTEK, PCI_PRODUCT_REALTEK_RT8168,
	    RTK_8168,
	    "RealTek 8168/8111 PCIe Gigabit Ethernet" },
	{ PCI_VENDOR_REALTEK, PCI_PRODUCT_REALTEK_RT8169,
	    RTK_8169,
	    "RealTek 8169/8110 Gigabit Ethernet" },
	{ PCI_VENDOR_REALTEK, PCI_PRODUCT_REALTEK_RT8169SC,
	    RTK_8169,
	    "RealTek 8169SC/8110SC Single-chip Gigabit Ethernet" },
	{ PCI_VENDOR_COREGA, PCI_PRODUCT_COREGA_LAPCIGT,
	    RTK_8169,
	    "Corega CG-LAPCIGT Gigabit Ethernet" },
	{ PCI_VENDOR_DLINK, PCI_PRODUCT_DLINK_DGE528T,
	    RTK_8169,
	    "D-Link DGE-528T Gigabit Ethernet" },
	{ PCI_VENDOR_USR2, PCI_PRODUCT_USR2_USR997902,
	    RTK_8169,
	    "US Robotics (3Com) USR997902 Gigabit Ethernet" },
	{ PCI_VENDOR_LINKSYS, PCI_PRODUCT_LINKSYS_EG1032,
	    RTK_8169,
	    "Linksys EG1032 rev. 3 Gigabit Ethernet" },
};

static const struct rtk_type *
re_pci_lookup(const struct pci_attach_args * pa)
{
	int i;

	for(i = 0; i < __arraycount(re_devs); i++) {
		if (PCI_VENDOR(pa->pa_id) != re_devs[i].rtk_vid)
			continue;
		if (PCI_PRODUCT(pa->pa_id) == re_devs[i].rtk_did)
			return &re_devs[i];
	}

	return NULL;
}

#define RE_LINKSYS_EG1032_SUBID	0x00241737

/*
 * Probe for a RealTek 8139C+/8169/8110 chip. Check the PCI vendor and device
 * IDs against our list and return a device name if we find a match.
 */
static int
re_pci_match(device_t parent, cfdata_t cf, void *aux)
{
	struct pci_attach_args *pa = aux;
	pcireg_t subid;

	subid = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_SUBSYS_ID_REG);

	/* special-case Linksys EG1032, since rev 2 uses sk(4) */
	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_LINKSYS &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_LINKSYS_EG1032) {
		if (subid != RE_LINKSYS_EG1032_SUBID)
			return 0;
	}

	/* Don't match 8139 other than C-PLUS */
	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_REALTEK &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_REALTEK_RT8139) {
		if (PCI_REVISION(pa->pa_class) != 0x20)
			return 0;
	}

	if (re_pci_lookup(pa) != NULL)
		return 2;	/* defeat rtk(4) */

	return 0;
}

static void
re_pci_attach(device_t parent, device_t self, void *aux)
{
	struct re_pci_softc *psc = device_private(self);
	struct rtk_softc *sc = &psc->sc_rtk;
	struct pci_attach_args *pa = aux;
	pci_chipset_tag_t pc = pa->pa_pc;
	pci_intr_handle_t ih;
	const char *intrstr = NULL;
	const struct rtk_type *t;
	uint32_t hwrev;
	pcireg_t command, memtype;
	bool ioh_valid, memh_valid;
	bus_space_tag_t iot, memt;
	bus_space_handle_t ioh, memh;
	bus_size_t iosize, memsize;
	char intrbuf[PCI_INTRSTR_LEN];

	sc->sc_dev = self;
	psc->sc_pc = pa->pa_pc;

	command = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	command |= PCI_COMMAND_MASTER_ENABLE;
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG, command);

	/*
	 * Map control/status registers.
	 */
	ioh_valid = (pci_mapreg_map(pa, RTK_PCI_LOIO, PCI_MAPREG_TYPE_IO, 0,
	    &iot, &ioh, NULL, &iosize) == 0);
	memtype = pci_mapreg_type(pa->pa_pc, pa->pa_tag, RTK_PCI_LOMEM);
	switch (memtype) {
	case PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT:
	case PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_64BIT:
		memh_valid =
		    (pci_mapreg_map(pa, RTK_PCI_LOMEM,
		        memtype, 0, &memt, &memh, NULL, &memsize) == 0) ||
		    (pci_mapreg_map(pa, RTK_PCI_LOMEM + 4,
			PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_64BIT,
			0, &memt, &memh, NULL, &memsize) == 0);
		break;
	default:
		memh_valid = 0;
		break;
	}

	if (ioh_valid) {
		sc->rtk_btag = iot;
		sc->rtk_bhandle = ioh;
		sc->rtk_bsize = iosize;
		if (memh_valid)
			bus_space_unmap(memt, memh, memsize);
	} else if (memh_valid) {
		sc->rtk_btag = memt;
		sc->rtk_bhandle = memh;
		sc->rtk_bsize = memsize;
	} else {
		aprint_error(": can't map registers\n");
		return;
	}

	t = re_pci_lookup(pa);
	KASSERT(t != NULL);

	pci_aprint_devinfo_fancy(pa, NULL, t->rtk_name, 1);

	if (t->rtk_basetype == RTK_8139CPLUS)
		sc->sc_quirk |= RTKQ_8139CPLUS;

	if (t->rtk_basetype == RTK_8168 ||
	    t->rtk_basetype == RTK_8101E)
		sc->sc_quirk |= RTKQ_PCIE;

	if (pci_dma64_available(pa) && (sc->sc_quirk & RTKQ_PCIE))
		sc->sc_dmat = pa->pa_dmat64;
	else
		sc->sc_dmat = pa->pa_dmat;

	/*
	 * No power/enable/disable machinery for PCI attach;
	 * mark the card enabled now.
	 */
	sc->sc_flags |= RTK_ENABLED;

	/* Hook interrupt last to avoid having to lock softc */
	/* Allocate interrupt */
	if (pci_intr_map(pa, &ih)) {
		aprint_error_dev(self, "couldn't map interrupt\n");
		return;
	}
	intrstr = pci_intr_string(pc, ih, intrbuf, sizeof(intrbuf));
	psc->sc_ih = pci_intr_establish(pc, ih, IPL_NET, re_intr, sc);
	if (psc->sc_ih == NULL) {
		aprint_error_dev(self, "couldn't establish interrupt");
		if (intrstr != NULL)
			aprint_error(" at %s", intrstr);
		aprint_error("\n");
		return;
	}
	aprint_normal_dev(self, "interrupting at %s\n", intrstr);

	re_attach(sc);

	hwrev = CSR_READ_4(sc, RTK_TXCFG) & RTK_TXCFG_HWREV;

	/*
	 * Perform hardware diagnostic on the original RTL8169.
	 * Some 32-bit cards were incorrectly wired and would
	 * malfunction if plugged into a 64-bit slot.
	 */
	if (hwrev == RTK_HWREV_8169) {
		if (re_diag(sc)) {
			re_pci_detach(self, 0);
			aprint_error_dev(self, "disabled\n");
		}
	}
}

static int
re_pci_detach(device_t self, int flags)
{
	struct re_pci_softc *psc = device_private(self);
	struct rtk_softc *sc = &psc->sc_rtk;
	int rv;

	if ((sc->sc_flags & RTK_ATTACHED) != 0)
		/* re_stop() */
		sc->ethercom.ec_if.if_stop(&sc->ethercom.ec_if, 0);

	rv = re_detach(sc);
	if (rv)
		return rv;

	if (psc->sc_ih != NULL) {
		pci_intr_disestablish(psc->sc_pc, psc->sc_ih);
		psc->sc_ih = NULL;
	}

	if (sc->rtk_bsize != 0) {
		bus_space_unmap(sc->rtk_btag, sc->rtk_bhandle, sc->rtk_bsize);
		sc->rtk_bsize = 0;
	}

	return 0;
}
