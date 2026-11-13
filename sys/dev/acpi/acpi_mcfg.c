/*	$NetBSD: acpi_mcfg.c,v 1.16.4.1 2019/10/15 19:30:50 martin Exp $	*/

/*-
 * Copyright (C) 2015 NONAKA Kimihiro <nonaka@NetBSD.org>
 * All rights reserved.
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

#include "opt_pci.h"

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: acpi_mcfg.c,v 1.16.4.1 2019/10/15 19:30:50 martin Exp $");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/kmem.h>
#include <sys/systm.h>
#include <sys/extent.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pciconf.h>
#include <dev/pci/pcidevs.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>
#include <dev/acpi/acpi_mcfg.h>

#include "locators.h"

#define _COMPONENT      ACPI_RESOURCE_COMPONENT
ACPI_MODULE_NAME	("acpi_mcfg")

#define	EXTCONF_OFFSET(d, f, r)	((((d) * 8 + (f)) * PCI_EXTCONF_SIZE) + (r))

#define	PCIDEV_SET_VALID(mb, d, f)	((mb)->valid_devs[(d)] |= __BIT((f)))
#define	PCIDEV_SET_INVALID(mb, d, f)	((mb)->valid_devs[(d)] &= ~__BIT((f)))
#define	PCIDEV_IS_VALID(mb, d, f)	((mb)->valid_devs[(d)] & __BIT((f)))

#define	EXTCONF_SET_VALID(mb, d, f)	((mb)->valid_extconf[(d)] |= __BIT((f)))
#define	EXTCONF_SET_INVALID(mb, d, f)	((mb)->valid_extconf[(d)] &= ~__BIT((f)))
#define	EXTCONF_IS_VALID(mb, d, f)	((mb)->valid_extconf[(d)] & __BIT((f)))

struct mcfg_segment {
	uint64_t ms_address;		/* Base address */
	int ms_segment;			/* Segment # */
	int ms_bus_start;		/* Start bus # */
	int ms_bus_end;			/* End bus # */
	bus_space_tag_t ms_bst;
	struct mcfg_bus {
		bus_space_handle_t bsh[32][8];
		uint8_t valid_devs[32];
		uint8_t valid_extconf[32];
		int valid_ndevs;
		pcitag_t last_probed;
	} *ms_bus;
};

static struct mcfg_segment *mcfg_segs;
static int mcfg_nsegs;
static ACPI_TABLE_MCFG *mcfg;
static int mcfg_inited;
static struct acpi_softc *acpi_sc;

static const struct acpimcfg_ops mcfg_default_ops = {
	.ao_validate = acpimcfg_default_validate,

	.ao_read = acpimcfg_default_read,
	.ao_write = acpimcfg_default_write,
};
static const struct acpimcfg_ops *mcfg_ops = &mcfg_default_ops;

/*
 * default operations.
 */
bool
acpimcfg_default_validate(uint64_t address, int bus_start, int *bus_end)
{

	/* Always Ok */
	return true;
}

uint32_t
acpimcfg_default_read(bus_space_tag_t bst, bus_space_handle_t bsh,
    bus_addr_t addr)
{

	return bus_space_read_4(bst, bsh, addr);
}

void
acpimcfg_default_write(bus_space_tag_t bst, bus_space_handle_t bsh,
    bus_addr_t addr, uint32_t data)
{

	bus_space_write_4(bst, bsh, addr, data);
}


/*
 * Check MCFG memory region at system resource
 */
struct acpimcfg_memrange {
	const char	*hid;
	uint64_t	address;
	int		bus_start;
	int		bus_end;
	bool		found;
};

