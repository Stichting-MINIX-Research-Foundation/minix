/*	$NetBSD: ehci_pci.c,v 1.62 2015/08/31 10:41:22 skrll Exp $	*/

/*
 * Copyright (c) 2001, 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net).
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
__KERNEL_RCSID(0, "$NetBSD: ehci_pci.c,v 1.62 2015/08/31 10:41:22 skrll Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/queue.h>

#include <sys/bus.h>

#include <dev/pci/pcidevs.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/usb_pci.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usb_mem.h>

#include <dev/usb/ehcireg.h>
#include <dev/usb/ehcivar.h>

#ifdef EHCI_DEBUG
#define DPRINTF(x)	if (ehcidebug) printf x
extern int ehcidebug;
#else
#define DPRINTF(x)
#endif

enum ehci_pci_quirk_flags {
	EHCI_PCI_QUIRK_AMD_SB600 = 0x1,	/* always need a quirk */
	EHCI_PCI_QUIRK_AMD_SB700 = 0x2,	/* depends on the SMB revision */
};

static const struct pci_quirkdata ehci_pci_quirks[] = {
	{ PCI_VENDOR_ATI, PCI_PRODUCT_ATI_SB600_USB_EHCI,
	    EHCI_PCI_QUIRK_AMD_SB600 },
	{ PCI_VENDOR_ATI, PCI_PRODUCT_ATI_SB700_USB_EHCI,
	    EHCI_PCI_QUIRK_AMD_SB700 },
};

static void ehci_release_ownership(ehci_softc_t *sc, pci_chipset_tag_t pc,
				   pcitag_t tag);
static void ehci_get_ownership(ehci_softc_t *sc, pci_chipset_tag_t pc,
			       pcitag_t tag);
static bool ehci_pci_suspend(device_t, const pmf_qual_t *);
static bool ehci_pci_resume(device_t, const pmf_qual_t *);

struct ehci_pci_softc {
	ehci_softc_t		sc;
	pci_chipset_tag_t	sc_pc;
	pcitag_t		sc_tag;
	void 			*sc_ih;		/* interrupt vectoring */
};

static int ehci_sb700_match(const struct pci_attach_args *pa);
static int ehci_apply_amd_quirks(struct ehci_pci_softc *sc);
enum ehci_pci_quirk_flags ehci_pci_lookup_quirkdata(pci_vendor_id_t,
	pci_product_id_t);

#define EHCI_MAX_BIOS_WAIT		100 /* ms*10 */
#define EHCI_SBx00_WORKAROUND_REG	0x50
#define EHCI_SBx00_WORKAROUND_ENABLE	__BIT(27)


static int
ehci_pci_match(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa = (struct pci_attach_args *) aux;

	if (PCI_CLASS(pa->pa_class) == PCI_CLASS_SERIALBUS &&
	    PCI_SUBCLASS(pa->pa_class) == PCI_SUBCLASS_SERIALBUS_USB &&
	    PCI_INTERFACE(pa->pa_class) == PCI_INTERFACE_EHCI)
		return 1;

	return 0;
}

