/*	$NetBSD: pci_map.c,v 1.32 2014/12/26 05:09:03 msaitoh Exp $	*/

/*-
 * Copyright (c) 1998, 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum; by William R. Studenmund; by Jason R. Thorpe.
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
 * PCI device mapping.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: pci_map.c,v 1.32 2014/12/26 05:09:03 msaitoh Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

static int
pci_io_find(pci_chipset_tag_t pc, pcitag_t tag, int reg, pcireg_t type,
    bus_addr_t *basep, bus_size_t *sizep, int *flagsp)
{
	pcireg_t address, mask;
	int s;

	if (reg < PCI_MAPREG_START ||
#if 0
	    /*
	     * Can't do this check; some devices have mapping registers
	     * way out in left field.
	     */
	    reg >= PCI_MAPREG_END ||
#endif
	    (reg & 3))
		panic("pci_io_find: bad request");

	/*
	 * Section 6.2.5.1, `Address Maps', tells us that:
	 *
	 * 1) The builtin software should have already mapped the device in a
	 * reasonable way.
	 *
	 * 2) A device which wants 2^n bytes of memory will hardwire the bottom
	 * n bits of the address to 0.  As recommended, we write all 1s and see
	 * what we get back.
	 */
	s = splhigh();
	address = pci_conf_read(pc, tag, reg);
	pci_conf_write(pc, tag, reg, 0xffffffff);
	mask = pci_conf_read(pc, tag, reg);
	pci_conf_write(pc, tag, reg, address);
	splx(s);

	if (PCI_MAPREG_TYPE(address) != PCI_MAPREG_TYPE_IO) {
		aprint_debug("pci_io_find: expected type i/o, found mem\n");
		return 1;
	}

	if (PCI_MAPREG_IO_SIZE(mask) == 0) {
		aprint_debug("pci_io_find: void region\n");
		return 1;
	}

	if (basep != NULL)
		*basep = PCI_MAPREG_IO_ADDR(address);
	if (sizep != NULL)
		*sizep = PCI_MAPREG_IO_SIZE(mask);
	if (flagsp != NULL)
		*flagsp = 0;

	return 0;
}

static int
pci_mem_find(pci_chipset_tag_t pc, pcitag_t tag, int reg, pcireg_t type,
    bus_addr_t *basep, bus_size_t *sizep, int *flagsp)
{
	pcireg_t address, mask, address1 = 0, mask1 = 0xffffffff;
	u_int64_t waddress, wmask;
	int s, is64bit, isrom;

	is64bit = (PCI_MAPREG_MEM_TYPE(type) == PCI_MAPREG_MEM_TYPE_64BIT);
	isrom = (reg == PCI_MAPREG_ROM);

	if ((!isrom) && (reg < PCI_MAPREG_START ||
#if 0
	    /*
	     * Can't do this check; some devices have mapping registers
	     * way out in left field.
	     */
	    reg >= PCI_MAPREG_END ||
#endif
	    (reg & 3)))
		panic("pci_mem_find: bad request");

	if (is64bit && (reg + 4) >= PCI_MAPREG_END)
		panic("pci_mem_find: bad 64-bit request");

	/*
	 * Section 6.2.5.1, `Address Maps', tells us that:
	 *
	 * 1) The builtin software should have already mapped the device in a
	 * reasonable way.
	 *
	 * 2) A device which wants 2^n bytes of memory will hardwire the bottom
	 * n bits of the address to 0.  As recommended, we write all 1s and see
	 * what we get back.  Only probe the upper BAR of a mem64 BAR if bit 31
	 * is readonly.
	 */
	s = splhigh();
	address = pci_conf_read(pc, tag, reg);
	pci_conf_write(pc, tag, reg, 0xffffffff);
	mask = pci_conf_read(pc, tag, reg);
	pci_conf_write(pc, tag, reg, address);
	if (is64bit) {
		address1 = pci_conf_read(pc, tag, reg + 4);
		if ((mask & 0x80000000) == 0) {
			pci_conf_write(pc, tag, reg + 4, 0xffffffff);
			mask1 = pci_conf_read(pc, tag, reg + 4);
			pci_conf_write(pc, tag, reg + 4, address1);
		}
	}
	splx(s);

	if (!isrom) {
		/*
		 * roms should have an enable bit instead of a memory
		 * type decoder bit.  For normal BARs, make sure that
		 * the address decoder type matches what we asked for.
		 */
		if (PCI_MAPREG_TYPE(address) != PCI_MAPREG_TYPE_MEM) {
			printf("pci_mem_find: expected type mem, found i/o\n");
			return 1;
		}
		/* XXX Allow 64bit bars for 32bit requests.*/
		if (PCI_MAPREG_MEM_TYPE(address) !=
		    PCI_MAPREG_MEM_TYPE(type) &&
		    PCI_MAPREG_MEM_TYPE(address) !=
		    PCI_MAPREG_MEM_TYPE_64BIT) {
			printf("pci_mem_find: "
			    "expected mem type %08x, found %08x\n",
			    PCI_MAPREG_MEM_TYPE(type),
			    PCI_MAPREG_MEM_TYPE(address));
			return 1;
		}
	}

	waddress = (u_int64_t)address1 << 32UL | address;
	wmask = (u_int64_t)mask1 << 32UL | mask;

	if ((is64bit && PCI_MAPREG_MEM64_SIZE(wmask) == 0) ||
	    (!is64bit && PCI_MAPREG_MEM_SIZE(mask) == 0)) {
		aprint_debug("pci_mem_find: void region\n");
		return 1;
	}

	switch (PCI_MAPREG_MEM_TYPE(address)) {
	case PCI_MAPREG_MEM_TYPE_32BIT:
	case PCI_MAPREG_MEM_TYPE_32BIT_1M:
		break;
	case PCI_MAPREG_MEM_TYPE_64BIT:
		/*
		 * Handle the case of a 64-bit memory register on a
		 * platform with 32-bit addressing.  Make sure that
		 * the address assigned and the device's memory size
		 * fit in 32 bits.  We implicitly assume that if
		 * bus_addr_t is 64-bit, then so is bus_size_t.
		 */
		if (sizeof(u_int64_t) > sizeof(bus_addr_t) &&
		    (address1 != 0 || mask1 != 0xffffffff)) {
			printf("pci_mem_find: 64-bit memory map which is "
			    "inaccessible on a 32-bit platform\n");
			return 1;
		}
		break;
	default:
		printf("pci_mem_find: reserved mapping register type\n");
		return 1;
	}

	if (sizeof(u_int64_t) > sizeof(bus_addr_t)) {
		if (basep != NULL)
			*basep = PCI_MAPREG_MEM_ADDR(address);
		if (sizep != NULL)
			*sizep = PCI_MAPREG_MEM_SIZE(mask);
	} else {
		if (basep != NULL)
			*basep = PCI_MAPREG_MEM64_ADDR(waddress);
		if (sizep != NULL)
			*sizep = PCI_MAPREG_MEM64_SIZE(wmask);
	}
	if (flagsp != NULL)
		*flagsp = (isrom || PCI_MAPREG_MEM_PREFETCHABLE(address)) ?
		    BUS_SPACE_MAP_PREFETCHABLE : 0;

	return 0;
}