static ACPI_STATUS
acpimcfg_parse_callback(ACPI_RESOURCE *res, void *ctx)
{
	struct acpimcfg_memrange *mr = ctx;
	const char *type;
	uint64_t size, mapaddr, mapsize;
	int n;

	switch (res->Type) {
	case ACPI_RESOURCE_TYPE_FIXED_MEMORY32:
		type = "FIXED_MEMORY32";
		mapaddr = res->Data.FixedMemory32.Address;
		mapsize = res->Data.FixedMemory32.AddressLength;
		break;

	case ACPI_RESOURCE_TYPE_ADDRESS32:
		/* XXX Only fixed size supported for now */
		if (res->Data.Address32.Address.AddressLength == 0 ||
		    res->Data.Address32.ProducerConsumer != ACPI_CONSUMER)
			goto out;

		if (res->Data.Address32.ResourceType != ACPI_MEMORY_RANGE)
			goto out;

		if (res->Data.Address32.MinAddressFixed != ACPI_ADDRESS_FIXED ||
		    res->Data.Address32.MaxAddressFixed != ACPI_ADDRESS_FIXED)
			goto out;

		type = "ADDRESS32";
		mapaddr = res->Data.Address32.Address.Minimum;
		mapsize = res->Data.Address32.Address.AddressLength;
		break;

#ifdef _LP64
	case ACPI_RESOURCE_TYPE_ADDRESS64:
		/* XXX Only fixed size supported for now */
		if (res->Data.Address64.Address.AddressLength == 0 ||
		    res->Data.Address64.ProducerConsumer != ACPI_CONSUMER)
			goto out;

		if (res->Data.Address64.ResourceType != ACPI_MEMORY_RANGE)
			goto out;

		if (res->Data.Address64.MinAddressFixed != ACPI_ADDRESS_FIXED ||
		    res->Data.Address64.MaxAddressFixed != ACPI_ADDRESS_FIXED)
			goto out;

		type = "ADDRESS64";
		mapaddr = res->Data.Address64.Address.Minimum;
		mapsize = res->Data.Address64.Address.AddressLength;
		break;
#endif

	default:
 out:
		aprint_debug_dev(acpi_sc->sc_dev, "MCFG: %s: Type=%d\n",
		    mr->hid, res->Type);
		return_ACPI_STATUS(AE_OK);
	}

	aprint_debug_dev(acpi_sc->sc_dev, "MCFG: %s: Type=%d(%s), "
	    "Address=0x%016" PRIx64 ", Length=0x%016" PRIx64 "\n",
	    mr->hid, res->Type, type, mapaddr, mapsize);

	if (mr->address < mapaddr || mr->address >= mapaddr + mapsize)
		return_ACPI_STATUS(AE_OK);

	size = (mr->bus_end - mr->bus_start + 1) * ACPIMCFG_SIZE_PER_BUS;

	/* full map */
	if (mr->address + size <= mapaddr + mapsize) {
		mr->found = true;
		return_ACPI_STATUS(AE_CTRL_TERMINATE);
	}

	/* partial map */
	n = (mapsize - (mr->address - mapaddr)) / ACPIMCFG_SIZE_PER_BUS;
	/* bus_start == bus_end is not allowed. */
	if (n > 1) {
		mr->bus_end = mr->bus_start + n - 1;
		mr->found = true;
		return_ACPI_STATUS(AE_CTRL_TERMINATE);
	}

	aprint_debug_dev(acpi_sc->sc_dev, "MCFG: bus %d-%d, "
	    "address 0x%016" PRIx64 ": invalid size: request 0x%016" PRIx64
	    ", actual 0x%016" PRIx64 "\n",
	    mr->bus_start, mr->bus_end, mr->address, size, mapsize);

	return_ACPI_STATUS(AE_OK);
}

static ACPI_STATUS
acpimcfg_check_system_resource(ACPI_HANDLE handle, UINT32 level, void *ctx,
    void **retval)
{
	struct acpimcfg_memrange *mr = ctx;
	ACPI_STATUS status;

	status = AcpiWalkResources(handle, "_CRS", acpimcfg_parse_callback, mr);
	if (ACPI_FAILURE(status))
		return_ACPI_STATUS(status);

	if (mr->found)
		return_ACPI_STATUS(AE_CTRL_TERMINATE);

	aprint_debug_dev(acpi_sc->sc_dev, "MCFG: %s: bus %d-%d, "
	    "address 0x%016" PRIx64 ": no valid region\n", mr->hid,
	    mr->bus_start, mr->bus_end, mr->address);

	return_ACPI_STATUS(AE_OK);
}

static bool
acpimcfg_find_system_resource(uint64_t address, int bus_start, int *bus_end)
{
	static const char *system_resource_hid[] = {
		"PNP0C01",	/* System Board */
		"PNP0C02"	/* General ID for reserving resources */
	};
	struct acpimcfg_memrange mr;
	ACPI_STATUS status;
	int i;

	mr.address = address;
	mr.bus_start = bus_start;
	mr.bus_end = *bus_end;
	mr.found = false;

	for (i = 0; i < __arraycount(system_resource_hid); i++) {
		mr.hid = system_resource_hid[i];
		status = AcpiGetDevices(__UNCONST(system_resource_hid[i]),
		    acpimcfg_check_system_resource, &mr, NULL);
		if (ACPI_FAILURE(status))
			continue;
		if (mr.found) {
			*bus_end = mr.bus_end;
			return true;
		}
	}
	return false;
}