static void
ehci_pci_attach(device_t parent, device_t self, void *aux)
{
	struct ehci_pci_softc *sc = device_private(self);
	struct pci_attach_args *pa = (struct pci_attach_args *)aux;
	pci_chipset_tag_t pc = pa->pa_pc;
	pcitag_t tag = pa->pa_tag;
	char const *intrstr;
	pci_intr_handle_t ih;
	pcireg_t csr;
	usbd_status r;
	int ncomp;
	struct usb_pci *up;
	int quirk;
	char intrbuf[PCI_INTRSTR_LEN];

	sc->sc.sc_dev = self;
	sc->sc.sc_bus.hci_private = sc;

	pci_aprint_devinfo(pa, "USB controller");

	/* Check for quirks */
	quirk = ehci_pci_lookup_quirkdata(PCI_VENDOR(pa->pa_id),
					   PCI_PRODUCT(pa->pa_id));

	/* Map I/O registers */
	if (pci_mapreg_map(pa, PCI_CBMEM, PCI_MAPREG_TYPE_MEM, 0,
			   &sc->sc.iot, &sc->sc.ioh, NULL, &sc->sc.sc_size)) {
		sc->sc.sc_size = 0;
		aprint_error_dev(self, "can't map memory space\n");
		return;
	}

	sc->sc_pc = pc;
	sc->sc_tag = tag;
	sc->sc.sc_bus.dmatag = pa->pa_dmat;

	/* Disable interrupts, so we don't get any spurious ones. */
	sc->sc.sc_offs = EREAD1(&sc->sc, EHCI_CAPLENGTH);
	DPRINTF(("%s: offs=%d\n", device_xname(self), sc->sc.sc_offs));
	EOWRITE4(&sc->sc, EHCI_USBINTR, 0);

	/* Handle quirks */
	switch (quirk) {
	case EHCI_PCI_QUIRK_AMD_SB600:
		ehci_apply_amd_quirks(sc);
		break;
	case EHCI_PCI_QUIRK_AMD_SB700:
		if (pci_find_device(NULL, ehci_sb700_match))
			ehci_apply_amd_quirks(sc);
		break;
	}

	/* Enable the device. */
	csr = pci_conf_read(pc, tag, PCI_COMMAND_STATUS_REG);
	pci_conf_write(pc, tag, PCI_COMMAND_STATUS_REG,
		       csr | PCI_COMMAND_MASTER_ENABLE);

	/* Map and establish the interrupt. */
	if (pci_intr_map(pa, &ih)) {
		aprint_error_dev(self, "couldn't map interrupt\n");
		goto fail;
	}

	/*
	 * Allocate IRQ
	 */
	intrstr = pci_intr_string(pc, ih, intrbuf, sizeof(intrbuf));
	sc->sc_ih = pci_intr_establish(pc, ih, IPL_USB, ehci_intr, sc);
	if (sc->sc_ih == NULL) {
		aprint_error_dev(self, "couldn't establish interrupt");
		if (intrstr != NULL)
			aprint_error(" at %s", intrstr);
		aprint_error("\n");
		goto fail;
	}
	aprint_normal_dev(self, "interrupting at %s\n", intrstr);

	switch(pci_conf_read(pc, tag, PCI_USBREV) & PCI_USBREV_MASK) {
	case PCI_USBREV_PRE_1_0:
	case PCI_USBREV_1_0:
	case PCI_USBREV_1_1:
		sc->sc.sc_bus.usbrev = USBREV_UNKNOWN;
		aprint_verbose_dev(self, "pre-2.0 USB rev\n");
		goto fail;
	case PCI_USBREV_2_0:
		sc->sc.sc_bus.usbrev = USBREV_2_0;
		break;
	default:
		sc->sc.sc_bus.usbrev = USBREV_UNKNOWN;
		break;
	}

	/* Figure out vendor for root hub descriptor. */
	sc->sc.sc_id_vendor = PCI_VENDOR(pa->pa_id);
	pci_findvendor(sc->sc.sc_vendor,
	    sizeof(sc->sc.sc_vendor), sc->sc.sc_id_vendor);
	/* Enable workaround for dropped interrupts as required */
	switch (sc->sc.sc_id_vendor) {
	case PCI_VENDOR_ATI:
	case PCI_VENDOR_VIATECH:
		sc->sc.sc_flags |= EHCIF_DROPPED_INTR_WORKAROUND;
		aprint_normal_dev(self, "dropped intr workaround enabled\n");
		break;
	default:
		break;
	}

	/*
	 * Find companion controllers.  According to the spec they always
	 * have lower function numbers so they should be enumerated already.
	 */
	const u_int maxncomp = EHCI_HCS_N_CC(EREAD4(&sc->sc, EHCI_HCSPARAMS));
	KASSERT(maxncomp <= EHCI_COMPANION_MAX);
	ncomp = 0;
	TAILQ_FOREACH(up, &ehci_pci_alldevs, next) {
		if (up->bus == pa->pa_bus && up->device == pa->pa_device
		    && !up->claimed) {
			DPRINTF(("ehci_pci_attach: companion %s\n",
				 device_xname(up->usb)));
			sc->sc.sc_comps[ncomp++] = up->usb;
			up->claimed = true;
			if (ncomp == maxncomp)
				break;
		}
	}
	sc->sc.sc_ncomp = ncomp;

	ehci_get_ownership(&sc->sc, pc, tag);

	r = ehci_init(&sc->sc);
	if (r != USBD_NORMAL_COMPLETION) {
		aprint_error_dev(self, "init failed, error=%d\n", r);
		goto fail;
	}

	if (!pmf_device_register1(self, ehci_pci_suspend, ehci_pci_resume,
	                          ehci_shutdown))
		aprint_error_dev(self, "couldn't establish power handler\n");

	/* Attach usb device. */
	sc->sc.sc_child = config_found(self, &sc->sc.sc_bus, usbctlprint);
	return;

fail:
	if (sc->sc_ih) {
		pci_intr_disestablish(sc->sc_pc, sc->sc_ih);
		sc->sc_ih = NULL;
	}
	if (sc->sc.sc_size) {
		ehci_release_ownership(&sc->sc, sc->sc_pc, sc->sc_tag);
		bus_space_unmap(sc->sc.iot, sc->sc.ioh, sc->sc.sc_size);
		sc->sc.sc_size = 0;
	}
	return;
}