#define _PCI_MAPREG_TYPEBITS(reg) \
	(PCI_MAPREG_TYPE(reg) == PCI_MAPREG_TYPE_IO ? \
	reg & PCI_MAPREG_TYPE_MASK : \
	reg & (PCI_MAPREG_TYPE_MASK|PCI_MAPREG_MEM_TYPE_MASK))

pcireg_t
pci_mapreg_type(pci_chipset_tag_t pc, pcitag_t tag, int reg)
{

	return _PCI_MAPREG_TYPEBITS(pci_conf_read(pc, tag, reg));
}

int
pci_mapreg_probe(pci_chipset_tag_t pc, pcitag_t tag, int reg, pcireg_t *typep)
{
	pcireg_t address, mask;
	int s;

	s = splhigh();
	address = pci_conf_read(pc, tag, reg);
	pci_conf_write(pc, tag, reg, 0xffffffff);
	mask = pci_conf_read(pc, tag, reg);
	pci_conf_write(pc, tag, reg, address);
	splx(s);

	if (mask == 0) /* unimplemented mapping register */
		return 0;

	if (typep != NULL)
		*typep = _PCI_MAPREG_TYPEBITS(address);
	return 1;
}

int
pci_mapreg_info(pci_chipset_tag_t pc, pcitag_t tag, int reg, pcireg_t type,
    bus_addr_t *basep, bus_size_t *sizep, int *flagsp)
{

	if (PCI_MAPREG_TYPE(type) == PCI_MAPREG_TYPE_IO)
		return pci_io_find(pc, tag, reg, type, basep, sizep,
		    flagsp);
	else
		return pci_mem_find(pc, tag, reg, type, basep, sizep,
		    flagsp);
}

int
pci_mapreg_map(const struct pci_attach_args *pa, int reg, pcireg_t type,
    int busflags, bus_space_tag_t *tagp, bus_space_handle_t *handlep,
    bus_addr_t *basep, bus_size_t *sizep)
{
	return pci_mapreg_submap(pa, reg, type, busflags, 0, 0, tagp, 
	    handlep, basep, sizep);
}

