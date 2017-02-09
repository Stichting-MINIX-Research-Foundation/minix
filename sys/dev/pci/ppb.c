/*	$NetBSD: ppb.c,v 1.54 2014/09/24 10:57:03 msaitoh Exp $	*/

/*
 * Copyright (c) 1996, 1998 Christopher G. Demetriou.  All rights reserved.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: ppb.c,v 1.54 2014/09/24 10:57:03 msaitoh Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/ppbreg.h>
#include <dev/pci/pcidevs.h>

#define	PCIE_SLCSR_NOTIFY_MASK					\
	(PCIE_SLCSR_ABE | PCIE_SLCSR_PFE | PCIE_SLCSR_MSE |	\
	 PCIE_SLCSR_PDE | PCIE_SLCSR_CCE | PCIE_SLCSR_HPE)

struct ppb_softc {
	device_t sc_dev;		/* generic device glue */
	pci_chipset_tag_t sc_pc;	/* our PCI chipset... */
	pcitag_t sc_tag;		/* ...and tag. */

	pcireg_t sc_pciconfext[48];
};

static const char pcie_linkspeed_strings[4][5] = {
	"1.25", "2.5", "5.0", "8.0",
};

static bool		ppb_resume(device_t, const pmf_qual_t *);
static bool		ppb_suspend(device_t, const pmf_qual_t *);

static int
ppbmatch(device_t parent, cfdata_t match, void *aux)
{
	struct pci_attach_args *pa = aux;

	/*
	 * Check the ID register to see that it's a PCI bridge.
	 * If it is, we assume that we can deal with it; it _should_
	 * work in a standardized way...
	 */
	if (PCI_CLASS(pa->pa_class) == PCI_CLASS_BRIDGE &&
	    PCI_SUBCLASS(pa->pa_class) == PCI_SUBCLASS_BRIDGE_PCI)
		return 1;

#ifdef __powerpc__
	if (PCI_CLASS(pa->pa_class) == PCI_CLASS_PROCESSOR &&
	    PCI_SUBCLASS(pa->pa_class) == PCI_SUBCLASS_PROCESSOR_POWERPC) {
		pcireg_t bhlc = pci_conf_read(pa->pa_pc, pa->pa_tag,
		    PCI_BHLC_REG);
		if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_FREESCALE
		    && PCI_HDRTYPE(bhlc) == PCI_HDRTYPE_RC)
		return 1;
	}
#endif

#ifdef _MIPS_PADDR_T_64BIT
	/* The LDT HB acts just like a PPB.  */
	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_SIBYTE
	    && PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_SIBYTE_BCM1250_LDTHB)
		return 1;
#endif

	return 0;
}

static void
ppb_fix_pcie(device_t self)
{
	struct ppb_softc *sc = device_private(self);
	pcireg_t reg;
	int off;

	if (!pci_get_capability(sc->sc_pc, sc->sc_tag, PCI_CAP_PCIEXPRESS,
				&off, &reg))
		return; /* Not a PCIe device */

	aprint_normal_dev(self, "PCI Express capability version ");
	switch (reg & PCIE_XCAP_VER_MASK) {
	case PCIE_XCAP_VER_1:
		aprint_normal("1");
		break;
	case PCIE_XCAP_VER_2:
		aprint_normal("2");
		break;
	default:
		aprint_normal_dev(self,
		    "unsupported (0x%" PRIxMAX ")\n",
		    __SHIFTOUT(reg, PCIE_XCAP_VER_MASK));
		return;
	}
	aprint_normal(" <");
	switch (reg & PCIE_XCAP_TYPE_MASK) {
	case PCIE_XCAP_TYPE_PCIE_DEV:
		aprint_normal("PCI-E Endpoint device");
		break;
	case PCIE_XCAP_TYPE_PCI_DEV:
		aprint_normal("Legacy PCI-E Endpoint device");
		break;
	case PCIE_XCAP_TYPE_ROOT:
		aprint_normal("Root Port of PCI-E Root Complex");
		break;
	case PCIE_XCAP_TYPE_UP:
		aprint_normal("Upstream Port of PCI-E Switch");
		break;
	case PCIE_XCAP_TYPE_DOWN:
		aprint_normal("Downstream Port of PCI-E Switch");
		break;
	case PCIE_XCAP_TYPE_PCIE2PCI:
		aprint_normal("PCI-E to PCI/PCI-X Bridge");
		break;
	case PCIE_XCAP_TYPE_PCI2PCIE:
		aprint_normal("PCI/PCI-X to PCI-E Bridge");
		break;
	default:
		aprint_normal("Device/Port Type 0x%" PRIxMAX,
		    __SHIFTOUT(reg, PCIE_XCAP_TYPE_MASK));
		break;
	}

	switch (reg & PCIE_XCAP_TYPE_MASK) {
	case PCIE_XCAP_TYPE_ROOT:
	case PCIE_XCAP_TYPE_DOWN:
	case PCIE_XCAP_TYPE_PCI2PCIE:
		reg = pci_conf_read(sc->sc_pc, sc->sc_tag, off + PCIE_LCAP);
		u_int mlw = __SHIFTOUT(reg, PCIE_LCAP_MAX_WIDTH);
		u_int mls = __SHIFTOUT(reg, PCIE_LCAP_MAX_SPEED);

		if (mls < __arraycount(pcie_linkspeed_strings)) {
			aprint_normal("> x%d @ %sGT/s\n",
			    mlw, pcie_linkspeed_strings[mls]);
		} else {
			aprint_normal("> x%d @ %d.%dGT/s\n",
			    mlw, (mls * 25) / 10, (mls * 25) % 10);
		}

		reg = pci_conf_read(sc->sc_pc, sc->sc_tag, off + PCIE_LCSR);
		if (reg & PCIE_LCSR_DLACTIVE) {	/* DLLA */
			u_int lw = __SHIFTOUT(reg, PCIE_LCSR_NLW);
			u_int ls = __SHIFTOUT(reg, PCIE_LCSR_LINKSPEED);

			if (lw != mlw || ls != mls) {
				if (ls < __arraycount(pcie_linkspeed_strings)) {
					aprint_normal_dev(self,
					    "link is x%d @ %sGT/s\n",
					    lw, pcie_linkspeed_strings[ls]);
				} else {
					aprint_normal_dev(self,
					    "link is x%d @ %d.%dGT/s\n",
					    lw, (ls * 25) / 10, (ls * 25) % 10);
				}
			}
		}
		break;
	default:
		aprint_normal(">\n");
		break;
	}

	reg = pci_conf_read(sc->sc_pc, sc->sc_tag, off + PCIE_SLCSR);
	if (reg & PCIE_SLCSR_NOTIFY_MASK) {
		aprint_debug_dev(self, "disabling notification events\n");
		reg &= ~PCIE_SLCSR_NOTIFY_MASK;
		pci_conf_write(sc->sc_pc, sc->sc_tag,
		    off + PCIE_SLCSR, reg);
	}
}