static int
ehci_pci_detach(device_t self, int flags)
{
	struct ehci_pci_softc *sc = device_private(self);
	int rv;

	rv = ehci_detach(&sc->sc, flags);
	if (rv)
		return rv;

	pmf_device_deregister(self);
	ehci_shutdown(self, flags);

	/* disable interrupts */
	EOWRITE4(&sc->sc, EHCI_USBINTR, 0);
	/* XXX grotty hack to flush the write */
	(void)EOREAD4(&sc->sc, EHCI_USBINTR);

	if (sc->sc_ih != NULL) {
		pci_intr_disestablish(sc->sc_pc, sc->sc_ih);
		sc->sc_ih = NULL;
	}
	if (sc->sc.sc_size) {
		ehci_release_ownership(&sc->sc, sc->sc_pc, sc->sc_tag);
		bus_space_unmap(sc->sc.iot, sc->sc.ioh, sc->sc.sc_size);
		sc->sc.sc_size = 0;
	}

#if 1
	/* XXX created in ehci.c */
	mutex_destroy(&sc->sc.sc_lock);
	mutex_destroy(&sc->sc.sc_intr_lock);

	softint_disestablish(sc->sc.sc_doorbell_si);
	softint_disestablish(sc->sc.sc_pcd_si);
#endif

	return 0;
}

CFATTACH_DECL3_NEW(ehci_pci, sizeof(struct ehci_pci_softc),
    ehci_pci_match, ehci_pci_attach, ehci_pci_detach, ehci_activate, NULL,
    ehci_childdet, DVF_DETACH_SHUTDOWN);

#ifdef EHCI_DEBUG
static void
ehci_dump_caps(ehci_softc_t *sc, pci_chipset_tag_t pc, pcitag_t tag)
{
	uint32_t cparams, legctlsts, addr, cap, id;
	int maxdump = 10;

	cparams = EREAD4(sc, EHCI_HCCPARAMS);
	addr = EHCI_HCC_EECP(cparams);
	while (addr != 0) {
		cap = pci_conf_read(pc, tag, addr);
		id = EHCI_CAP_GET_ID(cap);
		switch (id) {
		case EHCI_CAP_ID_LEGACY:
			legctlsts = pci_conf_read(pc, tag,
			    addr + PCI_EHCI_USBLEGCTLSTS);
			printf("ehci_dump_caps: legsup=0x%08x "
			       "legctlsts=0x%08x\n", cap, legctlsts);
			break;
		default:
			printf("ehci_dump_caps: cap=0x%08x\n", cap);
			break;
		}
		if (--maxdump < 0)
			break;
		addr = EHCI_CAP_GET_NEXT(cap);
	}
}
#endif

static void
ehci_release_ownership(ehci_softc_t *sc, pci_chipset_tag_t pc, pcitag_t tag)
{
	const char *devname = device_xname(sc->sc_dev);
	uint32_t cparams, addr, cap;
	pcireg_t legsup;
	int maxcap = 10;

	cparams = EREAD4(sc, EHCI_HCCPARAMS);
	addr = EHCI_HCC_EECP(cparams);
	while (addr != 0) {
		cap = pci_conf_read(pc, tag, addr);
		if (EHCI_CAP_GET_ID(cap) != EHCI_CAP_ID_LEGACY)
			goto next;
		legsup = pci_conf_read(pc, tag, addr + PCI_EHCI_USBLEGSUP);
		pci_conf_write(pc, tag, addr + PCI_EHCI_USBLEGSUP,
		    legsup & ~EHCI_LEG_HC_OS_OWNED);

next:
		if (--maxcap < 0) {
			aprint_normal("%s: broken extended capabilities "
				      "ignored\n", devname);
			return;
		}
		addr = EHCI_CAP_GET_NEXT(cap);
	}
}

