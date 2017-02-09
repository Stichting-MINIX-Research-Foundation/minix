/*	$NetBSD: agpvar.h,v 1.21 2014/11/02 00:05:03 christos Exp $	*/

/*-
 * Copyright (c) 2000 Doug Rabson
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$FreeBSD: src/sys/pci/agppriv.h,v 1.3 2000/07/12 10:13:04 dfr Exp $
 */

#ifndef _PCI_AGPVAR_H_
#define _PCI_AGPVAR_H_

#include <sys/mallocvar.h>
#include <sys/mutex.h>

struct agpbus_attach_args {
	char	*_apa_busname; /* XXX placeholder */
	struct pci_attach_args apa_pci_args;
};

/*
 * The AGP chipset can be acquired by user or kernel code. If the
 * chipset has already been acquired, it cannot be acquired by another
 * user until the previous user has released it.
 */
enum agp_acquire_state {
	AGP_ACQUIRE_FREE,
	AGP_ACQUIRE_USER,
	AGP_ACQUIRE_KERNEL
};

/*
 * This structure is used to query the state of the AGP system.
 */
struct agp_info {
	u_int32_t	ai_mode;
	bus_addr_t	ai_aperture_base;
	bus_size_t	ai_aperture_size;
	vsize_t		ai_memory_allowed;
	vsize_t		ai_memory_used;
	u_int32_t	ai_devid;
};

struct agp_memory_info {
	vsize_t		ami_size;	/* size in bytes */
	bus_addr_t	ami_physical;	/* bogus hack for i810 */
	off_t		ami_offset;	/* page offset if bound */
	int		ami_is_bound;	/* non-zero if bound */
};

#ifdef AGP_DEBUG
#define AGP_DPF(x) do {			\
    printf("agp: ");				\
    printf x;				\
} while (0) 
#else
#define AGP_DPF(x) 
#endif

#define AGPUNIT(x)	minor(x)

/*
 * Data structure to describe an AGP memory allocation.
 */
TAILQ_HEAD(agp_memory_list, agp_memory);
struct agp_memory {
	TAILQ_ENTRY(agp_memory) am_link;	/* wiring for the tailq */
	int		am_id;			/* unique id for block */
	vsize_t		am_size;		/* number of bytes allocated */
	int		am_type;		/* chipset specific type */
	off_t		am_offset;		/* page offset if bound */
	int		am_is_bound;		/* non-zero if bound */
	bus_addr_t	  am_physical;
	void *		  am_virtual;
	bus_dmamap_t	  am_dmamap;
	bus_dma_segment_t *am_dmaseg;
	int		  am_nseg;
};

struct agp_softc;

struct agp_methods {
	u_int32_t (*get_aperture)(struct agp_softc *);
	int (*set_aperture)(struct agp_softc *, u_int32_t);
	int (*bind_page)(struct agp_softc *, off_t, bus_addr_t);
	int (*unbind_page)(struct agp_softc *, off_t);
	void (*flush_tlb)(struct agp_softc *);
	int (*enable)(struct agp_softc *, u_int32_t mode);
	struct agp_memory *(*alloc_memory)(struct agp_softc *, int, vsize_t);
	int (*free_memory)(struct agp_softc *, struct agp_memory *);
	int (*bind_memory)(struct agp_softc *, struct agp_memory *, off_t);
	int (*unbind_memory)(struct agp_softc *, struct agp_memory *);
};

#define AGP_GET_APERTURE(sc)	 ((sc)->as_methods->get_aperture(sc))
#define AGP_SET_APERTURE(sc,a)	 ((sc)->as_methods->set_aperture((sc),(a)))
#define AGP_BIND_PAGE(sc,o,p)	 ((sc)->as_methods->bind_page((sc),(o),(p)))
#define AGP_UNBIND_PAGE(sc,o)	 ((sc)->as_methods->unbind_page((sc), (o)))
#define AGP_FLUSH_TLB(sc)	 ((sc)->as_methods->flush_tlb(sc))
#define AGP_ENABLE(sc,m)	 ((sc)->as_methods->enable((sc),(m)))
#define AGP_ALLOC_MEMORY(sc,t,s) ((sc)->as_methods->alloc_memory((sc),(t),(s)))
#define AGP_FREE_MEMORY(sc,m)	 ((sc)->as_methods->free_memory((sc),(m)))
#define AGP_BIND_MEMORY(sc,m,o)	 ((sc)->as_methods->bind_memory((sc),(m),(o)))
#define AGP_UNBIND_MEMORY(sc,m)	 ((sc)->as_methods->unbind_memory((sc),(m)))

/*
 * All chipset drivers must have this at the start of their softc.
 */
