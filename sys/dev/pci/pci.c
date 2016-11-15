/*	$NetBSD: pci.c,v 1.149 2015/10/02 05:22:53 msaitoh Exp $	*/

/*
 * Copyright (c) 1995, 1996, 1997, 1998
 *     Christopher G. Demetriou.  All rights reserved.
 * Copyright (c) 1994 Charles M. Hannum.  All rights reserved.
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
 *	This product includes software developed by Charles M. Hannum.
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

/*
 * PCI bus autoconfiguration.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: pci.c,v 1.149 2015/10/02 05:22:53 msaitoh Exp $");

#ifdef _KERNEL_OPT
#include "opt_pci.h"
#endif

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/module.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <net/if.h>

#include "locators.h"

static bool pci_child_register(device_t);

#ifdef PCI_CONFIG_DUMP
int pci_config_dump = 1;
#else
int pci_config_dump = 0;
#endif

int	pciprint(void *, const char *);

#ifdef PCI_MACHDEP_ENUMERATE_BUS
#define pci_enumerate_bus PCI_MACHDEP_ENUMERATE_BUS
#endif

/*
 * Important note about PCI-ISA bridges:
 *
 * Callbacks are used to configure these devices so that ISA/EISA bridges
 * can attach their child busses after PCI configuration is done.
 *
 * This works because:
 *	(1) there can be at most one ISA/EISA bridge per PCI bus, and
 *	(2) any ISA/EISA bridges must be attached to primary PCI
 *	    busses (i.e. bus zero).
 *
 * That boils down to: there can only be one of these outstanding
 * at a time, it is cleared when configuring PCI bus 0 before any
 * subdevices have been found, and it is run after all subdevices
 * of PCI bus 0 have been found.
 *
 * This is needed because there are some (legacy) PCI devices which
 * can show up as ISA/EISA devices as well (the prime example of which
 * are VGA controllers).  If you attach ISA from a PCI-ISA/EISA bridge,
 * and the bridge is seen before the video board is, the board can show
 * up as an ISA device, and that can (bogusly) complicate the PCI device's
 * attach code, or make the PCI device not be properly attached at all.
 *
 * We use the generic config_defer() facility to achieve this.
 */

int
pcirescan(device_t self, const char *ifattr, const int *locators)
{
	struct pci_softc *sc = device_private(self);

	KASSERT(ifattr && !strcmp(ifattr, "pci"));
	KASSERT(locators);

	pci_enumerate_bus(sc, locators, NULL, NULL);

	return 0;
}

int
pcimatch(device_t parent, cfdata_t cf, void *aux)
{
	struct pcibus_attach_args *pba = aux;

	/* Check the locators */
	if (cf->cf_loc[PCIBUSCF_BUS] != PCIBUSCF_BUS_DEFAULT &&
	    cf->cf_loc[PCIBUSCF_BUS] != pba->pba_bus)
		return 0;

	/* sanity */
	if (pba->pba_bus < 0 || pba->pba_bus > 255)
		return 0;

	/*
	 * XXX check other (hardware?) indicators
	 */

	return 1;
}

void
pciattach(device_t parent, device_t self, void *aux)
{
	struct pcibus_attach_args *pba = aux;
	struct pci_softc *sc = device_private(self);
	int io_enabled, mem_enabled, mrl_enabled, mrm_enabled, mwi_enabled;
	const char *sep = "";
	static const int wildcard[PCICF_NLOCS] = {
		PCICF_DEV_DEFAULT, PCICF_FUNCTION_DEFAULT
	};

	sc->sc_dev = self;

	pci_attach_hook(parent, self, pba);

	aprint_naive("\n");
	aprint_normal("\n");

	io_enabled = (pba->pba_flags & PCI_FLAGS_IO_OKAY);
	mem_enabled = (pba->pba_flags & PCI_FLAGS_MEM_OKAY);
	mrl_enabled = (pba->pba_flags & PCI_FLAGS_MRL_OKAY);
	mrm_enabled = (pba->pba_flags & PCI_FLAGS_MRM_OKAY);
	mwi_enabled = (pba->pba_flags & PCI_FLAGS_MWI_OKAY);

	if (io_enabled == 0 && mem_enabled == 0) {
		aprint_error_dev(self, "no spaces enabled!\n");
		goto fail;
	}

#define	PRINT(str)							\
do {									\
	aprint_verbose("%s%s", sep, str);				\
	sep = ", ";							\
} while (/*CONSTCOND*/0)

	aprint_verbose_dev(self, "");

	if (io_enabled)
		PRINT("i/o space");
	if (mem_enabled)
		PRINT("memory space");
	aprint_verbose(" enabled");

	if (mrl_enabled || mrm_enabled || mwi_enabled) {
		if (mrl_enabled)
			PRINT("rd/line");
		if (mrm_enabled)
			PRINT("rd/mult");
		if (mwi_enabled)
			PRINT("wr/inv");
		aprint_verbose(" ok");
	}

	aprint_verbose("\n");

#undef PRINT

	sc->sc_iot = pba->pba_iot;
	sc->sc_memt = pba->pba_memt;
	sc->sc_dmat = pba->pba_dmat;
	sc->sc_dmat64 = pba->pba_dmat64;
	sc->sc_pc = pba->pba_pc;
	sc->sc_bus = pba->pba_bus;
	sc->sc_bridgetag = pba->pba_bridgetag;
	sc->sc_maxndevs = pci_bus_maxdevs(pba->pba_pc, pba->pba_bus);
	sc->sc_intrswiz = pba->pba_intrswiz;
	sc->sc_intrtag = pba->pba_intrtag;
	sc->sc_flags = pba->pba_flags;

	device_pmf_driver_set_child_register(sc->sc_dev, pci_child_register);

	pcirescan(sc->sc_dev, "pci", wildcard);

fail:
	if (!pmf_device_register(self, NULL, NULL))
		aprint_error_dev(self, "couldn't establish power handler\n");
}

