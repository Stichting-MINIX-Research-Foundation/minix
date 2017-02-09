/*-
 * Copyright (c) 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Coyote Point Systems, Inc.
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

/*-
 * Copyright (C) 2001-2003 by NBMK Encryption Technologies.
 * All rights reserved.
 *
 * NBMK Encryption Technologies provides no support of any kind for
 * this software.  Questions or concerns about it may be addressed to
 * the members of the relevant open-source community at
 * <tech-crypto@netbsd.org>.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

static char const n8_id[] = "$Id: n8_memory_bsd.c,v 1.6 2012/12/01 11:37:27 mbalmer Exp $";
/*****************************************************************************/
/** @file n8_memory_bsd.c
 *  @brief NetOctaveMemory Services - FreeBSD-specific support routines.
 *
 * This file contains all FreeBSD-specific support routines for the large
 * allocation services used by the driver. The cross-platform memory management
 * code uses these routines.
 *****************************************************************************/

/*****************************************************************************
 * Revision history:
 * 05/12/03 brr   Modified user pools to take advantage of the support for
 *                multiple memory banks.
 * 05/08/03 brr   Dimension arrays to allow of multiple user pools.
 * 05/05/03 brr   Moved memory functions here from helper.c.
 * 04/22/03 brr   Change wait flag passed to contigmalloc to M_WAITOK.
 * 04/22/03 brr   Clean up comments & add debug statements.
 *                Removed redundant parameter from n8_FreeLargeAllocation.
 * 04/21/03 brr   Added support for multiple memory banks.
 * 11/01/02 brr   Correctly deallocate memory resources.
 * 10/25/02 brr   Temporarily comment out call to contigfree().
 * 10/22/02 brr   File created.
 ****************************************************************************/
/** @defgroup NSP2000Driver NSP2000 Device Driver - FreeBSD version.
 */


#include "helper.h"
#include "n8_malloc_common.h"
#include "n8_OS_intf.h"
#include "n8_memory.h"
#include "n8_driver_parms.h"
#include <uvm/uvm.h>
#include <machine/pmap.h>
#include "nsp.h"

extern NspInstance_t NSPDeviceTable_g[];
static unsigned long  MemBaseAddress_g[N8_MEMBANK_MAX + DEF_USER_POOL_BANKS];
static unsigned long  MemTopAddress_g[N8_MEMBANK_MAX + DEF_USER_POOL_BANKS];
static unsigned long  MemSize_g[N8_MEMBANK_MAX + DEF_USER_POOL_BANKS];
static          void *BasePointer_g[N8_MEMBANK_MAX + DEF_USER_POOL_BANKS];
static bus_dmamap_t   DmaMap_g[N8_MEMBANK_MAX + DEF_USER_POOL_BANKS];
static int            Rseg_g[N8_MEMBANK_MAX + DEF_USER_POOL_BANKS];
static bus_dma_segment_t Seg_g[N8_MEMBANK_MAX + DEF_USER_POOL_BANKS];

/*****************************************************************************
 * n8_vmalloc
 *****************************************************************************/
/** @ingroup NSP2000Driver
 * @brief n8_memory - Allocates and clears a buffer.
 *
 * This routine abstracts memory allocation for the functions in the driver.
 *
 * @param size   RO:  Specifies the desired allocation size
 *
 * @return
 *    Virtual address of the memory allocation, NULL if failed.
 *
 *
 * @par Errors:
 *    See return section for error information.
 *****************************************************************************/

void *
n8_vmalloc(unsigned long size)
{
    N8_UmallocHdr_t *m;

    if (size) {
	m = malloc(size, M_DEVBUF, M_WAITOK);

	if (m) {
	    memset(m, 0, sizeof (*m));

	    return m+1;
	}
    }

    return 0;
}

/*****************************************************************************
 * n8_vfree
 *****************************************************************************/
/** @ingroup NSP2000Driver
 * @brief n8_memory - Frees a buffer allocated by n8_vmalloc.
 *
 * This routine abstracts memory allocation for the functions in the driver.
 *
 * @param a   RO:  Specifies the address to free.
 *
 * @return
 *    None.
 *
 *
 * @par Errors:
 *    See return section for error information.
 *****************************************************************************/

void
n8_vfree(void *a)
{
    N8_UmallocHdr_t *m	= a;

    if (m) {
	free(m-1, M_DEVBUF);
    }
}