static void
ehci_get_ownership(ehci_softc_t *sc, pci_chipset_tag_t pc, pcitag_t tag)
{
	const char *devname = device_xname(sc->sc_dev);
	uint32_t cparams, addr, cap;
	pcireg_t legsup;
	int maxcap = 10;
	int ms;

#ifdef EHCI_DEBUG
	if (ehcidebug)
		ehci_dump_caps(sc, pc, tag);
#endif
	cparams = EREAD4(sc, EHCI_HCCPARAMS);
	addr = EHCI_HCC_EECP(cparams);
	while (addr != 0) {
		cap = pci_conf_read(pc, tag, addr);
		if (EHCI_CAP_GET_ID(cap) != EHCI_CAP_ID_LEGACY)
			goto next;
		legsup = pci_conf_read(pc, tag, addr + PCI_EHCI_USBLEGSUP);
		if (legsup & EHCI_LEG_HC_BIOS_OWNED) {
			/* Ask BIOS to give up ownership */
			pci_conf_write(pc, tag, addr + PCI_EHCI_USBLEGSUP,
			    legsup | EHCI_LEG_HC_OS_OWNED);
			for (ms = 0; ms < EHCI_MAX_BIOS_WAIT; ms++) {
				legsup = pci_conf_read(pc, tag,
				    addr + PCI_EHCI_USBLEGSUP);
				if (!(legsup & EHCI_LEG_HC_BIOS_OWNED))
					break;
				delay(10000);
			}
			if (ms == EHCI_MAX_BIOS_WAIT) {
				aprint_normal("%s: BIOS refuses to give up "
				    "ownership, using force\n", devname);
				pci_conf_write(pc, tag,
				    addr + PCI_EHCI_USBLEGSUP, 0);
			} else
				aprint_verbose("%s: BIOS has given up "
				    "ownership\n", devname);
		}

		/* Disable SMIs */
		pci_conf_write(pc, tag, addr + PCI_EHCI_USBLEGCTLSTS, 0);

next:
		if (--maxcap < 0) {
			aprint_normal("%s: broken extended capabilities "
				      "ignored\n", devname);
			return;
		}
		addr = EHCI_CAP_GET_NEXT(cap);
	}

}

static bool
ehci_pci_suspend(device_t dv, const pmf_qual_t *qual)
{
	struct ehci_pci_softc *sc = device_private(dv);

	ehci_suspend(dv, qual);
	ehci_release_ownership(&sc->sc, sc->sc_pc, sc->sc_tag);

	return true;
}

static bool
ehci_pci_resume(device_t dv, const pmf_qual_t *qual)
{
	struct ehci_pci_softc *sc = device_private(dv);

	ehci_get_ownership(&sc->sc, sc->sc_pc, sc->sc_tag);
	return ehci_resume(dv, qual);
}

static int
ehci_sb700_match(const struct pci_attach_args *pa)
{
	if (!(PCI_VENDOR(pa->pa_id) == PCI_VENDOR_ATI &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_ATI_SB600_SMB))
		return 0;

	switch (PCI_REVISION(pa->pa_class)) {
	case 0x3a:
	case 0x3b:
		return 1;
	}

	return 0;
}

static int
ehci_apply_amd_quirks(struct ehci_pci_softc *sc)
{
	pcireg_t value;
 
	aprint_normal_dev(sc->sc.sc_dev,
	    "applying AMD SB600/SB700 USB freeze workaround\n");
	value = pci_conf_read(sc->sc_pc, sc->sc_tag, EHCI_SBx00_WORKAROUND_REG);
	pci_conf_write(sc->sc_pc, sc->sc_tag, EHCI_SBx00_WORKAROUND_REG,
	    value | EHCI_SBx00_WORKAROUND_ENABLE);

	return 0;
}

enum ehci_pci_quirk_flags
ehci_pci_lookup_quirkdata(pci_vendor_id_t vendor, pci_product_id_t product)
{
	int i;

	for (i = 0; i < __arraycount(ehci_pci_quirks); i++) {
		if (vendor == ehci_pci_quirks[i].vendor &&
		    product == ehci_pci_quirks[i].product)
			return ehci_pci_quirks[i].quirks;
	}
	return 0;
}