/*
 * ACPI MCFG
 */
void
acpimcfg_probe(struct acpi_softc *sc)
{
	ACPI_MCFG_ALLOCATION *ama;
	ACPI_STATUS status;
	uint32_t offset;
	int i, nsegs;

	if (acpi_sc != NULL)
		panic("acpi_sc != NULL");
	acpi_sc = sc;

	status = AcpiGetTable(ACPI_SIG_MCFG, 0, (ACPI_TABLE_HEADER **)&mcfg);
	if (ACPI_FAILURE(status)) {
		mcfg = NULL;
		return;
	}

	nsegs = 0;
	offset = sizeof(ACPI_TABLE_MCFG);
	ama = ACPI_ADD_PTR(ACPI_MCFG_ALLOCATION, mcfg, offset);
	for (i = 0; offset + sizeof(ACPI_MCFG_ALLOCATION) <=
	    mcfg->Header.Length; i++) {
		aprint_debug_dev(sc->sc_dev,
		    "MCFG: segment %d, bus %d-%d, address 0x%016" PRIx64 "\n",
		    ama->PciSegment, ama->StartBusNumber, ama->EndBusNumber,
		    ama->Address);
		nsegs++;
		offset += sizeof(ACPI_MCFG_ALLOCATION);
		ama = ACPI_ADD_PTR(ACPI_MCFG_ALLOCATION, mcfg, offset);
	}
	if (nsegs == 0) {
		mcfg = NULL;
		return;
	}

	mcfg_segs = kmem_zalloc(sizeof(*mcfg_segs) * nsegs, KM_SLEEP);
	mcfg_nsegs = nsegs;
}

int
acpimcfg_init(bus_space_tag_t memt, const struct acpimcfg_ops *ops)
{
	ACPI_MCFG_ALLOCATION *ama;
	struct mcfg_segment *seg;
	uint32_t offset;
	int i, n, nsegs, bus_end;

	if (mcfg == NULL)
		return ENXIO;

	if (mcfg_inited)
		return 0;

	if (ops != NULL)
		mcfg_ops = ops;

	nsegs = 0;
	offset = sizeof(ACPI_TABLE_MCFG);
	ama = ACPI_ADD_PTR(ACPI_MCFG_ALLOCATION, mcfg, offset);
	for (i = 0; offset < mcfg->Header.Length; i++) {
#ifndef _LP64
		if (ama->Address >= 0x100000000ULL) {
			aprint_debug_dev(acpi_sc->sc_dev,
			    "MCFG: segment %d, bus %d-%d, address 0x%016" PRIx64
			    ": ignore (64bit address)\n", ama->PciSegment,
			    ama->StartBusNumber, ama->EndBusNumber,
			    ama->Address);
			goto next;
		}
#endif
		/*
		 * Some (broken?) BIOSen have an MCFG table for an empty
		 * bus range.  Ignore those tables.
		 */
		if (ama->StartBusNumber > ama->EndBusNumber) {
			aprint_debug_dev(acpi_sc->sc_dev,
			    "MCFG: segment %d, bus %d-%d, address 0x%016" PRIx64
			    ": ignore (bus %d > %d)\n", ama->PciSegment,
			    ama->StartBusNumber, ama->EndBusNumber,
			    ama->Address, ama->StartBusNumber,
			    ama->EndBusNumber);
			goto next;
		}

		/* Validate MCFG memory range */
		bus_end = ama->EndBusNumber;
		if (mcfg_ops->ao_validate != NULL &&
		    !mcfg_ops->ao_validate(ama->Address, ama->StartBusNumber,
		      &bus_end)) {
			if (!acpimcfg_find_system_resource( ama->Address,
			    ama->StartBusNumber, &bus_end)) {
				aprint_debug_dev(acpi_sc->sc_dev,
				    "MCFG: segment %d, bus %d-%d, "
				    "address 0x%016" PRIx64
				    ": ignore (invalid address)\n",
				    ama->PciSegment,
				    ama->StartBusNumber, ama->EndBusNumber,
				    ama->Address);
				goto next;
			}
		}
		if (ama->EndBusNumber != bus_end) {
			aprint_debug_dev(acpi_sc->sc_dev,
			    "MCFG: segment %d, bus %d-%d, address 0x%016" PRIx64
			    " -> bus %d-%d\n", ama->PciSegment,
			    ama->StartBusNumber, ama->EndBusNumber,
			    ama->Address, ama->StartBusNumber, bus_end);
		}

#ifndef __HAVE_PCI_GET_SEGMENT
		if (ama->PciSegment != 0) {
			aprint_debug_dev(acpi_sc->sc_dev,
			    "MCFG: segment %d, bus %d-%d, address 0x%016" PRIx64
			    ": ignore (non PCI segment 0)\n", ama->PciSegment,
			    ama->StartBusNumber, bus_end, ama->Address);
			goto next;
		}
#endif

		seg = &mcfg_segs[nsegs++];
		seg->ms_address = ama->Address;
		seg->ms_segment = ama->PciSegment;
		seg->ms_bus_start = ama->StartBusNumber;
		seg->ms_bus_end = bus_end;
		seg->ms_bst = memt;
		n = seg->ms_bus_end - seg->ms_bus_start + 1;
		seg->ms_bus = kmem_zalloc(sizeof(*seg->ms_bus) * n, KM_SLEEP);

 next:
		offset += sizeof(ACPI_MCFG_ALLOCATION);
		ama = ACPI_ADD_PTR(ACPI_MCFG_ALLOCATION, mcfg, offset);
	}
	if (nsegs == 0)
		return ENOENT;

	for (i = 0; i < nsegs; i++) {
		seg = &mcfg_segs[i];
		aprint_verbose_dev(acpi_sc->sc_dev,
		    "MCFG: segment %d, bus %d-%d, address 0x%016" PRIx64 "\n",
		    seg->ms_segment, seg->ms_bus_start, seg->ms_bus_end,
		    seg->ms_address);
	}

	/* Update # of segment */
	mcfg_nsegs = nsegs;
	mcfg_inited = true;

	return 0;
}