int
pcidetach(device_t self, int flags)
{
	int rc;

	if ((rc = config_detach_children(self, flags)) != 0)
		return rc;
	pmf_device_deregister(self);
	return 0;
}

int
pciprint(void *aux, const char *pnp)
{
	struct pci_attach_args *pa = aux;
	char devinfo[256];
	const struct pci_quirkdata *qd;

	if (pnp) {
		pci_devinfo(pa->pa_id, pa->pa_class, 1, devinfo, sizeof(devinfo));
		aprint_normal("%s at %s", devinfo, pnp);
	}
	aprint_normal(" dev %d function %d", pa->pa_device, pa->pa_function);
	if (pci_config_dump) {
		printf(": ");
		pci_conf_print(pa->pa_pc, pa->pa_tag, NULL);
		if (!pnp)
			pci_devinfo(pa->pa_id, pa->pa_class, 1, devinfo, sizeof(devinfo));
		printf("%s at %s", devinfo, pnp ? pnp : "?");
		printf(" dev %d function %d (", pa->pa_device, pa->pa_function);
#ifdef __i386__
		printf("tag %#lx, intrtag %#lx, intrswiz %#lx, intrpin %#lx",
		    *(long *)&pa->pa_tag, *(long *)&pa->pa_intrtag,
		    (long)pa->pa_intrswiz, (long)pa->pa_intrpin);
#else
		printf("intrswiz %#lx, intrpin %#lx",
		    (long)pa->pa_intrswiz, (long)pa->pa_intrpin);
#endif
		printf(", i/o %s, mem %s,",
		    pa->pa_flags & PCI_FLAGS_IO_OKAY ? "on" : "off",
		    pa->pa_flags & PCI_FLAGS_MEM_OKAY ? "on" : "off");
		qd = pci_lookup_quirkdata(PCI_VENDOR(pa->pa_id),
		    PCI_PRODUCT(pa->pa_id));
		if (qd == NULL) {
			printf(" no quirks");
		} else {
			snprintb(devinfo, sizeof (devinfo),
			    "\002\001multifn\002singlefn\003skipfunc0"
			    "\004skipfunc1\005skipfunc2\006skipfunc3"
			    "\007skipfunc4\010skipfunc5\011skipfunc6"
			    "\012skipfunc7", qd->quirks);
			printf(" quirks %s", devinfo);
		}
		printf(")");
	}
	return UNCONF;
}

int
pci_probe_device(struct pci_softc *sc, pcitag_t tag,
    int (*match)(const struct pci_attach_args *),
    struct pci_attach_args *pap)
{
	pci_chipset_tag_t pc = sc->sc_pc;
	struct pci_attach_args pa;
	pcireg_t id, /* csr, */ pciclass, intr, bhlcr, bar, endbar;
#ifdef __HAVE_PCI_MSI_MSIX
	pcireg_t cap;
	int off;
#endif
	int ret, pin, bus, device, function, i, width;
	int locs[PCICF_NLOCS];

	pci_decompose_tag(pc, tag, &bus, &device, &function);

	/* a driver already attached? */
	if (sc->PCI_SC_DEVICESC(device, function).c_dev != NULL && !match)
		return 0;

	bhlcr = pci_conf_read(pc, tag, PCI_BHLC_REG);
	if (PCI_HDRTYPE_TYPE(bhlcr) > 2)
		return 0;

	id = pci_conf_read(pc, tag, PCI_ID_REG);
	/* csr = pci_conf_read(pc, tag, PCI_COMMAND_STATUS_REG); */
	pciclass = pci_conf_read(pc, tag, PCI_CLASS_REG);

