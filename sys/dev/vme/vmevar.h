/* $NetBSD: vmevar.h,v 1.14 2012/10/27 17:18:38 chs Exp $ */

/*
 * Copyright (c) 1999
 *	Matthias Drochner.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
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
 *
 */

#ifndef _VMEVAR_H_
#define _VMEVAR_H_

typedef u_int32_t vme_addr_t, vme_size_t;
typedef int vme_am_t;

typedef enum {
	VME_D8 = 1,
	VME_D16 = 2,
	VME_D32 = 4
} vme_datasize_t;

typedef int vme_swap_t; /* hardware swap capabilities,
			 placeholder - contents to be specified */

#ifdef _KERNEL

/*
 * Generic placeholder for any resources needed for a mapping,
 * overloaded by bus interface driver
 */
typedef void *vme_mapresc_t;

/* Describes interrupt mapping, opaque to MI drivers */
typedef void *vme_intr_handle_t;

/*
 * Tag structure passed to VME bus devices;
 * contains the bus dependent functions, accessed via macros below.
 */
typedef struct vme_chipset_tag {
	void *cookie;

	int (*vct_map)(void *, vme_addr_t, vme_size_t,
			vme_am_t, vme_datasize_t, vme_swap_t,
			bus_space_tag_t *, bus_space_handle_t *,
			vme_mapresc_t *);
	void (*vct_unmap)(void *, vme_mapresc_t);

	int (*vct_probe)(void *, vme_addr_t, vme_size_t,
			 vme_am_t, vme_datasize_t,
			 int (*)(void *, bus_space_tag_t, bus_space_handle_t),
			      void *);

	int (*vct_int_map)(void *, int, int, vme_intr_handle_t *);
	const struct evcnt *(*vct_int_evcnt)(void *, vme_intr_handle_t);
	void *(*vct_int_establish)(void *, vme_intr_handle_t, int,
					int (*)(void *), void *);
	void (*vct_int_disestablish)(void *, void *);

	int (*vct_dmamap_create)(void *, vme_size_t,
				 vme_am_t, vme_datasize_t, vme_swap_t,
				 int, vme_size_t, vme_addr_t,
				 int, bus_dmamap_t *);
	void (*vct_dmamap_destroy)(void *, bus_dmamap_t);

	/*
	 * This sucks: we have to give all the VME specific arguments
	 * twice - for dmamem_alloc and for dmamem_create. Perhaps
	 * give a "dmamap" argument here, meaning: "allocate memory which
	 * can be accessed through this DMA map".
	 */
	int (*vct_dmamem_alloc)(void *, vme_size_t,
				vme_am_t, vme_datasize_t, vme_swap_t,
				bus_dma_segment_t *, int, int *, int);
	void (*vct_dmamem_free)(void *, bus_dma_segment_t *, int);

	struct vmebus_softc *bus;
} *vme_chipset_tag_t;

/*
 * map / unmap: map VME address ranges into kernel address space
 * XXX should have mapping to CPU only to allow user mmap() without
 *     wasting kvm
 */
#define vme_space_map(vc, vmeaddr, len, am, datasize, swap, tag, handle, resc) \
	(*((vc)->vct_map))((vc)->cookie, (vmeaddr), (len), (am), (datasize), \
			   (swap), (tag), (handle), (resc))
#define vme_space_unmap(vc, resc) \
	(*((vc)->vct_unmap))((vc)->cookie, (resc))

/*
 * probe: check readability or call callback.
 */
#define vme_probe(vc, vmeaddr, len, am, datasize, callback, cbarg) \
	(*((vc)->vct_probe))((vc)->cookie, (vmeaddr), (len), (am), \
			     (datasize), (callback), (cbarg))

/*
 * install / deinstall VME interrupt handler.
 */
#define vme_intr_map(vc, level, vector, handlep) \
	(*((vc)->vct_int_map))((vc)->cookie, (level), (vector), (handlep))
#define vme_intr_evcnt(vc, handle) \
	(*((vc)->vct_int_evcnt))((vc)->cookie, (handle))
#define vme_intr_establish(vc, handle, prio, func, arg) \
	(*((vc)->vct_int_establish))((vc)->cookie, \
		(handle), (prio), (func), (arg))
#define vme_intr_disestablish(vc, cookie) \
	(*((vc)->vct_int_unmap))((vc)->cookie, (cookie))

/*
 * Create DMA map (which is later used by bus independent DMA functions).
 */
#define vme_dmamap_create(vc, size, am, datasize, swap, nsegs, segsz, bound, \
			  flags, map) \
	(*((vc)->vct_dmamap_create))((vc)->cookie, (size), (am), (datasize), \
		(swap), (nsegs), (segsz), (bound), (flags), (map))
#define vme_dmamap_destroy(vc, map) \
	(*((vc)->vct_dmamap_destroy))((vc)->cookie, (map))

/*
 * Allocate memory directly accessible from VME.
 */
#define vme_dmamem_alloc(vc, size, am, datasize, swap, \
  segs, nsegs, rsegs, flags) \
  (*((vc)->vct_dmamem_alloc))((vc)->cookie, (size), (am), (datasize), (swap), \
  (segs), (nsegs), (rsegs), (flags))
#define vme_dmamem_free(vc, segs, nsegs) \
  (*((vc)->vct_dmamem_free))((vc)->cookie, (segs), (nsegs))

/*
 * Autoconfiguration data structures.
 */

struct vme_attach_args;
typedef void (*vme_slaveconf_callback)(device_t,
				       struct vme_attach_args *);

struct vmebus_attach_args {
	vme_chipset_tag_t va_vct;
	bus_dma_tag_t va_bdt;

	vme_slaveconf_callback va_slaveconfig;
};

struct extent;

struct vmebus_softc {
	vme_chipset_tag_t sc_vct;
	bus_dma_tag_t sc_bdt;

	vme_slaveconf_callback slaveconfig;

	struct extent *vme32ext, *vme24ext, *vme16ext;
};

#define VME_MAXCFRANGES 3

struct vme_range {
	vme_addr_t offset;
	vme_size_t size;
	vme_am_t am;
};

struct vme_attach_args {
	vme_chipset_tag_t va_vct;
	bus_dma_tag_t va_bdt;

	int ivector, ilevel;
	int numcfranges;
	struct vme_range r[VME_MAXCFRANGES];
};

/*
 * Address space accounting.
 */
int _vme_space_alloc(struct vmebus_softc *, vme_addr_t, vme_size_t, vme_am_t);
void _vme_space_free(struct vmebus_softc *, vme_addr_t, vme_size_t, vme_am_t);
int _vme_space_get(struct vmebus_softc *, vme_size_t, vme_am_t,
		   u_long, vme_addr_t*);

#define vme_space_alloc(tag, addr, size, ams) \
	_vme_space_alloc(tag->bus, addr, size, ams)

#define vme_space_free(tag, addr, size, ams) \
	_vme_space_free(tag->bus, addr, size, ams)

#define vme_space_get(tag, size, ams, align, addr) \
	_vme_space_get(tag->bus, size, ams, align, addr)

#endif /* KERNEL */
#endif /* _VMEVAR_H_ */