static int
acpimcfg_ext_conf_is_aliased(pci_chipset_tag_t pc, pcitag_t tag)
{
	pcireg_t id;
	int i;

	id = pci_conf_read(pc, tag, PCI_ID_REG);
	for (i = PCI_CONF_SIZE; i < PCI_EXTCONF_SIZE; i += PCI_CONF_SIZE) {
		if (pci_conf_read(pc, tag, i) != id)
			return false;
	}
	return true;
}

static struct mcfg_segment *
acpimcfg_get_segment(pci_chipset_tag_t pc, int bus)
{
	struct mcfg_segment *seg;
	u_int segment;
	int i;

#ifdef __HAVE_PCI_GET_SEGMENT
	segment = pci_get_segment(pc);
#else
	segment = 0;
#endif

	for (i = 0; i < mcfg_nsegs; i++) {
		seg = &mcfg_segs[i];
		if (segment == seg->ms_segment &&
		    bus >= seg->ms_bus_start && bus <= seg->ms_bus_end)
			return seg;
	}
	return NULL;
}

static int
acpimcfg_device_probe(const struct pci_attach_args *pa)
{
	pci_chipset_tag_t pc = pa->pa_pc;
	struct mcfg_segment *seg;
	struct mcfg_bus *mb;
	pcitag_t tag;
	pcireg_t reg;
	int bus = pa->pa_bus;
	int dev = pa->pa_device;
	int func = pa->pa_function;
	int last_dev, last_func, end_func;
	int alias = 0;
	const struct pci_quirkdata *qd;
	bool force_hasextcnf = false;
	bool force_noextcnf = false;
	int i, j;

	seg = acpimcfg_get_segment(pc, bus);
	if (seg == NULL)
		return 0;

	mb = &seg->ms_bus[bus - seg->ms_bus_start];
	tag = pci_make_tag(pc, bus, dev, func);

	/* Mark invalid between last probed device to probed device. */
	pci_decompose_tag(pc, mb->last_probed, NULL, &last_dev, &last_func);
	if (dev != 0 || func != 0) {
		for (i = last_dev; i <= dev; i++) {
			end_func = (i == dev) ? func : 8;
			for (j = last_func; j < end_func; j++) {
				if (i == last_dev && j == last_func)
					continue;
				PCIDEV_SET_INVALID(mb, i, j);
			}
			last_func = 0;
		}
	}
	mb->last_probed = tag;

	reg = pci_conf_read(pc, tag, PCI_ID_REG);
	qd = pci_lookup_quirkdata(PCI_VENDOR(reg), PCI_PRODUCT(reg));
	if (qd != NULL && (qd->quirks & PCI_QUIRK_HASEXTCNF) != 0)
		force_hasextcnf = true;
	if (qd != NULL && (qd->quirks & PCI_QUIRK_NOEXTCNF) != 0)
		force_noextcnf = true;
	
	/* Probe extended configuration space. */
	if ((!force_hasextcnf) && ((force_noextcnf) ||
		((reg = pci_conf_read(pc, tag, PCI_CONF_SIZE)) == (pcireg_t)-1)
		|| (reg == 0)
		|| (alias = acpimcfg_ext_conf_is_aliased(pc, tag)))) {
		aprint_debug_dev(acpi_sc->sc_dev,
		    "MCFG: %03d:%02d:%d: invalid config space "
		    "(cfg[0x%03x]=0x%08x, alias=%s)\n", bus, dev, func,
		    PCI_CONF_SIZE, reg, alias ? "true" : "false");
		EXTCONF_SET_INVALID(mb, dev, func);
	}

	aprint_debug_dev(acpi_sc->sc_dev,
	    "MCFG: %03d:%02d:%d: Ok (cfg[0x%03x]=0x%08x extconf=%c)\n",
	    bus, dev, func, PCI_CONF_SIZE, reg,
	    EXTCONF_IS_VALID(mb, dev, func) ? 'Y' : 'N');
	mb->valid_ndevs++;

	return 0;
}