	/* Invalid vendor ID value? */
	if (PCI_VENDOR(id) == PCI_VENDOR_INVALID)
		return 0;
	/* XXX Not invalid, but we've done this ~forever. */
	if (PCI_VENDOR(id) == 0)
		return 0;

	/* Collect memory range info */
	memset(sc->PCI_SC_DEVICESC(device, function).c_range, 0,
	    sizeof(sc->PCI_SC_DEVICESC(device, function).c_range));
	i = 0;
	switch (PCI_HDRTYPE_TYPE(bhlcr)) {
	case PCI_HDRTYPE_PPB:
		endbar = PCI_MAPREG_PPB_END;
		break;
	case PCI_HDRTYPE_PCB:
		endbar = PCI_MAPREG_PCB_END;
		break;
	default:
		endbar = PCI_MAPREG_END;
		break;
	}
	for (bar = PCI_MAPREG_START; bar < endbar; bar += width) {
		struct pci_range *r;
		pcireg_t type;

		width = 4;
		if (pci_mapreg_probe(pc, tag, bar, &type) == 0)
			continue;

		if (PCI_MAPREG_TYPE(type) == PCI_MAPREG_TYPE_MEM) {
			if (PCI_MAPREG_MEM_TYPE(type) ==
			    PCI_MAPREG_MEM_TYPE_64BIT)
				width = 8;

			r = &sc->PCI_SC_DEVICESC(device, function).c_range[i++];
			if (pci_mapreg_info(pc, tag, bar, type,
			    &r->r_offset, &r->r_size, &r->r_flags) != 0)
				break;
			if ((PCI_VENDOR(id) == PCI_VENDOR_ATI) && (bar == 0x10)
			    && (r->r_size == 0x1000000)) {
				struct pci_range *nr;
				/*
				 * this has to be a mach64
				 * split things up so each half-aperture can
				 * be mapped PREFETCHABLE except the last page
				 * which may contain registers
				 */
				r->r_size = 0x7ff000;
				r->r_flags = BUS_SPACE_MAP_LINEAR |
					     BUS_SPACE_MAP_PREFETCHABLE;
				nr = &sc->PCI_SC_DEVICESC(device,
				    function).c_range[i++];
				nr->r_offset = r->r_offset + 0x800000;
				nr->r_size = 0x7ff000;
				nr->r_flags = BUS_SPACE_MAP_LINEAR |
					      BUS_SPACE_MAP_PREFETCHABLE;
			}
			
		}
	}

	pa.pa_iot = sc->sc_iot;
	pa.pa_memt = sc->sc_memt;
	pa.pa_dmat = sc->sc_dmat;
	pa.pa_dmat64 = sc->sc_dmat64;
	pa.pa_pc = pc;
	pa.pa_bus = bus;
	pa.pa_device = device;
	pa.pa_function = function;
	pa.pa_tag = tag;
	pa.pa_id = id;
	pa.pa_class = pciclass;

	/*
	 * Set up memory, I/O enable, and PCI command flags
	 * as appropriate.
	 */
	pa.pa_flags = sc->sc_flags;

	/*
	 * If the cache line size is not configured, then
	 * clear the MRL/MRM/MWI command-ok flags.
	 */
	if (PCI_CACHELINE(bhlcr) == 0) {
		pa.pa_flags &= ~(PCI_FLAGS_MRL_OKAY|
		    PCI_FLAGS_MRM_OKAY|PCI_FLAGS_MWI_OKAY);
	}

	if (sc->sc_bridgetag == NULL) {
		pa.pa_intrswiz = 0;
		pa.pa_intrtag = tag;
	} else {
		pa.pa_intrswiz = sc->sc_intrswiz + device;
		pa.pa_intrtag = sc->sc_intrtag;
	}

	intr = pci_conf_read(pc, tag, PCI_INTERRUPT_REG);

	pin = PCI_INTERRUPT_PIN(intr);
	pa.pa_rawintrpin = pin;
	if (pin == PCI_INTERRUPT_PIN_NONE) {
		/* no interrupt */
		pa.pa_intrpin = 0;
	} else {
		/*
		 * swizzle it based on the number of busses we're
		 * behind and our device number.
		 */
		pa.pa_intrpin = 	/* XXX */
		    ((pin + pa.pa_intrswiz - 1) % 4) + 1;
	}
	pa.pa_intrline = PCI_INTERRUPT_LINE(intr);

#ifdef __HAVE_PCI_MSI_MSIX
	if (pci_get_ht_capability(pc, tag, PCI_HT_CAP_MSIMAP, &off, &cap)) {
		/*
		 * XXX Should we enable MSI mapping ourselves on
		 * systems that have it disabled?
		 */
		if (cap & PCI_HT_MSI_ENABLED) {
			uint64_t addr;
			if ((cap & PCI_HT_MSI_FIXED) == 0) {
				addr = pci_conf_read(pc, tag,
				    off + PCI_HT_MSI_ADDR_LO);
				addr |= (uint64_t)pci_conf_read(pc, tag,
				    off + PCI_HT_MSI_ADDR_HI) << 32;
			} else
				addr = PCI_HT_MSI_FIXED_ADDR;

			/*
			 * XXX This will fail to enable MSI on systems
			 * that don't use the canonical address.
			 */
			if (addr == PCI_HT_MSI_FIXED_ADDR) {
				pa.pa_flags |= PCI_FLAGS_MSI_OKAY;
				pa.pa_flags |= PCI_FLAGS_MSIX_OKAY;
			}
		}
	}
#endif

