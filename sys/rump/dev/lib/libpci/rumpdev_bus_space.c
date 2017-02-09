/*	$NetBSD: rumpdev_bus_space.c,v 1.6 2015/08/11 22:28:34 pooka Exp $	*/

/*-
 * Copyright (c) 2013 Antti Kantee.  All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#include <sys/atomic.h>

#include <sys/param.h>

#include <dev/pci/pcivar.h>

#include "pci_user.h"

#if defined(RUMPCOMP_USERFEATURE_PCI_IOSPACE) \
    && (defined(__i386__) || defined(__x86_64__))
#define IOSPACE_SUPPORTED
#endif

int
bus_space_map(bus_space_tag_t bst, bus_addr_t address, bus_size_t size,
	int flags, bus_space_handle_t *handlep)
{
	int rv;

	/*
	 * I/O space we just "let it bli" in case someone wants to
	 * map it (e.g. on Xen)
 	 *
	 * Memory space needs to be mapped into our guest, so we
	 * make a hypercall to request it.
	 */
	if (bst == 0) {
#ifdef IOSPACE_SUPPORTED
		*handlep = address;
		rv = 0;
#else
		rv = ENOTSUP;
#endif
	} else {
		*handlep = (bus_space_handle_t)rumpcomp_pci_map(address, size);
		rv = *handlep ? 0 : EINVAL;
	}

	return rv;
}

uint8_t
bus_space_read_1(bus_space_tag_t bst, bus_space_handle_t bsh,
	bus_size_t offset)
{
	uint8_t rv;

	if (bst == 0) {
#ifdef IOSPACE_SUPPORTED
		unsigned short addr = bsh + offset;
		__asm__ __volatile__("inb %1, %0" : "=a"(rv) : "d"(addr)); 
#else
		panic("IO space not supported");
#endif
	} else {
		rv = *(volatile uint8_t *)(bsh + offset);
	}

	return rv;
}

uint16_t
bus_space_read_2(bus_space_tag_t bst, bus_space_handle_t bsh,
	bus_size_t offset)
{
	uint16_t rv;

	if (bst == 0) {
#ifdef IOSPACE_SUPPORTED
		unsigned short addr = bsh + offset;
		__asm__ __volatile__("in %1, %0" : "=a"(rv) : "d"(addr)); 
#else
		panic("IO space not supported");
#endif
	} else {
		rv = *(volatile uint16_t *)(bsh + offset);
	}

	return rv;
}

uint32_t
bus_space_read_4(bus_space_tag_t bst, bus_space_handle_t bsh,
	bus_size_t offset)
{
	uint32_t rv;

	if (bst == 0) {
#ifdef IOSPACE_SUPPORTED
		unsigned short addr = bsh + offset;
		__asm__ __volatile__("inl %1, %0" : "=a"(rv) : "d"(addr)); 
#else
		panic("IO space not supported");
#endif
	} else {
		rv = *(volatile uint32_t *)(bsh + offset);
	}

	return rv;
}

void
bus_space_read_multi_1(bus_space_tag_t bst, bus_space_handle_t bsh,
	bus_size_t offset, uint8_t *datap, bus_size_t count)
{

	while (count--) {
		*datap++ = bus_space_read_1(bst, bsh, offset);
		bus_space_barrier(bst, bsh, offset, 1, BUS_SPACE_BARRIER_READ);
	}
}

void
bus_space_read_multi_2(bus_space_tag_t bst, bus_space_handle_t bsh,
	bus_size_t offset, uint16_t *datap, bus_size_t count)
{

	while (count--) {
		*datap++ = bus_space_read_2(bst, bsh, offset);
		bus_space_barrier(bst, bsh, offset, 2, BUS_SPACE_BARRIER_READ);
	}
}

void
bus_space_read_multi_4(bus_space_tag_t bst, bus_space_handle_t bsh,
	bus_size_t offset, uint32_t *datap, bus_size_t count)
{

	while (count--) {
		*datap++ = bus_space_read_4(bst, bsh, offset);
		bus_space_barrier(bst, bsh, offset, 4, BUS_SPACE_BARRIER_READ);
	}
}

void
bus_space_write_1(bus_space_tag_t bst, bus_space_handle_t bsh,
	bus_size_t offset, uint8_t v)
{

	if (bst == 0) {
#ifdef IOSPACE_SUPPORTED
		unsigned short addr = bsh + offset;
		__asm__ __volatile__("outb %0, %1" :: "a"(v), "d"(addr));
#else
		panic("IO space not supported");
#endif
	} else {
		*(volatile uint8_t *)(bsh + offset) = v;
	}
}

void
bus_space_write_2(bus_space_tag_t bst, bus_space_handle_t bsh,
	bus_size_t offset, uint16_t v)
{

	if (bst == 0) {
#ifdef IOSPACE_SUPPORTED
		unsigned short addr = bsh + offset;
		__asm__ __volatile__("out %0, %1" :: "a"(v), "d"(addr));
#else
		panic("IO space not supported");
#endif
	} else {
		*(volatile uint16_t *)(bsh + offset) = v;
	}
}

void
bus_space_write_4(bus_space_tag_t bst, bus_space_handle_t bsh,
	bus_size_t offset, uint32_t v)
{

	if (bst == 0) {
#ifdef IOSPACE_SUPPORTED
		unsigned short addr = bsh + offset;
		__asm__ __volatile__("outl %0, %1" :: "a"(v), "d"(addr));
#else
		panic("IO space not supported");
#endif
	} else {
		*(volatile uint32_t *)(bsh + offset) = v;
	}
}

void
bus_space_write_multi_1(bus_space_tag_t bst, bus_space_handle_t bsh,
	bus_size_t offset, const uint8_t *datap, bus_size_t count)
{

	while (count--) {
		const uint8_t value = *datap++;

		bus_space_write_1(bst, bsh, offset, value);
		bus_space_barrier(bst, bsh, offset, 1, BUS_SPACE_BARRIER_WRITE);
	}
}

void
bus_space_write_multi_2(bus_space_tag_t bst, bus_space_handle_t bsh,
	bus_size_t offset, const uint16_t *datap, bus_size_t count)
{

	while (count--) {
		const uint16_t value = *datap++;

		bus_space_write_2(bst, bsh, offset, value);
		bus_space_barrier(bst, bsh, offset, 2, BUS_SPACE_BARRIER_WRITE);
	}
}

void
bus_space_write_multi_4(bus_space_tag_t bst, bus_space_handle_t bsh,
	bus_size_t offset, const uint32_t *datap, bus_size_t count)
{

	while (count--) {
		const uint32_t value = *datap++;

		bus_space_write_4(bst, bsh, offset, value);
		bus_space_barrier(bst, bsh, offset, 4, BUS_SPACE_BARRIER_WRITE);
	}
}

paddr_t
bus_space_mmap(bus_space_tag_t bst, bus_addr_t addr, off_t off,
	int prot, int flags)
{

	panic("%s: unimplemented", __func__);
}

int
bus_space_subregion(bus_space_tag_t bst, bus_space_handle_t bsh,
	bus_size_t offset, bus_size_t size, bus_space_handle_t *nhandlep)
{

	*nhandlep = bsh + offset;
	return 0;
}

void
bus_space_unmap(bus_space_tag_t bst, bus_space_handle_t bsh,
	bus_size_t size)
{

	panic("%s: unimplemented", __func__);
}

void
bus_space_barrier(bus_space_tag_t bst, bus_space_handle_t bsh,
	bus_size_t offset, bus_size_t len, int flags)
{

	/* weelll ... */
	membar_sync();
}