static void
ppbattach(device_t parent, device_t self, void *aux)
{
	struct ppb_softc *sc = device_private(self);
	struct pci_attach_args *pa = aux;
	pci_chipset_tag_t pc = pa->pa_pc;
	struct pcibus_attach_args pba;
	pcireg_t busdata;

	pci_aprint_devinfo(pa, NULL);

	sc->sc_pc = pc;
	sc->sc_tag = pa->pa_tag;
	sc->sc_dev = self;

	busdata = pci_conf_read(pc, pa->pa_tag, PPB_REG_BUSINFO);

	if (PPB_BUSINFO_SECONDARY(busdata) == 0) {
		aprint_normal_dev(self, "not configured by system firmware\n");
		return;
	}

	ppb_fix_pcie(self);

#if 0
	/*
	 * XXX can't do this, because we're not given our bus number
	 * (we shouldn't need it), and because we've no way to
	 * decompose our tag.
	 */
	/* sanity check. */
	if (pa->pa_bus != PPB_BUSINFO_PRIMARY(busdata))
		panic("ppbattach: bus in tag (%d) != bus in reg (%d)",
		    pa->pa_bus, PPB_BUSINFO_PRIMARY(busdata));
#endif

	if (!pmf_device_register(self, ppb_suspend, ppb_resume))
		aprint_error_dev(self, "couldn't establish power handler\n");

	/*
	 * Attach the PCI bus than hangs off of it.
	 *
	 * XXX Don't pass-through Memory Read Multiple.  Should we?
	 * XXX Consult the spec...
	 */
	pba.pba_iot = pa->pa_iot;
	pba.pba_memt = pa->pa_memt;
	pba.pba_dmat = pa->pa_dmat;
	pba.pba_dmat64 = pa->pa_dmat64;
	pba.pba_pc = pc;
	pba.pba_flags = pa->pa_flags & ~PCI_FLAGS_MRM_OKAY;
	pba.pba_bus = PPB_BUSINFO_SECONDARY(busdata);
	pba.pba_sub = PPB_BUSINFO_SUBORDINATE(busdata);
	pba.pba_bridgetag = &sc->sc_tag;
	pba.pba_intrswiz = pa->pa_intrswiz;
	pba.pba_intrtag = pa->pa_intrtag;

	config_found_ia(self, "pcibus", &pba, pcibusprint);
}

static int
ppbdetach(device_t self, int flags)
{
	int rc;

	if ((rc = config_detach_children(self, flags)) != 0)
		return rc;
	pmf_device_deregister(self);
	return 0;
}

static bool
ppb_resume(device_t dv, const pmf_qual_t *qual)
{
	struct ppb_softc *sc = device_private(dv);
	int off;
	pcireg_t val;

        for (off = 0x40; off <= 0xff; off += 4) {
		val = pci_conf_read(sc->sc_pc, sc->sc_tag, off);
		if (val != sc->sc_pciconfext[(off - 0x40) / 4])
			pci_conf_write(sc->sc_pc, sc->sc_tag, off,
			    sc->sc_pciconfext[(off - 0x40)/4]);
	}

	ppb_fix_pcie(dv);

	return true;
}

static bool
ppb_suspend(device_t dv, const pmf_qual_t *qual)
{
	struct ppb_softc *sc = device_private(dv);
	int off;

	for (off = 0x40; off <= 0xff; off += 4)
		sc->sc_pciconfext[(off - 0x40) / 4] =
		    pci_conf_read(sc->sc_pc, sc->sc_tag, off);

	return true;
}

static void
ppbchilddet(device_t self, device_t child)
{
	/* we keep no references to child devices, so do nothing */
}

CFATTACH_DECL3_NEW(ppb, sizeof(struct ppb_softc),
    ppbmatch, ppbattach, ppbdetach, NULL, NULL, ppbchilddet,
    DVF_DETACH_SHUTDOWN);