	if (match != NULL) {
		ret = (*match)(&pa);
		if (ret != 0 && pap != NULL)
			*pap = pa;
	} else {
		struct pci_child *c;
		locs[PCICF_DEV] = device;
		locs[PCICF_FUNCTION] = function;

		c = &sc->PCI_SC_DEVICESC(device, function);
		pci_conf_capture(pc, tag, &c->c_conf);
		if (pci_get_powerstate(pc, tag, &c->c_powerstate) == 0)
			c->c_psok = true;
		else
			c->c_psok = false;

		c->c_dev = config_found_sm_loc(sc->sc_dev, "pci", locs, &pa,
					     pciprint, config_stdsubmatch);

		ret = (c->c_dev != NULL);
	}

	return ret;
}

void
pcidevdetached(device_t self, device_t child)
{
	struct pci_softc *sc = device_private(self);
	int d, f;
	pcitag_t tag;
	struct pci_child *c;

	d = device_locator(child, PCICF_DEV);
	f = device_locator(child, PCICF_FUNCTION);

	c = &sc->PCI_SC_DEVICESC(d, f);

	KASSERT(c->c_dev == child);

	tag = pci_make_tag(sc->sc_pc, sc->sc_bus, d, f);
	if (c->c_psok)
		pci_set_powerstate(sc->sc_pc, tag, c->c_powerstate);
	pci_conf_restore(sc->sc_pc, tag, &c->c_conf);
	c->c_dev = NULL;
}

CFATTACH_DECL3_NEW(pci, sizeof(struct pci_softc),
    pcimatch, pciattach, pcidetach, NULL, pcirescan, pcidevdetached,
    DVF_DETACH_SHUTDOWN);

int
pci_get_capability(pci_chipset_tag_t pc, pcitag_t tag, int capid,
    int *offset, pcireg_t *value)
{
	pcireg_t reg;
	unsigned int ofs;

	reg = pci_conf_read(pc, tag, PCI_COMMAND_STATUS_REG);
	if (!(reg & PCI_STATUS_CAPLIST_SUPPORT))
		return 0;

	/* Determine the Capability List Pointer register to start with. */
	reg = pci_conf_read(pc, tag, PCI_BHLC_REG);
	switch (PCI_HDRTYPE_TYPE(reg)) {
	case 0:	/* standard device header */
	case 1: /* PCI-PCI bridge header */
		ofs = PCI_CAPLISTPTR_REG;
		break;
	case 2:	/* PCI-CardBus Bridge header */
		ofs = PCI_CARDBUS_CAPLISTPTR_REG;
		break;
	default:
		return 0;
	}

	ofs = PCI_CAPLIST_PTR(pci_conf_read(pc, tag, ofs));
	while (ofs != 0) {
		if ((ofs & 3) || (ofs < 0x40)) {
			int bus, device, function;

			pci_decompose_tag(pc, tag, &bus, &device, &function);

			printf("Skipping broken PCI header on %d:%d:%d\n",
			    bus, device, function);
			break;
		}
		reg = pci_conf_read(pc, tag, ofs);
		if (PCI_CAPLIST_CAP(reg) == capid) {
			if (offset)
				*offset = ofs;
			if (value)
				*value = reg;
			return 1;
		}
		ofs = PCI_CAPLIST_NEXT(reg);
	}

	return 0;
}

int
pci_get_ht_capability(pci_chipset_tag_t pc, pcitag_t tag, int capid,
    int *offset, pcireg_t *value)
{
	pcireg_t reg;
	unsigned int ofs;

	if (pci_get_capability(pc, tag, PCI_CAP_LDT, &ofs, NULL) == 0)
		return 0;

	while (ofs != 0) {
#ifdef DIAGNOSTIC
		if ((ofs & 3) || (ofs < 0x40))
			panic("pci_get_ht_capability");
#endif
		reg = pci_conf_read(pc, tag, ofs);
		if (PCI_HT_CAP(reg) == capid) {
			if (offset)
				*offset = ofs;
			if (value)
				*value = reg;
			return 1;
		}
		ofs = PCI_CAPLIST_NEXT(reg);
	}

	return 0;
}

