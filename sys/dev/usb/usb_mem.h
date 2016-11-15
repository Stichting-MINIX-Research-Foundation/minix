/*	$NetBSD: usb_mem.h,v 1.30 2013/01/23 23:44:30 jmcneill Exp $	*/
/*	$FreeBSD: src/sys/dev/usb/usb_mem.h,v 1.9 1999/11/17 22:33:47 n_hibma Exp $	*/

/*
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net) at
 * Carlstedt Research & Technology.
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

typedef struct usb_dma_block {
	bus_dma_tag_t tag;
	bus_dmamap_t map;
        void *kaddr;
        bus_dma_segment_t *segs;
        int nsegs;
        int nsegs_alloc;
        size_t size;
        size_t align;
	int flags;
#define USB_DMA_FULLBLOCK	0x0001
#define USB_DMA_RESERVE		0x0002
	LIST_ENTRY(usb_dma_block) next;
} usb_dma_block_t;

#define USBMALLOC_MULTISEG	1

usbd_status	usb_allocmem(usbd_bus_handle, size_t, size_t, usb_dma_t *);
usbd_status	usb_allocmem_flags(usbd_bus_handle, size_t, size_t, usb_dma_t *,
			int);
void		usb_freemem(usbd_bus_handle, usb_dma_t *);
void		usb_syncmem(usb_dma_t *, bus_addr_t, bus_size_t, int ops);

bus_addr_t	usb_dmaaddr(usb_dma_t *, unsigned int);

#define DMAADDR(dma, o) usb_dmaaddr((dma), (o))
#define KERNADDR(dma, o) \
	((void *)((char *)(dma)->block->kaddr + (dma)->offs + (o)))

struct extent;

struct usb_dma_reserve {
	bus_dma_tag_t dtag;
	bus_dmamap_t map;
	void *vaddr;
	bus_addr_t paddr;
	size_t size;
	struct extent *extent;
	device_t dv;
};

#if defined(_KERNEL_OPT)
#include "opt_usb_mem_reserve.h"
#endif

#ifndef USB_MEM_RESERVE
#define USB_MEM_RESERVE (256 * 1024)
#endif

usbd_status usb_reserve_allocm(struct usb_dma_reserve *, usb_dma_t *,
				u_int32_t);
int usb_setup_reserve(device_t, struct usb_dma_reserve *, bus_dma_tag_t, size_t);
void usb_reserve_freem(struct usb_dma_reserve *, usb_dma_t *);