int
pci_mapreg_submap(const struct pci_attach_args *pa, int reg, pcireg_t type,
    int busflags, bus_size_t maxsize, bus_size_t offset, bus_space_tag_t *tagp,
	bus_space_handle_t *handlep, bus_addr_t *basep, bus_size_t *sizep)
{
	bus_space_tag_t tag;
	bus_space_handle_t handle;
	bus_addr_t base;
	bus_size_t size;
	int flags;

	if (PCI_MAPREG_TYPE(type) == PCI_MAPREG_TYPE_IO) {
		if ((pa->pa_flags & PCI_FLAGS_IO_OKAY) == 0)
			return 1;
		if (pci_io_find(pa->pa_pc, pa->pa_tag, reg, type, &base,
		    &size, &flags))
			return 1;
		tag = pa->pa_iot;
	} else {
		if ((pa->pa_flags & PCI_FLAGS_MEM_OKAY) == 0)
			return 1;
		if (pci_mem_find(pa->pa_pc, pa->pa_tag, reg, type, &base,
		    &size, &flags))
			return 1;
		tag = pa->pa_memt;
	}

	if (reg == PCI_MAPREG_ROM) {
		pcireg_t 	mask;
		int		s;
		/* we have to enable the ROM address decoder... */
		s = splhigh();
		mask = pci_conf_read(pa->pa_pc, pa->pa_tag, reg);
		mask |= PCI_MAPREG_ROM_ENABLE;
		pci_conf_write(pa->pa_pc, pa->pa_tag, reg, mask);
		splx(s);
	}

	/* If we're called with maxsize/offset of 0, behave like 
	 * pci_mapreg_map.
	 */

	maxsize = (maxsize != 0) ? maxsize : size;
	base += offset;

	if ((size < maxsize) || (size < (offset + maxsize)))
		return 1;

	if (bus_space_map(tag, base, maxsize, busflags | flags, &handle))
		return 1;

	if (tagp != NULL)
		*tagp = tag;
	if (handlep != NULL)
		*handlep = handle;
	if (basep != NULL)
		*basep = base;
	if (sizep != NULL)
		*sizep = maxsize;

	return 0;
}

int
pci_find_rom(const struct pci_attach_args *pa, bus_space_tag_t bst,
    bus_space_handle_t bsh, bus_size_t sz, int type,
    bus_space_handle_t *romh, bus_size_t *romsz)
{
	bus_size_t	offset = 0, imagesz;
	uint16_t	ptr;
	int		done = 0;

	/*
	 * no upper bound check; i cannot imagine a 4GB ROM, but
	 * it appears the spec would allow it!
	 */
	if (sz < 1024)
		return 1;

	while (offset < sz && !done){
		struct pci_rom_header	hdr;
		struct pci_rom		rom;

		hdr.romh_magic = bus_space_read_2(bst, bsh,
		    offset + offsetof (struct pci_rom_header, romh_magic));
		hdr.romh_data_ptr = bus_space_read_2(bst, bsh,
		    offset + offsetof (struct pci_rom_header, romh_data_ptr));

		/* no warning: quite possibly ROM is simply not populated */
		if (hdr.romh_magic != PCI_ROM_HEADER_MAGIC)
			return 1;

		ptr = offset + hdr.romh_data_ptr;
		
		if (ptr > sz) {
			printf("pci_find_rom: rom data ptr out of range\n");
			return 1;
		}

		rom.rom_signature = bus_space_read_4(bst, bsh, ptr);
		rom.rom_vendor = bus_space_read_2(bst, bsh, ptr +
		    offsetof(struct pci_rom, rom_vendor));
		rom.rom_product = bus_space_read_2(bst, bsh, ptr +
		    offsetof(struct pci_rom, rom_product));
		rom.rom_class = bus_space_read_1(bst, bsh,
		    ptr + offsetof (struct pci_rom, rom_class));
		rom.rom_subclass = bus_space_read_1(bst, bsh,
		    ptr + offsetof (struct pci_rom, rom_subclass));
		rom.rom_interface = bus_space_read_1(bst, bsh,
		    ptr + offsetof (struct pci_rom, rom_interface));
		rom.rom_len = bus_space_read_2(bst, bsh,
		    ptr + offsetof (struct pci_rom, rom_len));
		rom.rom_code_type = bus_space_read_1(bst, bsh,
		    ptr + offsetof (struct pci_rom, rom_code_type));
		rom.rom_indicator = bus_space_read_1(bst, bsh,
		    ptr + offsetof (struct pci_rom, rom_indicator));

		if (rom.rom_signature != PCI_ROM_SIGNATURE) {
			printf("pci_find_rom: bad rom data signature\n");
			return 1;
		}

		imagesz = rom.rom_len * 512;

		if ((rom.rom_vendor == PCI_VENDOR(pa->pa_id)) &&
		    (rom.rom_product == PCI_PRODUCT(pa->pa_id)) &&
		    (rom.rom_class == PCI_CLASS(pa->pa_class)) &&
		    (rom.rom_subclass == PCI_SUBCLASS(pa->pa_class)) &&
		    (rom.rom_interface == PCI_INTERFACE(pa->pa_class)) &&
		    (rom.rom_code_type == type)) {
			*romsz = imagesz;
			bus_space_subregion(bst, bsh, offset, imagesz, romh);
			return 0;
		}
		
		/* last image check */
		if (rom.rom_indicator & PCI_ROM_INDICATOR_LAST)
			return 1;

		/* offset by size */
		offset += imagesz;
	}
	return 1;
}
