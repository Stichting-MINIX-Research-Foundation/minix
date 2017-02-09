/*	$NetBSD: rbus_machdep.c,v 1.26 2011/07/01 18:14:15 dyoung Exp $	*/

/*
 * Copyright (c) 1999
 *     HAYAKAWA Koichi.  All rights reserved.
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
__KERNEL_RCSID(0, "$NetBSD: rbus_machdep.c,v 1.26 2011/07/01 18:14:15 dyoung Exp $");

#include "opt_pcibios.h"
#include "opt_pcifixup.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/extent.h>

#include <uvm/uvm_extern.h>

#include <sys/sysctl.h>

#include <sys/bus.h>
#include <dev/cardbus/rbus.h>

#include <sys/device.h>
#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>

#include <dev/pci/pcivar.h>
#if defined(PCI_ADDR_FIXUP)
#include <arch/x86/pci/pci_addr_fixup.h>
#endif

#ifndef RBUS_IO_BASE
#define	RBUS_IO_BASE	0x4000
#endif
#ifndef RBUS_IO_SIZE
#define	RBUS_IO_SIZE	0x2000
#endif

#ifndef RBUS_MIN_START
#define RBUS_MIN_START 0x40000000	/* 1GB */
#else
/*
 * Note that kernel config forced RBUS_MIN_START.
 */
#define RBUS_MIN_START_FORCED
#endif
bus_addr_t rbus_min_start = RBUS_MIN_START;

/*
 * Dynamically set the start address for rbus.  This must be called
 * before rbus is initialized.  The start address should be determined
 * by the amount of installed memory.  Generally 1 GB has been found
 * to be a good value, but it fails on some Thinkpads (e.g. 2645-4AU),
 * for which 0.5 GB is a good value.  It also fails on (at least)
 * Thinkpads with 2GB of RAM, for which 2 GB is a good value.
 *
 * Thus, a general strategy of setting rbus_min_start to the amount of
 * memory seems in order.  However, the actually amount of memory is
 * generally slightly more than the amount found, e.g. 1014MB vs 1024,
 * or 2046 vs 2048.
 */
void
rbus_min_start_hint(psize_t ram)
{
#ifdef RBUS_MIN_START_FORCED
	aprint_debug("rbus: rbus_min_start from config at %#0" PRIxPADDR "\n",
	    rbus_min_start);
#else
        if (ram <= 192*1024*1024UL) {
		/*
		 * <= 192 MB, so try 0.5 GB.  This will work on
		 * Thinkpad 600E (2645-4AU), which fails at 1GB, and
		 * on some other older machines that may have trouble
		 * with addresses needing more than 20 bits.
		 */
		rbus_min_start = 512 * 1024 * 1024UL;
	}

	if (ram >= 1024*1024*1024UL) {
		/*
		 * > 1GB, so try 2 GB.
		 */
		rbus_min_start =  2 * 1024 * 1024 * 1024UL;
	}

	/* XXX Not tested in > 2 GB case. */
	if (ram > 2 * 1024*1024*1024UL) {
		/*
		 * > 2 GB, so try 3 GB.
		 */
		rbus_min_start =  3 * 1024 * 1024 * 1024UL;
	}

	aprint_debug("rbus: rbus_min_start set to %#0" PRIxPADDR "\n",
	   rbus_min_start);
#endif
}


/*
 * rbus_tag_t rbus_fakeparent_mem(struct pci_attach_args *pa)
 *
 *   This function makes an rbus tag for memory space.  This rbus tag
 *   shares the all memory region of ex_iomem.
 */
rbus_tag_t
rbus_pccbb_parent_mem(struct pci_attach_args *pa)
{
	bus_addr_t start;
	bus_size_t size;
	extern struct extent *iomem_ex;
	struct extent *ex = iomem_ex;

#if defined(PCI_ADDR_FIXUP)
	if (pciaddr.extent_mem != NULL)
		ex = pciaddr.extent_mem;
#endif

	start = ex->ex_start;

	/*
	 * XXX: unfortunately, iomem_ex cannot be used for the dynamic
	 * bus_space allocation.  There are some hidden memory (or
	 * some obstacles which are not recognised by the kernel) in
	 * the region governed by iomem_ex.  So I decide to use only
	 * very high address region.
	 *
	 * If pcibios_addr_fixup() succeeded, the PCI device is using an area
	 * which is not recognised by the kernel as already reserved.
	 */

	if (start < rbus_min_start) 
		start = rbus_min_start;

	size = ex->ex_end - start;
  
	return rbus_new_root_share(pa->pa_memt, ex, start, size, 0);
}


/*
 * rbus_tag_t rbus_pccbb_parent_io(struct pci_attach_args *pa)
 */
rbus_tag_t
rbus_pccbb_parent_io(struct pci_attach_args *pa)
{
	bus_addr_t start;
	bus_size_t size;
	rbus_tag_t ret;
	extern struct extent *ioport_ex;
	struct extent *ex = ioport_ex;

#if defined(PCI_ADDR_FIXUP)
	if (pciaddr.extent_port != NULL)
		ex = pciaddr.extent_port;
#endif

	start = RBUS_IO_BASE;
	size  = RBUS_IO_SIZE;

	ret = rbus_new_root_share(pa->pa_iot, ex, start, size, 0);
	if (ret == NULL)
	  panic("failed to alloc I/O space");

	return ret;
}