#ifdef PCI_MACHDEP_ENUMERATE_BUS
#define pci_enumerate_bus PCI_MACHDEP_ENUMERATE_BUS
#endif

static void
acpimcfg_scan_bus(struct pci_softc *sc, pci_chipset_tag_t pc, int bus)
{
	static const int wildcard[PCICF_NLOCS] = {
		PCICF_DEV_DEFAULT, PCICF_FUNCTION_DEFAULT
	};

	sc->sc_bus = bus;	/* XXX */
	sc->sc_pc = pc;

	pci_enumerate_bus(sc, wildcard, acpimcfg_device_probe, NULL);
}

int
acpimcfg_map_bus(device_t self, pci_chipset_tag_t pc, int bus)
{
	struct pci_softc *sc = device_private(self);
	struct mcfg_segment *seg = NULL;
	struct mcfg_bus *mb;
	bus_space_handle_t bsh;
	bus_addr_t baddr;
	pcitag_t tag;
	pcireg_t reg;
	bool is_e7520_mch;
	int boff;
	int last_dev, last_func;
	int i, j;
	int error;

	if (!mcfg_inited)
		return ENXIO;

	seg = acpimcfg_get_segment(pc, bus);
	if (seg == NULL)
		return ENOENT;

	boff = bus - seg->ms_bus_start;
	if (seg->ms_bus[boff].valid_ndevs > 0)
		return 0;

	mb = &seg->ms_bus[boff];
	baddr = seg->ms_address + (bus * ACPIMCFG_SIZE_PER_BUS);

	/* Map extended configration space of all dev/func. */
	error = bus_space_map(seg->ms_bst, baddr, ACPIMCFG_SIZE_PER_BUS, 0,
	    &bsh);
	if (error != 0)
		return error;
	for (i = 0; i < 32; i++) {
		for (j = 0; j < 8; j++) {
			error = bus_space_subregion(seg->ms_bst, bsh,
			    EXTCONF_OFFSET(i, j, 0), PCI_EXTCONF_SIZE,
			    &mb->bsh[i][j]);
			if (error != 0)
				break;
		}
	}
	if (error != 0)
		return error;

	aprint_debug("\n");

	/* Probe extended configuration space of all devices. */
	memset(mb->valid_devs, 0xff, sizeof(mb->valid_devs));
	memset(mb->valid_extconf, 0xff, sizeof(mb->valid_extconf));
	mb->valid_ndevs = 0;
	mb->last_probed = pci_make_tag(pc, bus, 0, 0);

	/*
	 * On an Intel E7520 we have to temporarily disable
	 * Enhanced Config Access error detection and reporting
	 * by setting the appropriate error mask in HI_ERRMASK register.
	 *
	 * See "Intel E7520 Memory Controller Hub (MCH) Datasheet",
	 * Document 303006-002, pg. 82
	 */
	tag = pci_make_tag(pc, 0, 0, 1);
	reg = pci_conf_read(pc, tag, PCI_ID_REG);
	is_e7520_mch = (reg ==
	    PCI_ID_CODE(PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_E7525_MCHER));
	if (is_e7520_mch) {
		reg = pci_conf_read(pc, tag, 0x54);
		pci_conf_write(pc, tag, 0x54, reg | 0x20);
	}

	acpimcfg_scan_bus(sc, pc, bus);

	if (is_e7520_mch) {
		pci_conf_write(pc, tag, 0x54, reg);
	}

	/* Unmap configuration space of all dev/func. */
	bus_space_unmap(seg->ms_bst, bsh, ACPIMCFG_SIZE_PER_BUS);
	memset(mb->bsh, 0, sizeof(mb->bsh));

	if (mb->valid_ndevs == 0) {
		aprint_debug_dev(acpi_sc->sc_dev,
		    "MCFG: bus %d: no valid devices.\n", bus);
		memset(mb->valid_devs, 0, sizeof(mb->valid_devs));
		goto out;
	}

	/* Mark invalid on remaining all devices. */
	pci_decompose_tag(pc, mb->last_probed, NULL, &last_dev, &last_func);
	for (i = last_dev; i < 32; i++) {
		for (j = last_func; j < 8; j++) {
			if (i == last_dev && j == last_func) {
				/* Don't mark invalid to last probed device. */
				continue;
			}
			PCIDEV_SET_INVALID(mb, i, j);
		}
		last_func = 0;
	}

	/* Map configuration space per dev/func. */
	for (i = 0; i < 32; i++) {
		for (j = 0; j < 8; j++) {
			if (!PCIDEV_IS_VALID(mb, i, j))
				continue;
			error = bus_space_map(seg->ms_bst,
			    baddr + EXTCONF_OFFSET(i, j, 0), PCI_EXTCONF_SIZE,
			    0, &mb->bsh[i][j]);
			if (error != 0) {
				/* Unmap all handles when map failed. */
				do {
					while (--j >= 0) {
						if (!PCIDEV_IS_VALID(mb, i, j))
							continue;
						bus_space_unmap(seg->ms_bst,
						    mb->bsh[i][j],
						    PCI_EXTCONF_SIZE);
					}
					j = 8;
				} while (--i >= 0);
				memset(mb->valid_devs, 0,
				    sizeof(mb->valid_devs));
				goto out;
			}
		}
	}

	aprint_debug_dev(acpi_sc->sc_dev, "MCFG: bus %d: valid devices\n", bus);
	for (i = 0; i < 32; i++) {
		for (j = 0; j < 8; j++) {
			if (PCIDEV_IS_VALID(mb, i, j)) {
				aprint_debug_dev(acpi_sc->sc_dev,
				    "MCFG: %03d:%02d:%d\n", bus, i, j);
			}
		}
	}

	error = 0;
out:
	aprint_debug_dev(acpi_sc->sc_dev, "%s done", __func__);

	return error;
}