/*****************************************************************************
 * N8_PhysToVirt
 *****************************************************************************/
/** @ingroup NSP2000Driver
 * @brief N8_PhysToVirt - Converts a physical address to it virtual address
 *
 * @param phys   RO:  Physical address to convert.
 *
 * @return
 *    Virtual address of phys
 *
 *
 * @par Errors:
 *****************************************************************************/

void *
N8_PhysToVirt(unsigned long phys)
{
    unsigned long offset;
    int bankIndex = N8_MEMBANK_QUEUE;
    int ctr;
    void *virtaddr;

    for (ctr = 0; ctr < (N8_MEMBANK_MAX + DEF_USER_POOL_BANKS); ctr++)
    {
       if ((phys >= MemBaseAddress_g[ctr]) && 
           (phys < MemTopAddress_g[ctr]))
       {
	   bankIndex = ctr;
           break;
       }
    }
    if (ctr >= (N8_MEMBANK_MAX + DEF_USER_POOL_BANKS)) {
	printf("N8_PhysToVirt(0x%lx) ran out of banks\n", phys);
    }

    offset = phys - MemBaseAddress_g[bankIndex];

    virtaddr = (void *)(offset + (unsigned long)BasePointer_g[bankIndex]);
    return virtaddr;
}



/*****************************************************************************
 * N8_VirtToPhys
 *****************************************************************************/
/** @ingroup NSP2000Driver
 * @brief Convert a virtual address to a physical address.
 *
 * This routine abstracts the Linux system call to convert a virtual address
 * to a physical address.
 *
 * @param virtaddr   RO:  Specifies the physical address.
 *
 * @par Externals:
 *    N/A
 *
 * @return 
 *    Returns the corresponding physical address.
 *
 * @par Errors:
 *    See return section for error information.
 *****************************************************************************/

unsigned long N8_VirtToPhys(void *virtaddr)
{
     paddr_t phys_addr;
     pmap_extract(pmap_kernel(), (unsigned long)virtaddr, &phys_addr);
     return phys_addr;
}


/*****************************************************************************
 * n8_GetLargeAllocation
 *****************************************************************************/
/** @ingroup NSP2000Driver
 * @brief n8_memory - Allocates a large physically contiguous memory range.
 *
 * This routine allocates the requested large allocation with a call
 * to contigmalloc.
 *
 * @param bankIndex  RO:  Bank type for the memory allocation.
 * @param size       RO:  The allocation size.
 * @param debug      RO:  Dump debug information.
 *
 * @return
 *    Physical address of the memory pool. NULL if failed.
 *
 *
 * @par Errors:
 *    See return section for error information.
 *****************************************************************************/

unsigned long
n8_GetLargeAllocation(N8_MemoryType_t bankIndex,
		      unsigned long size, unsigned char debug)
{

	NspInstance_t	*nip  = &NSPDeviceTable_g[0];	/* can only attach once */
	struct nsp_softc *sc = device_private(nip->dev);

	bus_dma_segment_t seg;
	int rseg;
	void *kva = NULL;

#if 0
	/* Replacement for: */
	m = contigmalloc(size, M_DEVBUF, M_WAITOK,
		     0, 		/* lower acceptable phys addr	*/
		     0xffffffff,	/* upper acceptable phys addr	*/
		     PAGE_SIZE,		/* alignment			*/
		     0);		/* boundary			*/
#endif
	if (bus_dmamem_alloc(sc->dma_tag, size, PAGE_SIZE, 0,
	    &seg, 1, &rseg, BUS_DMA_NOWAIT)) {
		printf("%s: can't alloc DMA buffer\n",
		    device_xname(sc->sc_dev));
		return 0;
        }
	if (bus_dmamem_map(sc->dma_tag, &seg, rseg, size, &kva,
	    BUS_DMA_NOWAIT)) {
		printf("%s: can't map DMA buffers (%lu bytes)\n",
		    device_xname(sc->sc_dev), size);
		bus_dmamem_free(sc->dma_tag, &seg, rseg);
		return 0;
	}
	if (bus_dmamap_create(sc->dma_tag, size, 1,
	    size, 0, BUS_DMA_NOWAIT, &DmaMap_g[bankIndex])) {
		printf("%s: can't create DMA map\n", device_xname(sc->sc_dev));
		bus_dmamem_unmap(sc->dma_tag, kva, size);
		bus_dmamem_free(sc->dma_tag, &seg, rseg);
		return 0;
	}
	if (bus_dmamap_load(sc->dma_tag, DmaMap_g[bankIndex], kva, size,
	    NULL, BUS_DMA_NOWAIT)) {
		printf("%s: can't load DMA map\n", device_xname(sc->sc_dev));
		bus_dmamap_destroy(sc->dma_tag, DmaMap_g[bankIndex]);
		bus_dmamem_unmap(sc->dma_tag, kva, size);
		bus_dmamem_free(sc->dma_tag, &seg, rseg);
		return 0;
	}
	if (kva) {
	    /* memset(kva, 0, size) */
	    BasePointer_g[bankIndex]    = kva;
	    MemSize_g[bankIndex]        = size;
	    Seg_g[bankIndex]            = seg;
	    Rseg_g[bankIndex]           = rseg;
	    MemBaseAddress_g[bankIndex] = vtophys((u_int)kva);
	    MemTopAddress_g[bankIndex]  = MemBaseAddress_g[bankIndex] + size;
	}

	if (debug)
	{
	   printf("n8_GetLargeAllocation: %p (0x%08lx) allocated for bankIndex %d\n",
		   BasePointer_g[bankIndex], MemBaseAddress_g[bankIndex], bankIndex);
	}

	return MemBaseAddress_g[bankIndex];
}