struct agp_softc {
	device_t		as_dev;
	bus_space_tag_t		as_apt;
	int			as_capoff;
	bus_addr_t		as_apaddr;
	bus_size_t		as_apsize;
	int			as_apflags;
	bus_dma_tag_t		as_dmat;
	u_int32_t		as_maxmem;	/* allocation upper bound */
	u_int32_t		as_allocated;	/* amount allocated */
	enum agp_acquire_state	as_state;
	struct agp_memory_list	as_memory;	/* list of allocated memory */
	int			as_nextid;	/* next memory block id */
	int			as_isopen;	/* user device is open */
#if 0
	dev_t			as_devnode;	/* from make_dev */
#endif
	kmutex_t		as_mtx;		/* mutex for access to GATT */
	struct agp_methods	*as_methods;	/* chipset-dependent API */
	void			*as_chipc;	/* chipset-dependent state */
	pci_chipset_tag_t	as_pc;
	pcitag_t		as_tag;
	pcireg_t		as_id;
};

struct agp_gatt {
	u_int32_t	  ag_entries;
	u_int32_t        *ag_virtual;
	bus_addr_t	  ag_physical;
	bus_dmamap_t	  ag_dmamap;
	bus_dma_segment_t ag_dmaseg;
	size_t		  ag_size;
};

int agpbusprint(void *, const char *);

/*
 * Functions private to the AGP code.
 */
void agp_flush_cache(void);
int agp_find_caps(pci_chipset_tag_t, pcitag_t);
int agp_map_aperture(struct pci_attach_args *, struct agp_softc *, int);
struct agp_gatt *agp_alloc_gatt(struct agp_softc *);
void agp_free_gatt(struct agp_softc *, struct agp_gatt *);
int agp_generic_attach(struct agp_softc *);
int agp_generic_detach(struct agp_softc *);
int agp_generic_enable(struct agp_softc *, u_int32_t);
struct agp_memory *agp_generic_alloc_memory(struct agp_softc *, int, vsize_t);
int agp_generic_free_memory(struct agp_softc *, struct agp_memory *);
int agp_generic_bind_memory(struct agp_softc *, struct agp_memory *, off_t);
int agp_generic_bind_memory_bounded(struct agp_softc *, struct agp_memory *,
	off_t, off_t, off_t);
int agp_generic_unbind_memory(struct agp_softc *, struct agp_memory *);

/* The vendor has already been matched when these functions are called */
int agp_amd_match(const struct pci_attach_args *);
int agp_amd64_match(const struct pci_attach_args *);

int agp_ali_attach(device_t, device_t, void *);
int agp_amd_attach(device_t, device_t, void *);
int agp_apple_attach(device_t, device_t, void *);
int agp_i810_attach(device_t, device_t, void *);
int agp_intel_attach(device_t, device_t, void *);
int agp_via_attach(device_t, device_t, void *);
int agp_sis_attach(device_t, device_t, void *);
int agp_amd64_attach(device_t, device_t, void *);

int agp_alloc_dmamem(bus_dma_tag_t, size_t, int, bus_dmamap_t *, void **,
		     bus_addr_t *, bus_dma_segment_t *, int, int *);
void agp_free_dmamem(bus_dma_tag_t, size_t, bus_dmamap_t, void *,
		     bus_dma_segment_t *, int) ;

MALLOC_DECLARE(M_AGP);

/*
 * Kernel API
 */
/*
 * Find the AGP device and return it.
 */
void *agp_find_device(int);

/*
 * Return the current owner of the AGP chipset.
 */
enum agp_acquire_state agp_state(void *);

/*
 * Query the state of the AGP system.
 */
void agp_get_info(void *, struct agp_info *);

/*
 * Acquire the AGP chipset for use by the kernel. Returns EBUSY if the
 * AGP chipset is already acquired by another user.
 */
int agp_acquire(void *);

/*
 * Release the AGP chipset.
 */
int agp_release(void *);

/*
 * Enable the agp hardware with the relavent mode. The mode bits are
 * defined in <dev/pci/agpreg.h>
 */
int agp_enable(void *, u_int32_t);

/*
 * Allocate physical memory suitable for mapping into the AGP
 * aperture.  The value returned is an opaque handle which can be
 * passed to agp_bind(), agp_unbind() or agp_deallocate().
 */
void *agp_alloc_memory(void *, int, vsize_t);

/*
 * Free memory which was allocated with agp_allocate().
 */
void agp_free_memory(void *, void *);

/*
 * Bind memory allocated with agp_allocate() at a given offset within
 * the AGP aperture. Returns EINVAL if the memory is already bound or
 * the offset is not at an AGP page boundary.
 */
int agp_bind_memory(void *, void *, off_t);

/*
 * Unbind memory from the AGP aperture. Returns EINVAL if the memory
 * is not bound.
 */
int agp_unbind_memory(void *, void *);

/*
 * Retrieve information about a memory block allocated with
 * agp_alloc_memory().
 */
void agp_memory_info(void *, void *, struct agp_memory_info *);

/*
 * XXX horrible hack to allow drm code to use our mapping
 * of VGA chip registers
 */
int agp_i810_borrow(bus_addr_t, bus_size_t, bus_space_handle_t *);

#endif /* !_PCI_AGPPRIV_H_ */