#ifdef PCI_NETBSD_CONFIGURE
struct acpimcfg_resource {
	struct extent *ioext;
	struct extent *memext;
	struct extent *pmemext;
};

static ACPI_STATUS
acpimcfg_configure_bus_cb(ACPI_RESOURCE *res, void *ctx)
{
	struct acpimcfg_resource *pcires = ctx;
	struct extent *ex;
	const char *s;
	int error;

	if (res->Type != ACPI_RESOURCE_TYPE_ADDRESS16 &&
	    res->Type != ACPI_RESOURCE_TYPE_ADDRESS32 &&
	    res->Type != ACPI_RESOURCE_TYPE_ADDRESS64)
		return AE_OK;

	if (res->Data.Address.ProducerConsumer != ACPI_PRODUCER)
		return AE_OK;

	if (res->Data.Address.ResourceType != ACPI_MEMORY_RANGE &&
	    res->Data.Address.ResourceType != ACPI_IO_RANGE)
		return AE_OK;

	if (res->Data.Address.ResourceType == ACPI_MEMORY_RANGE &&
	    res->Data.Address.Info.Mem.Caching == ACPI_PREFETCHABLE_MEMORY) {
		if (pcires->pmemext == NULL) {
			pcires->pmemext = extent_create("pcipmem", 0, ULONG_MAX,
			    NULL, 0, EX_WAITOK);
			error = extent_alloc_region(pcires->pmemext, 0, ULONG_MAX,
			    EX_WAITOK);
			if (error) {
				extent_destroy(pcires->pmemext);
				pcires->pmemext = NULL;
				return AE_NO_MEMORY;
			}
		}
		ex = pcires->pmemext;
		s = "prefetchable";
	} else if (res->Data.Address.ResourceType == ACPI_MEMORY_RANGE &&
	    res->Data.Address.Info.Mem.Caching != ACPI_PREFETCHABLE_MEMORY) {
		if (pcires->memext == NULL) {
			pcires->memext = extent_create("pcimem", 0, ULONG_MAX,
			    NULL, 0, EX_WAITOK);
			error = extent_alloc_region(pcires->memext, 0, ULONG_MAX,
			    EX_WAITOK);
			if (error) {
				extent_destroy(pcires->memext);
				pcires->memext = NULL;
				return AE_NO_MEMORY;
			}
		}
		ex = pcires->memext;
		s = "non-prefetchable";
	} else if (res->Data.Address.ResourceType == ACPI_IO_RANGE) {
		if (pcires->ioext == NULL) {
			pcires->ioext = extent_create("pciio", 0, ULONG_MAX,
			    NULL, 0, EX_WAITOK);
			error = extent_alloc_region(pcires->ioext, 0, ULONG_MAX,
			    EX_WAITOK);
			if (error) {
				extent_destroy(pcires->ioext);
				pcires->ioext = NULL;
				return AE_NO_MEMORY;
			}
		}
		ex = pcires->ioext;
		s = "i/o";
		
	}

	switch (res->Type) {
	case ACPI_RESOURCE_TYPE_ADDRESS16:
		aprint_debug(
		    "MCFG: range 0x%04" PRIx16 " size %#" PRIx16 " (16-bit %s)\n",
		    res->Data.Address16.Address.Minimum,
		    res->Data.Address16.Address.AddressLength,
		    s);
		error = extent_free(ex, res->Data.Address16.Address.Minimum,
		    res->Data.Address16.Address.AddressLength, EX_WAITOK);
		if (error)
			return AE_NO_MEMORY;
		break;
	case ACPI_RESOURCE_TYPE_ADDRESS32:
		aprint_debug(
		    "MCFG: range 0x%08" PRIx32 " size %#" PRIx32 " (32-bit %s)\n",
		    res->Data.Address32.Address.Minimum,
		    res->Data.Address32.Address.AddressLength,
		    s);
		error = extent_free(ex, res->Data.Address32.Address.Minimum,
		    res->Data.Address32.Address.AddressLength, EX_WAITOK);
		if (error)
			return AE_NO_MEMORY;
		break;
	case ACPI_RESOURCE_TYPE_ADDRESS64:
		aprint_debug(
		    "MCFG: range 0x%016" PRIx64 " size %#" PRIx64 " (64-bit %s)\n",
		    res->Data.Address64.Address.Minimum,
		    res->Data.Address64.Address.AddressLength,
		    s);
		error = extent_free(ex, res->Data.Address64.Address.Minimum,
		    res->Data.Address64.Address.AddressLength, EX_WAITOK);
		if (error)
			return AE_NO_MEMORY;
		break;
	}

	return AE_OK;
}