/*****************************************************************************
 * n8_FreeLargeAllocation
 *****************************************************************************/
/** @ingroup NSP2000Driver
 * @brief n8_memory - Releases a large physically contiguous memory range.
 *
 * This routine deallocates the large allocation with a call to contigfree.
 *
 * @param bankIndex  RO:  Bank type for the memory free.
 * @param debug      RO:  Dump debug information.
 *
 * @return
 *    N/A
 *
 * @par Errors:
 *    See return section for error information.
 *****************************************************************************/


void
n8_FreeLargeAllocation(N8_MemoryType_t bankIndex,
		       unsigned char   debug) 
{
	NspInstance_t	*nip  = &NSPDeviceTable_g[0];	/* can only attach once */
	struct nsp_softc *sc = device_private(nip->dev);

        printf("n8_FreeLargeAllocation: freeing %p for bankIndex %d\n",
		  BasePointer_g[bankIndex], bankIndex);
	if (debug) {
	  printf("n8_FreeLargeAllocation: freeing %p for bankIndex %d\n",
		  BasePointer_g[bankIndex], bankIndex);
	}

	if (BasePointer_g[bankIndex]) {
	    /* contigfree(BasePointer_g[bankIndex], MemSize_g[bankIndex], M_DEVBUF); */
	    printf("n8_FreeLargeAllocation: bus_dmamap_unload(bank %d) (kva=%p)\n",
		    bankIndex, BasePointer_g[bankIndex]);
	    bus_dmamap_unload(sc->dma_tag, DmaMap_g[bankIndex]);
	    printf("n8_FreeLargeAllocation: bus_dmamap_destroy()\n");
	    bus_dmamap_destroy(sc->dma_tag, DmaMap_g[bankIndex]);
	    printf("n8_FreeLargeAllocation: bus_dmamap_unmap()\n");
	    bus_dmamem_unmap(sc->dma_tag, BasePointer_g[bankIndex], 
		    MemSize_g[bankIndex]);
	    printf("n8_FreeLargeAllocation: bus_dmamap_unmap()\n");
	    bus_dmamem_free(sc->dma_tag, &Seg_g[bankIndex], Rseg_g[bankIndex]);
	}
	BasePointer_g[bankIndex] = NULL;
}

/*****************************************************************************
 * n8_bounds_check
 *****************************************************************************/
/** @ingroup NSP2000Driver
 * @brief n8_bounds_check - Validates address passed to mmap
 *
 * This routine validates address passed to mmap
 *
 * @param phys       RO:  Address of mmap request.
 *
 * @return
 *    N/A
 *
 * @par Errors:
 *    See return section for error information.
 *****************************************************************************/

int
n8_bounds_check(unsigned long phys)
{
    int ctr;

    for (ctr = 1; ctr < (N8_MEMBANK_MAX + DEF_USER_POOL_BANKS); ctr++)
    {
        if ((MemBaseAddress_g[ctr] <= phys) &&
            (phys < (MemBaseAddress_g[ctr] + MemSize_g[ctr])))
        {
            /* Valid mmap request */
            return 1;
        }
    }

    return 0;
}