/*
 * return number of the devices's MSI vectors
 * return 0 if the device does not support MSI
 */
int
pci_msi_count(pci_chipset_tag_t pc, pcitag_t tag)
{
	pcireg_t reg;
	uint32_t mmc;
	int count, offset;

	if (pci_get_capability(pc, tag, PCI_CAP_MSI, &offset, NULL) == 0)
		return 0;

	reg = pci_conf_read(pc, tag, offset + PCI_MSI_CTL);
	mmc = PCI_MSI_CTL_MMC(reg);
	count = 1 << mmc;
	if (count > PCI_MSI_MAX_VECTORS) {
		aprint_error("detect an illegal device! The device use reserved MMC values.\n");
		return 0;
	}

	return count;
}

/*
 * return number of the devices's MSI-X vectors
 * return 0 if the device does not support MSI-X
 */
int
pci_msix_count(pci_chipset_tag_t pc, pcitag_t tag)
{
	pcireg_t reg;
	int offset;

	if (pci_get_capability(pc, tag, PCI_CAP_MSIX, &offset, NULL) == 0)
		return 0;

	reg = pci_conf_read(pc, tag, offset + PCI_MSIX_CTL);

	return PCI_MSIX_CTL_TBLSIZE(reg);
}

int
pci_get_ext_capability(pci_chipset_tag_t pc, pcitag_t tag, int capid,
    int *offset, pcireg_t *value)
{
	pcireg_t reg;
	unsigned int ofs;

	/* Only supported for PCI-express devices */
	if (!pci_get_capability(pc, tag, PCI_CAP_PCIEXPRESS, NULL, NULL))
		return 0;

	ofs = PCI_EXTCAPLIST_BASE;
	reg = pci_conf_read(pc, tag, ofs);
	if (reg == 0xffffffff || reg == 0)
		return 0;

	for (;;) {
#ifdef DIAGNOSTIC
		if ((ofs & 3) || ofs < PCI_EXTCAPLIST_BASE)
			panic("%s: invalid offset %u", __func__, ofs);
#endif
		if (PCI_EXTCAPLIST_CAP(reg) == capid) {
			if (offset != NULL)
				*offset = ofs;
			if (value != NULL)
				*value = reg;
			return 1;
		}
		ofs = PCI_EXTCAPLIST_NEXT(reg);
		if (ofs == 0)
			break;
		reg = pci_conf_read(pc, tag, ofs);
	}

	return 0;
}

int
pci_find_device(struct pci_attach_args *pa,
		int (*match)(const struct pci_attach_args *))
{
	extern struct cfdriver pci_cd;
	device_t pcidev;
	int i;
	static const int wildcard[2] = {
		PCICF_DEV_DEFAULT,
		PCICF_FUNCTION_DEFAULT
	};

	for (i = 0; i < pci_cd.cd_ndevs; i++) {
		pcidev = device_lookup(&pci_cd, i);
		if (pcidev != NULL &&
		    pci_enumerate_bus(device_private(pcidev), wildcard,
		    		      match, pa) != 0)
			return 1;
	}
	return 0;
}

#ifndef PCI_MACHDEP_ENUMERATE_BUS
/*
 * Generic PCI bus enumeration routine.  Used unless machine-dependent
 * code needs to provide something else.
 */