int
acpimcfg_configure_bus(device_t self, pci_chipset_tag_t pc, ACPI_HANDLE handle,
    int bus, int cacheline_size)
{
	struct acpimcfg_resource res;
	struct mcfg_segment *seg;
	struct mcfg_bus *mb;
	bus_space_handle_t bsh[256];
	bool bsh_mapped[256];
	int error, boff, b, d, f;
	bus_addr_t baddr;
	ACPI_STATUS rv;

	seg = acpimcfg_get_segment(pc, bus);
	if (seg == NULL)
		return ENOENT;

	/*
	 * Map config space for all possible busses and mark them valid during
	 * configuration so pci_configure_bus can access them through our chipset
	 * tag with acpimcfg_conf_read/write below.
	 */
	memset(bsh_mapped, 0, sizeof(bsh_mapped));
	for (b = seg->ms_bus_start; b <= seg->ms_bus_end; b++) {
		boff = b - seg->ms_bus_start;
		mb = &seg->ms_bus[boff];
		baddr = seg->ms_address + (b * ACPIMCFG_SIZE_PER_BUS);

		/* Map extended configration space of all dev/func. */
		error = bus_space_map(seg->ms_bst, baddr, ACPIMCFG_SIZE_PER_BUS, 0,
		    &bsh[b]);
		if (error != 0)
			goto cleanup;
		bsh_mapped[b] = true;
		for (d = 0; d < 32; d++) {
			for (f = 0; f < 8; f++) {
				error = bus_space_subregion(seg->ms_bst, bsh[b],
				    EXTCONF_OFFSET(d, f, 0), PCI_EXTCONF_SIZE,
				    &mb->bsh[d][f]);
				if (error != 0)
					break;
			}
		}
		if (error != 0)
			goto cleanup;

		memset(mb->valid_devs, 0xff, sizeof(mb->valid_devs));
	}

	memset(&res, 0, sizeof(res));
	rv = AcpiWalkResources(handle, "_CRS", acpimcfg_configure_bus_cb, &res);
	if (ACPI_FAILURE(rv)) {
		error = ENXIO;
		goto cleanup;
	}

	error = pci_configure_bus(pc, res.ioext, res.memext, res.pmemext, bus,
	    cacheline_size);

cleanup:
	/*
	 * Unmap config space for the segment's busses. Valid devices will be
	 * re-mapped later on by acpimcfg_map_bus.
	 */
	for (b = seg->ms_bus_start; b <= seg->ms_bus_end; b++) {
		boff = b - seg->ms_bus_start;
		mb = &seg->ms_bus[boff];
		memset(mb->valid_devs, 0, sizeof(mb->valid_devs));

		if (bsh_mapped[b])
			bus_space_unmap(seg->ms_bst, bsh[b], ACPIMCFG_SIZE_PER_BUS);
	}

	if (res.ioext)
		extent_destroy(res.ioext);
	if (res.memext)
		extent_destroy(res.memext);
	if (res.pmemext)
		extent_destroy(res.pmemext);

	return error;
}
#else
int
acpimcfg_configure_bus(device_t self, pci_chipset_tag_t pc, ACPI_HANDLE handle,
    int bus, int cacheline_size)
{
	return ENXIO;
}
#endif

