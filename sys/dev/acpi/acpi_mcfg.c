/*	$NetBSD: acpi_mcfg.c,v 1.1 2015/10/02 05:22:52 msaitoh Exp $	*/

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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: acpi_mcfg.c,v 1.1 2015/10/02 05:22:52 msaitoh Exp $");

#include <sys/param.h>
#include <sys/device.h>
#include <sys/kmem.h>
#include <sys/systm.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>
#include <dev/acpi/acpi_mcfg.h>

#include "locators.h"

#define	EXTCONF_OFFSET(d, f, r)	((((d) * 8 + (f)) * PCI_EXTCONF_SIZE) + (r))

#define	EXTCONF_SET_VALID(mb, d, f)	((mb)->valid_devs[(d)] |= __BIT((f)))
#define	EXTCONF_SET_INVALID(mb, d, f)	((mb)->valid_devs[(d)] &= ~__BIT((f)))
#define	EXTCONF_IS_VALID(mb, d, f)	((mb)->valid_devs[(d)] & __BIT((f)))

struct mcfg_segment {
	uint64_t ms_address;		/* Base address */
	int ms_segment;			/* Segment # */
	int ms_bus_start;		/* Start bus # */
	int ms_bus_end;			/* End bus # */
	bus_space_tag_t ms_bst;
	struct mcfg_bus {
		bus_space_handle_t bsh[32][8];
		uint8_t valid_devs[32];
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
	for (i = 0; offset < mcfg->Header.Length; i++) {
		aprint_debug_dev(sc->sc_dev,
		    "MCFG: segment %d, bus %d-%d, address 0x%016" PRIx64 "\n",
		    ama->PciSegment, ama->StartBusNumber, ama->EndBusNumber,
		    ama->Address);
		nsegs++;
		offset += sizeof(ACPI_MCFG_ALLOCATION);
		ama = ACPI_ADD_PTR(ACPI_MCFG_ALLOCATION, ama, offset);
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
		if (ama->StartBusNumber == ama->EndBusNumber) {
			aprint_debug_dev(acpi_sc->sc_dev,
			    "MCFG: segment %d, bus %d-%d, address 0x%016" PRIx64
			    ": ignore (bus %d == %d)\n", ama->PciSegment,
			    ama->StartBusNumber, ama->EndBusNumber,
			    ama->Address, ama->StartBusNumber,
			    ama->EndBusNumber);
			goto next;
		}
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

		if (ama->PciSegment != 0) {
			aprint_debug_dev(acpi_sc->sc_dev,
			    "MCFG: segment %d, bus %d-%d, address 0x%016" PRIx64
			    ": ignore (non PCI segment 0)\n", ama->PciSegment,
			    ama->StartBusNumber, bus_end, ama->Address);
			goto next;
		}

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
		ama = ACPI_ADD_PTR(ACPI_MCFG_ALLOCATION, ama, offset);
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
acpimcfg_get_segment(int bus)
{
	struct mcfg_segment *seg;
	int i;

	for (i = 0; i < mcfg_nsegs; i++) {
		seg = &mcfg_segs[i];
		if (bus >= seg->ms_bus_start && bus <= seg->ms_bus_end)
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
	int i, j;

	seg = acpimcfg_get_segment(bus);
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
				EXTCONF_SET_INVALID(mb, i, j);
			}
			last_func = 0;
		}
	}
	mb->last_probed = tag;

	/* Probe extended configuration space. */
	if (((reg = pci_conf_read(pc, tag, PCI_CONF_SIZE)) == (pcireg_t)-1) ||
	    (alias = acpimcfg_ext_conf_is_aliased(pc, tag))) {
		aprint_debug_dev(acpi_sc->sc_dev,
		    "MCFG: %03d:%02d:%d: invalid config space "
		    "(cfg[0x%03x]=0x%08x, alias=%s)\n", bus, dev, func,
		    PCI_CONF_SIZE, reg, alias ? "true" : "false");
		EXTCONF_SET_INVALID(mb, dev, func);
	} else {
		aprint_debug_dev(acpi_sc->sc_dev,
		    "MCFG: %03d:%02d:%d: Ok (cfg[0x%03x]=0x%08x)\n",
		    bus, dev, func, PCI_CONF_SIZE, reg);
		mb->valid_ndevs++;
	}

	return 0;
}

static void
acpimcfg_scan_bus(struct pci_softc *sc, pci_chipset_tag_t pc, int bus)
{
	static const int wildcard[PCICF_NLOCS] = {
		PCICF_DEV_DEFAULT, PCICF_FUNCTION_DEFAULT
	};

	sc->sc_bus = bus;	/* XXX */

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
	int boff;
	int last_dev, last_func;
	int i, j;
	int error;

	if (!mcfg_inited)
		return ENXIO;

	seg = acpimcfg_get_segment(bus);
	if (seg == NULL)
		return ENOENT;

	boff = bus - seg->ms_bus_start;
	if (seg->ms_bus[boff].valid_ndevs > 0)
		return 0;

	mb = &seg->ms_bus[boff];
	baddr = seg->ms_address + (boff * ACPIMCFG_SIZE_PER_BUS);

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
	mb->valid_ndevs = 0;
	mb->last_probed = pci_make_tag(pc, bus, 0, 0);

	acpimcfg_scan_bus(sc, pc, bus);

	/* Unmap extended configration space of all dev/func. */
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
			EXTCONF_SET_INVALID(mb, i, j);
		}
		last_func = 0;
	}

	/* Map extended configuration space per dev/func. */
	for (i = 0; i < 32; i++) {
		for (j = 0; j < 8; j++) {
			if (!EXTCONF_IS_VALID(mb, i, j))
				continue;
			error = bus_space_map(seg->ms_bst,
			    baddr + EXTCONF_OFFSET(i, j, 0), PCI_EXTCONF_SIZE,
			    0, &mb->bsh[i][j]);
			if (error != 0) {
				/* Unmap all handles when map failed. */
				do {
					while (--j >= 0) {
						if (!EXTCONF_IS_VALID(mb, i, j))
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
			if (EXTCONF_IS_VALID(mb, i, j)) {
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

	seg = acpimcfg_get_segment(bus);
	if (seg == NULL) {
		*data = -1;
		return ERANGE;
	}

	mb = &seg->ms_bus[bus - seg->ms_bus_start];
	if (!EXTCONF_IS_VALID(mb, dev, func)) {
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

	seg = acpimcfg_get_segment(bus);
	if (seg == NULL)
		return ERANGE;

	mb = &seg->ms_bus[bus - seg->ms_bus_start];
	if (!EXTCONF_IS_VALID(mb, dev, func))
		return EINVAL;

	mcfg_ops->ao_write(seg->ms_bst, mb->bsh[dev][func], reg, data);
	return 0;
}