int
pci_enumerate_bus(struct pci_softc *sc, const int *locators,
    int (*match)(const struct pci_attach_args *), struct pci_attach_args *pap)
{
	pci_chipset_tag_t pc = sc->sc_pc;
	int device, function, nfunctions, ret;
	const struct pci_quirkdata *qd;
	pcireg_t id, bhlcr;
	pcitag_t tag;
	uint8_t devs[32];
	int i, n;

	n = pci_bus_devorder(sc->sc_pc, sc->sc_bus, devs, __arraycount(devs));
	for (i = 0; i < n; i++) {
		device = devs[i];

		if ((locators[PCICF_DEV] != PCICF_DEV_DEFAULT) &&
		    (locators[PCICF_DEV] != device))
			continue;

		tag = pci_make_tag(pc, sc->sc_bus, device, 0);

		bhlcr = pci_conf_read(pc, tag, PCI_BHLC_REG);
		if (PCI_HDRTYPE_TYPE(bhlcr) > 2)
			continue;

		id = pci_conf_read(pc, tag, PCI_ID_REG);

		/* Invalid vendor ID value? */
		if (PCI_VENDOR(id) == PCI_VENDOR_INVALID)
			continue;
		/* XXX Not invalid, but we've done this ~forever. */
		if (PCI_VENDOR(id) == 0)
			continue;

		qd = pci_lookup_quirkdata(PCI_VENDOR(id), PCI_PRODUCT(id));

		if (qd != NULL &&
		      (qd->quirks & PCI_QUIRK_MULTIFUNCTION) != 0)
			nfunctions = 8;
		else if (qd != NULL &&
		      (qd->quirks & PCI_QUIRK_MONOFUNCTION) != 0)
			nfunctions = 1;
		else
			nfunctions = PCI_HDRTYPE_MULTIFN(bhlcr) ? 8 : 1;

#ifdef __PCI_DEV_FUNCORDER
		char funcs[8];
		int j;
		for (j = 0; j < nfunctions; j++) {
			funcs[j] = j;
		}
		if (j < __arraycount(funcs))
			funcs[j] = -1;
		if (nfunctions > 1) {
			pci_dev_funcorder(sc->sc_pc, sc->sc_bus, device,
			    nfunctions, funcs);
		}
		for (j = 0;
		     j < 8 && (function = funcs[j]) < 8 && function >= 0;
		     j++) {
#else
		for (function = 0; function < nfunctions; function++) {
#endif
			if ((locators[PCICF_FUNCTION] != PCICF_FUNCTION_DEFAULT)
			    && (locators[PCICF_FUNCTION] != function))
				continue;

			if (qd != NULL &&
			    (qd->quirks & PCI_QUIRK_SKIP_FUNC(function)) != 0)
				continue;
			tag = pci_make_tag(pc, sc->sc_bus, device, function);
			ret = pci_probe_device(sc, tag, match, pap);
			if (match != NULL && ret != 0)
				return ret;
		}
	}
	return 0;
}
#endif /* PCI_MACHDEP_ENUMERATE_BUS */


/*
 * Vital Product Data (PCI 2.2)
 */

int
pci_vpd_read(pci_chipset_tag_t pc, pcitag_t tag, int offset, int count,
    pcireg_t *data)
{
	uint32_t reg;
	int ofs, i, j;

	KASSERT(data != NULL);
	KASSERT((offset + count) < 0x7fff);

	if (pci_get_capability(pc, tag, PCI_CAP_VPD, &ofs, &reg) == 0)
		return 1;

	for (i = 0; i < count; offset += sizeof(*data), i++) {
		reg &= 0x0000ffff;
		reg &= ~PCI_VPD_OPFLAG;
		reg |= PCI_VPD_ADDRESS(offset);
		pci_conf_write(pc, tag, ofs, reg);

		/*
		 * PCI 2.2 does not specify how long we should poll
		 * for completion nor whether the operation can fail.
		 */
		j = 0;
		do {
			if (j++ == 20)
				return 1;
			delay(4);
			reg = pci_conf_read(pc, tag, ofs);
		} while ((reg & PCI_VPD_OPFLAG) == 0);
		data[i] = pci_conf_read(pc, tag, PCI_VPD_DATAREG(ofs));
	}

	return 0;
}

int
pci_vpd_write(pci_chipset_tag_t pc, pcitag_t tag, int offset, int count,
    pcireg_t *data)
{
	pcireg_t reg;
	int ofs, i, j;

	KASSERT(data != NULL);
	KASSERT((offset + count) < 0x7fff);

	if (pci_get_capability(pc, tag, PCI_CAP_VPD, &ofs, &reg) == 0)
		return 1;

	for (i = 0; i < count; offset += sizeof(*data), i++) {
		pci_conf_write(pc, tag, PCI_VPD_DATAREG(ofs), data[i]);

		reg &= 0x0000ffff;
		reg |= PCI_VPD_OPFLAG;
		reg |= PCI_VPD_ADDRESS(offset);
		pci_conf_write(pc, tag, ofs, reg);

		/*
		 * PCI 2.2 does not specify how long we should poll
		 * for completion nor whether the operation can fail.
		 */
		j = 0;
		do {
			if (j++ == 20)
				return 1;
			delay(1);
			reg = pci_conf_read(pc, tag, ofs);
		} while (reg & PCI_VPD_OPFLAG);
	}

	return 0;
}

int
pci_dma64_available(const struct pci_attach_args *pa)
{
#ifdef _PCI_HAVE_DMA64
	if (BUS_DMA_TAG_VALID(pa->pa_dmat64))
                        return 1;
#endif
        return 0;
}

void
pci_conf_capture(pci_chipset_tag_t pc, pcitag_t tag,
		  struct pci_conf_state *pcs)
{
	int off;

	for (off = 0; off < 16; off++)
		pcs->reg[off] = pci_conf_read(pc, tag, (off * 4));

	return;
}

void
pci_conf_restore(pci_chipset_tag_t pc, pcitag_t tag,
		  struct pci_conf_state *pcs)
{
	int off;
	pcireg_t val;

	for (off = 15; off >= 0; off--) {
		val = pci_conf_read(pc, tag, (off * 4));
		if (val != pcs->reg[off])
			pci_conf_write(pc, tag, (off * 4), pcs->reg[off]);
	}

	return;
}

/*
 * Power Management Capability (Rev 2.2)
 */
static int
pci_get_powerstate_int(pci_chipset_tag_t pc, pcitag_t tag , pcireg_t *state,
    int offset)
{
	pcireg_t value, now;

	value = pci_conf_read(pc, tag, offset + PCI_PMCSR);
	now = value & PCI_PMCSR_STATE_MASK;
	switch (now) {
	case PCI_PMCSR_STATE_D0:
	case PCI_PMCSR_STATE_D1:
	case PCI_PMCSR_STATE_D2:
	case PCI_PMCSR_STATE_D3:
		*state = now;
		return 0;
	default:
		return EINVAL;
	}
}

int
pci_get_powerstate(pci_chipset_tag_t pc, pcitag_t tag , pcireg_t *state)
{
	int offset;
	pcireg_t value;

	if (!pci_get_capability(pc, tag, PCI_CAP_PWRMGMT, &offset, &value))
		return EOPNOTSUPP;

	return pci_get_powerstate_int(pc, tag, state, offset);
}

static int
pci_set_powerstate_int(pci_chipset_tag_t pc, pcitag_t tag, pcireg_t state,
    int offset, pcireg_t cap_reg)
{
	pcireg_t value, cap, now;

	cap = cap_reg >> PCI_PMCR_SHIFT;
	value = pci_conf_read(pc, tag, offset + PCI_PMCSR);
	now = value & PCI_PMCSR_STATE_MASK;
	value &= ~PCI_PMCSR_STATE_MASK;

	if (now == state)
		return 0;
	switch (state) {
	case PCI_PMCSR_STATE_D0:
		break;
	case PCI_PMCSR_STATE_D1:
		if (now == PCI_PMCSR_STATE_D2 || now == PCI_PMCSR_STATE_D3) {
			printf("invalid transition from %d to D1\n", (int)now);
			return EINVAL;
		}
		if (!(cap & PCI_PMCR_D1SUPP)) {
			printf("D1 not supported\n");
			return EOPNOTSUPP;
		}
		break;
	case PCI_PMCSR_STATE_D2:
		if (now == PCI_PMCSR_STATE_D3) {
			printf("invalid transition from %d to D2\n", (int)now);
			return EINVAL;
		}
		if (!(cap & PCI_PMCR_D2SUPP)) {
			printf("D2 not supported\n");
			return EOPNOTSUPP;
		}
		break;
	case PCI_PMCSR_STATE_D3:
		break;
	default:
		return EINVAL;
	}
	value |= state;
	pci_conf_write(pc, tag, offset + PCI_PMCSR, value);
	/* delay according to pcipm1.2, ch. 5.6.1 */
	if (state == PCI_PMCSR_STATE_D3 || now == PCI_PMCSR_STATE_D3)
		DELAY(10000);
	else if (state == PCI_PMCSR_STATE_D2 || now == PCI_PMCSR_STATE_D2)
		DELAY(200);

	return 0;
}

int
pci_set_powerstate(pci_chipset_tag_t pc, pcitag_t tag, pcireg_t state)
{
	int offset;
	pcireg_t value;

	if (!pci_get_capability(pc, tag, PCI_CAP_PWRMGMT, &offset, &value)) {
		printf("pci_set_powerstate not supported\n");
		return EOPNOTSUPP;
	}

	return pci_set_powerstate_int(pc, tag, state, offset, value);
}

int
pci_activate(pci_chipset_tag_t pc, pcitag_t tag, device_t dev,
    int (*wakefun)(pci_chipset_tag_t, pcitag_t, device_t, pcireg_t))
{
	pcireg_t pmode;
	int error;

	if ((error = pci_get_powerstate(pc, tag, &pmode)))
		return error;

	switch (pmode) {
	case PCI_PMCSR_STATE_D0:
		break;
	case PCI_PMCSR_STATE_D3:
		if (wakefun == NULL) {
			/*
			 * The card has lost all configuration data in
			 * this state, so punt.
			 */
			aprint_error_dev(dev,
			    "unable to wake up from power state D3\n");
			return EOPNOTSUPP;
		}
		/*FALLTHROUGH*/
	default:
		if (wakefun) {
			error = (*wakefun)(pc, tag, dev, pmode);
			if (error)
				return error;
		}
		aprint_normal_dev(dev, "waking up from power state D%d\n",
		    pmode);
		if ((error = pci_set_powerstate(pc, tag, PCI_PMCSR_STATE_D0)))
			return error;
	}
	return 0;
}

int
pci_activate_null(pci_chipset_tag_t pc, pcitag_t tag,
    device_t dev, pcireg_t state)
{
	return 0;
}

struct pci_child_power {
	struct pci_conf_state p_pciconf;
	pci_chipset_tag_t p_pc;
	pcitag_t p_tag;
	bool p_has_pm;
	int p_pm_offset;
	pcireg_t p_pm_cap;
	pcireg_t p_class;
	pcireg_t p_csr;
};

static bool
pci_child_suspend(device_t dv, const pmf_qual_t *qual)
{
	struct pci_child_power *priv = device_pmf_bus_private(dv);
	pcireg_t ocsr, csr;

	pci_conf_capture(priv->p_pc, priv->p_tag, &priv->p_pciconf);

	if (!priv->p_has_pm)
		return true; /* ??? hopefully handled by ACPI */
	if (PCI_CLASS(priv->p_class) == PCI_CLASS_DISPLAY)
		return true; /* XXX */

	/* disable decoding and busmastering, see pcipm1.2 ch. 8.2.1 */
	ocsr = pci_conf_read(priv->p_pc, priv->p_tag, PCI_COMMAND_STATUS_REG);
	csr = ocsr & ~(PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MEM_ENABLE
		       | PCI_COMMAND_MASTER_ENABLE);
	pci_conf_write(priv->p_pc, priv->p_tag, PCI_COMMAND_STATUS_REG, csr);
	if (pci_set_powerstate_int(priv->p_pc, priv->p_tag,
	    PCI_PMCSR_STATE_D3, priv->p_pm_offset, priv->p_pm_cap)) {
		pci_conf_write(priv->p_pc, priv->p_tag,
			       PCI_COMMAND_STATUS_REG, ocsr);
		aprint_error_dev(dv, "unsupported state, continuing.\n");
		return false;
	}
	return true;
}

static bool
pci_child_resume(device_t dv, const pmf_qual_t *qual)
{
	struct pci_child_power *priv = device_pmf_bus_private(dv);

	if (priv->p_has_pm &&
	    pci_set_powerstate_int(priv->p_pc, priv->p_tag,
	    PCI_PMCSR_STATE_D0, priv->p_pm_offset, priv->p_pm_cap)) {
		aprint_error_dev(dv, "unsupported state, continuing.\n");
		return false;
	}

	pci_conf_restore(priv->p_pc, priv->p_tag, &priv->p_pciconf);

	return true;
}

static bool
pci_child_shutdown(device_t dv, int how)
{
	struct pci_child_power *priv = device_pmf_bus_private(dv);
	pcireg_t csr;

	/* restore original bus-mastering state */
	csr = pci_conf_read(priv->p_pc, priv->p_tag, PCI_COMMAND_STATUS_REG);
	csr &= ~PCI_COMMAND_MASTER_ENABLE;
	csr |= priv->p_csr & PCI_COMMAND_MASTER_ENABLE;
	pci_conf_write(priv->p_pc, priv->p_tag, PCI_COMMAND_STATUS_REG, csr);
	return true;
}

static void
pci_child_deregister(device_t dv)
{
	struct pci_child_power *priv = device_pmf_bus_private(dv);

	free(priv, M_DEVBUF);
}

static bool
pci_child_register(device_t child)
{
	device_t self = device_parent(child);
	struct pci_softc *sc = device_private(self);
	struct pci_child_power *priv;
	int device, function, off;
	pcireg_t reg;

	priv = malloc(sizeof(*priv), M_DEVBUF, M_WAITOK);

	device = device_locator(child, PCICF_DEV);
	function = device_locator(child, PCICF_FUNCTION);

	priv->p_pc = sc->sc_pc;
	priv->p_tag = pci_make_tag(priv->p_pc, sc->sc_bus, device,
	    function);
	priv->p_class = pci_conf_read(priv->p_pc, priv->p_tag, PCI_CLASS_REG);
	priv->p_csr = pci_conf_read(priv->p_pc, priv->p_tag,
	    PCI_COMMAND_STATUS_REG);

	if (pci_get_capability(priv->p_pc, priv->p_tag,
			       PCI_CAP_PWRMGMT, &off, &reg)) {
		priv->p_has_pm = true;
		priv->p_pm_offset = off;
		priv->p_pm_cap = reg;
	} else {
		priv->p_has_pm = false;
		priv->p_pm_offset = -1;
	}

	device_pmf_bus_register(child, priv, pci_child_suspend,
	    pci_child_resume, pci_child_shutdown, pci_child_deregister);

	return true;
}

MODULE(MODULE_CLASS_DRIVER, pci, NULL);

static int
pci_modcmd(modcmd_t cmd, void *priv)
{
	if (cmd == MODULE_CMD_INIT || cmd == MODULE_CMD_FINI)
		return 0;
	return ENOTTY;
}