int
acpimcfg_conf_read(pci_chipset_tag_t pc, pcitag_t tag, int reg, pcireg_t *data)
{
	struct mcfg_segment *seg = NULL;
	struct mcfg_bus *mb;
	int bus, dev, func;

	KASSERT(reg < PCI_EXTCONF_SIZE);
	KASSERT((reg & 3) == 0);

	if (!mcfg_inited) {
		*data = -1;
		return ENXIO;
	}

	pci_decompose_tag(pc, tag, &bus, &dev, &func);

	seg = acpimcfg_get_segment(pc, bus);
	if (seg == NULL) {
		*data = -1;
		return ERANGE;
	}

	mb = &seg->ms_bus[bus - seg->ms_bus_start];
	if (!PCIDEV_IS_VALID(mb, dev, func)) {
		*data = -1;
		return EINVAL;
	}
	if (!EXTCONF_IS_VALID(mb, dev, func) && reg >= PCI_CONF_SIZE) {
		*data = -1;
		return EINVAL;
	}

	*data = mcfg_ops->ao_read(seg->ms_bst, mb->bsh[dev][func], reg);
	return 0;
}

int
acpimcfg_conf_write(pci_chipset_tag_t pc, pcitag_t tag, int reg, pcireg_t data)
{
	struct mcfg_segment *seg = NULL;
	struct mcfg_bus *mb;
	int bus, dev, func;

	KASSERT(reg < PCI_EXTCONF_SIZE);
	KASSERT((reg & 3) == 0);

	if (!mcfg_inited)
		return ENXIO;

	pci_decompose_tag(pc, tag, &bus, &dev, &func);

	seg = acpimcfg_get_segment(pc, bus);
	if (seg == NULL)
		return ERANGE;

	mb = &seg->ms_bus[bus - seg->ms_bus_start];
	if (!PCIDEV_IS_VALID(mb, dev, func))
		return EINVAL;
	if (!EXTCONF_IS_VALID(mb, dev, func) && reg >= PCI_CONF_SIZE)
		return EINVAL;

	mcfg_ops->ao_write(seg->ms_bst, mb->bsh[dev][func], reg, data);
	return 0;
}
