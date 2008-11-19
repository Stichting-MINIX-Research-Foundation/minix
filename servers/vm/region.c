
#define _SYSTEM 1

#include <minix/com.h>
#include <minix/callnr.h>
#include <minix/type.h>
#include <minix/config.h>
#include <minix/const.h>
#include <minix/sysutil.h>
#include <minix/syslib.h>

#include <sys/mman.h>

#include <limits.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <stdint.h>
#include <memory.h>

#include "vm.h"
#include "proto.h"
#include "util.h"
#include "glo.h"
#include "region.h"
#include "sanitycheck.h"

FORWARD _PROTOTYPE(int map_new_physblock, (struct vmproc *vmp,
	struct vir_region *region, vir_bytes offset, vir_bytes length,
	phys_bytes what, struct phys_region *physhint));

FORWARD _PROTOTYPE(int map_copy_ph_block, (struct vmproc *vmp, struct vir_region *region, struct phys_region *ph));
FORWARD _PROTOTYPE(struct vir_region *map_copy_region, (struct vir_region *));

#if SANITYCHECKS

FORWARD _PROTOTYPE(void map_printmap, (struct vmproc *vmp));

PRIVATE char *map_name(struct vir_region *vr)
{
	int type = vr->flags & (VR_ANON|VR_DIRECT);
	switch(type) {
		case VR_ANON:
			return "anonymous";
		case VR_DIRECT:
			return "direct";
		default:
			vm_panic("unknown mapping type", type);
	}

	return "NOTREACHED";
}


/*===========================================================================*
 *				map_printmap				     *
 *===========================================================================*/
PRIVATE void map_printmap(vmp)
struct vmproc *vmp;
{
	struct vir_region *vr;
	printf("%d:\n", vmp->vm_endpoint);
	for(vr = vmp->vm_regions; vr; vr = vr->next) {
		struct phys_region *ph;
		printf("\t0x%08lx - 0x%08lx: %s  (0x%lx)\n",
			vr->vaddr, vr->vaddr + vr->length, map_name(vr), vr);
		for(ph = vr->first; ph; ph = ph->next) {
			printf("0x%lx-0x%lx(%d) ",
				vr->vaddr + ph->ph->offset,
				vr->vaddr + ph->ph->offset + ph->ph->length,
				ph->ph->refcount);
		}
		printf("\n");
	}
}

/*===========================================================================*
 *				map_sanitycheck			     *
 *===========================================================================*/
PUBLIC void map_sanitycheck(char *file, int line)
{
	struct vmproc *vmp;

/* Macro for looping over all physical blocks of all regions of
 * all processes.
 */
#define ALLREGIONS(regioncode, physcode)			\
	for(vmp = vmproc; vmp <= &vmproc[_NR_PROCS]; vmp++) {	\
		struct vir_region *vr;				\
		if(!(vmp->vm_flags & VMF_INUSE))		\
			continue;				\
		for(vr = vmp->vm_regions; vr; vr = vr->next) {	\
			struct phys_region *pr;			\
			regioncode;				\
			for(pr = vr->first; pr; pr = pr->next) { \
				physcode;			\
			}					\
		}						\
	}

	/* Do counting for consistency check. */
	ALLREGIONS(;,pr->ph->seencount = 0;);
	ALLREGIONS(;,pr->ph->seencount++;);

	/* Do consistency check. */
	ALLREGIONS(if(vr->next) {
			MYASSERT(vr->vaddr < vr->next->vaddr);
			MYASSERT(vr->vaddr + vr->length <= vr->next->vaddr);
		}
		MYASSERT(!(vr->vaddr % VM_PAGE_SIZE));,	
		if(pr->ph->refcount != pr->ph->seencount) {
			map_printmap(vmp);
			printf("ph in vr 0x%lx: 0x%lx-0x%lx  refcount %d "
				"but seencount %lu\n", 
				vr, pr->ph->offset,
				pr->ph->offset + pr->ph->length,
				pr->ph->refcount, pr->ph->seencount);
		}
		MYASSERT(pr->ph->refcount == pr->ph->seencount);
		MYASSERT(!(pr->ph->offset % VM_PAGE_SIZE));
		MYASSERT(!(pr->ph->length % VM_PAGE_SIZE)););
}
#endif


/*=========================================================================*
 *				map_ph_writept				*
 *=========================================================================*/
PUBLIC int map_ph_writept(struct vmproc *vmp, struct vir_region *vr,
	struct phys_block *pb, int *ropages, int *rwpages)
{
	int rw;

	vm_assert(!(vr->vaddr % VM_PAGE_SIZE));
	vm_assert(!(pb->length % VM_PAGE_SIZE));
	vm_assert(!(pb->offset % VM_PAGE_SIZE));
	vm_assert(pb->refcount > 0);

	if((vr->flags & VR_WRITABLE)
	&& (pb->refcount == 1 || (vr->flags & VR_DIRECT)))
		rw = PTF_WRITE;
	else
		rw = 0;

#if SANITYCHECKS
	if(rwpages && ropages && (vr->flags & VR_ANON)) {
		int pages;
		pages = pb->length / VM_PAGE_SIZE;
		if(rw)
			(*rwpages) += pages;
		else
			(*ropages) += pages;
	}
#endif

	if(pt_writemap(&vmp->vm_pt, vr->vaddr + pb->offset,
	  pb->phys, pb->length, PTF_PRESENT | PTF_USER | rw,
		WMF_OVERWRITE) != OK) {
	    printf("VM: map_writept: pt_writemap failed\n");
	    return ENOMEM;
	}

	return OK;
}

/*===========================================================================*
 *				map_region				     *
 *===========================================================================*/
PUBLIC struct vir_region *map_page_region(vmp, minv, maxv, length,
	what, flags, mapflags)
struct vmproc *vmp;
vir_bytes minv;
vir_bytes maxv;
vir_bytes length;
vir_bytes what;
u32_t flags;
int mapflags;
{
	struct vir_region *vr, *prevregion = NULL, *newregion,
		*firstregion = vmp->vm_regions;
	vir_bytes startv;
	int foundflag = 0;

	SANITYCHECK(SCL_FUNCTIONS);

	/* We must be in paged mode to be able to do this. */
	vm_assert(vm_paged);

	/* Length must be reasonable. */
	vm_assert(length > 0);

	/* Special case: allow caller to set maxv to 0 meaning 'I want
	 * it to be mapped in right here.'
	 */
        if(maxv == 0) {
                maxv = minv + length;

                /* Sanity check. */
                if(maxv <= minv) {
                        printf("map_page_region: minv 0x%lx and bytes 0x%lx\n",
                                minv, length);
                        return NULL;
                }
        }

	/* Basic input sanity checks. */
	vm_assert(!(length % VM_PAGE_SIZE));
	if(minv >= maxv) {
		printf("VM: 1 minv: 0x%lx maxv: 0x%lx length: 0x%lx\n",
			minv, maxv, length);
	}
	vm_assert(minv < maxv);
	vm_assert(minv + length <= maxv);

#define FREEVRANGE(rangestart, rangeend, foundcode) {		\
	vir_bytes frstart = (rangestart), frend = (rangeend);	\
	frstart = MAX(frstart, minv);				\
	frend   = MIN(frend, maxv);				\
	if(frend > frstart && (frend - frstart) >= length) {	\
		startv = frstart;				\
		foundflag = 1;					\
		foundcode;					\
	} }

	/* This is the free virtual address space before the first region. */
	FREEVRANGE(0, firstregion ? firstregion->vaddr : VM_DATATOP, ;);

	if(!foundflag) {
		for(vr = vmp->vm_regions; vr && !foundflag; vr = vr->next) {
			FREEVRANGE(vr->vaddr + vr->length,
			  vr->next ? vr->next->vaddr : VM_DATATOP,
				prevregion = vr;);
		}
	}

	if(!foundflag) {
		printf("VM: map_page_region: no 0x%lx bytes found for %d between 0x%lx and 0x%lx\n",
			length, vmp->vm_endpoint, minv, maxv);
		return NULL;
	}

#if SANITYCHECKS
	if(prevregion) vm_assert(prevregion->vaddr < startv);
#endif

	/* However we got it, startv must be in the requested range. */
	vm_assert(startv >= minv);
	vm_assert(startv < maxv);
	vm_assert(startv + length <= maxv);

	/* Now we want a new region. */
	if(!SLABALLOC(newregion)) {
		printf("VM: map_page_region: allocating region failed\n");
		return NULL;
	}

	/* Fill in node details. */
	newregion->vaddr = startv;
	newregion->length = length;
	newregion->first = NULL;
	newregion->flags = flags;
	newregion->tag = VRT_NONE;

	/* If this is a 'direct' mapping, try to actually map it. */
	if(flags & VR_DIRECT) {
		vm_assert(!(length % VM_PAGE_SIZE));
		vm_assert(!(startv % VM_PAGE_SIZE));
		vm_assert(!newregion->first);
		vm_assert(!(mapflags & MF_PREALLOC));
		if(map_new_physblock(vmp, newregion, 0, length, what, NULL) != OK) {
			printf("VM: map_new_physblock failed\n");
			SLABFREE(newregion);
			return NULL;
		}
		vm_assert(newregion->first);
		vm_assert(!newregion->first->next);
		if(map_ph_writept(vmp, newregion, newregion->first->ph, NULL, NULL) != OK) {
			printf("VM: map_region_writept failed\n");
			SLABFREE(newregion);
			return NULL;
		}
	}

	if((flags & VR_ANON) && (mapflags & MF_PREALLOC)) {
		if(map_handle_memory(vmp, newregion, 0, length, 1) != OK) {
			printf("VM:map_page_region: prealloc failed\n");
			SLABFREE(newregion);
			return NULL;
		}
	}

	/* Link it. */
	if(prevregion) {
		vm_assert(prevregion->vaddr < newregion->vaddr);
		newregion->next = prevregion->next;
		prevregion->next = newregion;
	} else {
		newregion->next = vmp->vm_regions;
		vmp->vm_regions = newregion;
	}

#if SANITYCHECKS
	vm_assert(startv == newregion->vaddr);
	if(newregion->next) {
		vm_assert(newregion->vaddr < newregion->next->vaddr);
	}
#endif

	SANITYCHECK(SCL_FUNCTIONS);

	return newregion;
}


/*===========================================================================*
 *				map_free				     *
 *===========================================================================*/
PRIVATE int map_free(struct vir_region *region)
{
	struct phys_region *pr, *nextpr;

	for(pr = region->first; pr; pr = nextpr) {
		vm_assert(pr->ph->refcount > 0);
		pr->ph->refcount--;
		nextpr = pr->next;
		region->first = nextpr; /* For sanity checks. */
		if(pr->ph->refcount == 0) {
			if(region->flags & VR_ANON) {
				FREE_MEM(ABS2CLICK(pr->ph->phys),
					ABS2CLICK(pr->ph->length));
			} else if(region->flags & VR_DIRECT) {
				; /* No action required. */
			} else {
				vm_panic("strange phys flags", NO_NUM);
			}
			SLABFREE(pr->ph);
		}
		SLABFREE(pr);
	}

	SLABFREE(region);

	return OK;
}

/*========================================================================*
 *				map_free_proc				  *
 *========================================================================*/
PUBLIC int map_free_proc(vmp)
struct vmproc *vmp;
{
	struct vir_region *r, *nextr;

	SANITYCHECK(SCL_FUNCTIONS);

	for(r = vmp->vm_regions; r; r = nextr) {
		nextr = r->next;
		map_free(r);
		vmp->vm_regions = nextr;	/* For sanity checks. */
	}

	vmp->vm_regions = NULL;

	SANITYCHECK(SCL_FUNCTIONS);

	return OK;
}

/*===========================================================================*
 *				map_lookup				     *
 *===========================================================================*/
PUBLIC struct vir_region *map_lookup(vmp, offset)
struct vmproc *vmp;
vir_bytes offset;
{
	struct vir_region *r;

	SANITYCHECK(SCL_FUNCTIONS);

	if(!vmp->vm_regions)
		vm_panic("process has no regions", vmp->vm_endpoint);

	for(r = vmp->vm_regions; r; r = r->next) {
		if(offset >= r->vaddr && offset < r->vaddr + r->length)
			return r;
	}

	SANITYCHECK(SCL_FUNCTIONS);

	return NULL;
}


/*===========================================================================*
 *				map_new_physblock			     *
 *===========================================================================*/
PRIVATE int map_new_physblock(vmp, region, offset, length, what_mem, physhint)
struct vmproc *vmp;
struct vir_region *region;
vir_bytes offset;
vir_bytes length;
phys_bytes what_mem;
struct phys_region *physhint;
{
	struct phys_region *physr, *newphysr;
	struct phys_block *newpb;
	phys_bytes mem_clicks, clicks;
	vir_bytes mem;

	SANITYCHECK(SCL_FUNCTIONS);

	vm_assert(!(length % VM_PAGE_SIZE));
	if(!physhint) physhint = region->first;

	/* Allocate things necessary for this chunk of memory. */
	if(!SLABALLOC(newphysr))
		return ENOMEM;
	if(!SLABALLOC(newpb)) {
		SLABFREE(newphysr);
		return ENOMEM;
	}

	/* Memory for new physical block. */
	clicks = CLICKSPERPAGE * length / VM_PAGE_SIZE;
	if(!what_mem) {
		if((mem_clicks = ALLOC_MEM(clicks, PAF_CLEAR)) == NO_MEM) {
			SLABFREE(newpb);
			SLABFREE(newphysr);
			return ENOMEM;
		}
		mem = CLICK2ABS(mem_clicks);
	} else {
		mem = what_mem;
	}

	/* New physical block. */
	newpb->phys = mem;
	newpb->refcount = 1;
	newpb->offset = offset;
	newpb->length = length;

	/* New physical region. */
	newphysr->ph = newpb;

	/* Update pagetable. */
	vm_assert(!(length % VM_PAGE_SIZE));
	vm_assert(!(newpb->length % VM_PAGE_SIZE));
	if(map_ph_writept(vmp, region, newpb, NULL, NULL) != OK) {
		if(!what_mem)
			FREE_MEM(mem_clicks, clicks);
		SLABFREE(newpb);
		SLABFREE(newphysr);
		return ENOMEM;
	}

	if(!region->first || offset < region->first->ph->offset) {
		/* Special case: offset is before start. */
		if(region->first) {
			vm_assert(offset + length <= region->first->ph->offset);
		}
		newphysr->next = region->first;
		region->first = newphysr;
	} else {
		for(physr = physhint; physr; physr = physr->next) {
			if(!physr->next || physr->next->ph->offset > offset) {
				newphysr->next = physr->next;
				physr->next = newphysr;
				break;
			}
		}

		/* Loop must have put the node somewhere. */
		vm_assert(physr->next == newphysr);
	}

	SANITYCHECK(SCL_FUNCTIONS);

	return OK;
}


/*===========================================================================*
 *				map_copy_ph_block			     *
 *===========================================================================*/
PRIVATE int map_copy_ph_block(vmp, region, ph)
struct vmproc *vmp;
struct vir_region *region;
struct phys_region *ph;
{
	int r;
	phys_bytes newmem, newmem_cl, clicks;
	struct phys_block *newpb;

	SANITYCHECK(SCL_FUNCTIONS);

	/* This is only to be done if there is more than one copy. */
	vm_assert(ph->ph->refcount > 1);

	/* Do actal copy on write; allocate new physblock. */
	if(!SLABALLOC(newpb)) {
		printf("VM: map_copy_ph_block: couldn't allocate newpb\n");
		SANITYCHECK(SCL_FUNCTIONS);
		return ENOMEM;
	}

	clicks = CLICKSPERPAGE * ph->ph->length / VM_PAGE_SIZE;
	vm_assert(CLICK2ABS(clicks) == ph->ph->length);
	if((newmem_cl = ALLOC_MEM(clicks, 0)) == NO_MEM) {
		SLABFREE(newpb);
		return ENOMEM;
	}
	newmem = CLICK2ABS(newmem_cl);

	ph->ph->refcount--;
	vm_assert(ph->ph->refcount > 0);
	newpb->length = ph->ph->length;
	newpb->offset = ph->ph->offset;
	newpb->refcount = 1;
	newpb->phys = newmem;

	/* Copy old memory to new memory. */
	if((r=sys_abscopy(ph->ph->phys, newpb->phys, newpb->length)) != OK) {
		printf("VM: map_copy_ph_block: sys_abscopy failed\n");
		SANITYCHECK(SCL_FUNCTIONS);
		return r;
	}

#if VMSTATS
	vmp->vm_bytecopies += newpb->length;
#endif

	/* Reference new block. */
	ph->ph = newpb;

	/* Check reference counts. */
	SANITYCHECK(SCL_DETAIL);

	/* Update pagetable with new address.
	 * This will also make it writable.
	 */
	r = map_ph_writept(vmp, region, ph->ph, NULL, NULL);
	if(r != OK)
		vm_panic("map_copy_ph_block: map_ph_writept failed", r);

	SANITYCHECK(SCL_FUNCTIONS);

	return OK;
}

/*===========================================================================*
 *				map_pagefault			     *
 *===========================================================================*/
PUBLIC int map_pagefault(vmp, region, offset, write)
struct vmproc *vmp;
struct vir_region *region;
vir_bytes offset;
int write;
{
	vir_bytes virpage;
	struct phys_region *ph;
	int r;

	vm_assert(offset >= 0);
	vm_assert(offset < region->length);

	vm_assert(region->flags & VR_ANON);
	vm_assert(!(region->vaddr % VM_PAGE_SIZE));

	virpage = offset - offset % VM_PAGE_SIZE;

	SANITYCHECK(SCL_FUNCTIONS);

	for(ph = region->first; ph; ph = ph->next)
		if(ph->ph->offset <= offset && offset < ph->ph->offset + ph->ph->length)
			break;

	if(ph) {
		/* Pagefault in existing block. Do copy-on-write. */
		vm_assert(write);
		vm_assert(region->flags & VR_WRITABLE);
		vm_assert(ph->ph->refcount > 0);

		if(ph->ph->refcount == 1)
			r = map_ph_writept(vmp, region, ph->ph, NULL, NULL);
		else
			r = map_copy_ph_block(vmp, region, ph);
	} else {
		/* Pagefault in non-existing block. Map in new block. */
#if 0
		if(!write) {
			printf("VM: read from uninitialized memory by %d\n",
				vmp->vm_endpoint);
		}
#endif
		r = map_new_physblock(vmp, region, virpage, VM_PAGE_SIZE, 0,
			region->first);
	}

	if(r != OK)
		printf("VM: map_pagefault: failed (%d)\n", r);

	SANITYCHECK(SCL_FUNCTIONS);

	return r;
}

/*===========================================================================*
 *				map_handle_memory			     *
 *===========================================================================*/
PUBLIC int map_handle_memory(vmp, region, offset, length, write)
struct vmproc *vmp;
struct vir_region *region;
vir_bytes offset, length;
int write;
{
	struct phys_region *physr;
	int changes = 0;

#define FREE_RANGE_HERE(er1, er2) {					\
	struct phys_region *r1 = (er1), *r2 = (er2);			\
	vir_bytes start = offset, end = offset + length;		\
	if(r1) { start = MAX(start, r1->ph->offset + r1->ph->length); }	\
	if(r2) { end   = MIN(end, r2->ph->offset); }			\
	if(start < end) {						\
		int r;							\
		SANITYCHECK(SCL_DETAIL);				\
		if((r=map_new_physblock(vmp, region, start,		\
			end-start, 0, r1 ? r1 : r2)) != OK) {		\
			SANITYCHECK(SCL_DETAIL);			\
			return r;					\
		}							\
		changes++;						\
	} }

	SANITYCHECK(SCL_FUNCTIONS);

	vm_assert(region->flags & VR_ANON);
	vm_assert(!(region->vaddr % VM_PAGE_SIZE));
	vm_assert(!(offset % VM_PAGE_SIZE));
	vm_assert(!(length % VM_PAGE_SIZE));
	vm_assert(!write || (region->flags & VR_WRITABLE));

	FREE_RANGE_HERE(NULL, region->first);

	for(physr = region->first; physr; physr = physr->next) {
		int r;

		SANITYCHECK(SCL_DETAIL);

		if(write) {
		  vm_assert(physr->ph->refcount > 0);
		  if(physr->ph->refcount > 1) {
			SANITYCHECK(SCL_DETAIL);
			r = map_copy_ph_block(vmp, region, physr);
			if(r != OK) {
				printf("VM: map_handle_memory: no copy\n");
				return r;
			}
			changes++;
			SANITYCHECK(SCL_DETAIL);
		  } else {
			SANITYCHECK(SCL_DETAIL);
			if((r=map_ph_writept(vmp, region, physr->ph, NULL, NULL)) != OK) {
				printf("VM: map_ph_writept failed\n");
				return r;
			}
			changes++;
			SANITYCHECK(SCL_DETAIL);
		  }
		}

		SANITYCHECK(SCL_DETAIL);
		FREE_RANGE_HERE(physr, physr->next);
		SANITYCHECK(SCL_DETAIL);
	}

	SANITYCHECK(SCL_FUNCTIONS);

#if SANITYCHECKS
	if(changes == 0)  {
		vm_panic("no changes?!", changes);
	}
#endif

	return OK;
}

#if SANITYCHECKS
static int countregions(struct vir_region *vr)
{
	int n = 0;
	struct phys_region *ph;
	for(ph = vr->first; ph; ph = ph->next)
		n++;
	return n;
}
#endif

/*===========================================================================*
 *				map_copy_region			     	*
 *===========================================================================*/
PRIVATE struct vir_region *map_copy_region(struct vir_region *vr)
{
	struct vir_region *newvr;
	struct phys_region *ph, *prevph = NULL;
#if SANITYCHECKS
	int cr;
	cr = countregions(vr);
#endif
	if(!SLABALLOC(newvr))
		return NULL;
	*newvr = *vr;
	newvr->first = NULL;
	newvr->next = NULL;

	SANITYCHECK(SCL_FUNCTIONS);

	for(ph = vr->first; ph; ph = ph->next) {
		struct phys_region *newph;
		if(!SLABALLOC(newph)) {
			map_free(newvr);
			return NULL;
		}
		newph->next = NULL;
		newph->ph = ph->ph;
		if(prevph) prevph->next = newph;
		else newvr->first = newph;
		prevph = newph;
		SANITYCHECK(SCL_DETAIL);
		vm_assert(countregions(vr) == cr);
	}

	vm_assert(countregions(vr) == countregions(newvr));

	SANITYCHECK(SCL_FUNCTIONS);

	return newvr;
}

/*=========================================================================*
 *				map_writept				*
 *=========================================================================*/
PUBLIC int map_writept(struct vmproc *vmp)
{
	struct vir_region *vr;
	struct phys_region *ph;
	int ropages = 0, rwpages = 0;

	for(vr = vmp->vm_regions; vr; vr = vr->next)
		for(ph = vr->first; ph; ph = ph->next) {
			map_ph_writept(vmp, vr, ph->ph, &ropages, &rwpages);
		}

	return OK;
}

/*========================================================================*
 *				map_proc_copy			     	  *
 *========================================================================*/
PUBLIC int map_proc_copy(dst, src)
struct vmproc *dst;
struct vmproc *src;
{
	struct vir_region *vr, *prevvr = NULL;
	dst->vm_regions = NULL;

	SANITYCHECK(SCL_FUNCTIONS);
	for(vr = src->vm_regions; vr; vr = vr->next) {
		struct vir_region *newvr;
		struct phys_region *ph;
	SANITYCHECK(SCL_DETAIL);
		if(!(newvr = map_copy_region(vr))) {
			map_free_proc(dst);
	SANITYCHECK(SCL_FUNCTIONS);
			return ENOMEM;
		}
		SANITYCHECK(SCL_DETAIL);
		if(prevvr) { prevvr->next = newvr; }
		else { dst->vm_regions = newvr; }
		for(ph = vr->first; ph; ph = ph->next) {
			vm_assert(ph->ph->refcount > 0);
			ph->ph->refcount++;
			vm_assert(ph->ph->refcount > 1);
		}
		SANITYCHECK(SCL_DETAIL);
		prevvr = newvr;
	SANITYCHECK(SCL_DETAIL);
	}
	SANITYCHECK(SCL_DETAIL);

	map_writept(src);
	map_writept(dst);

	SANITYCHECK(SCL_FUNCTIONS);
	return OK;
}

/*========================================================================*
 *				map_proc_kernel		     	  	*
 *========================================================================*/
PUBLIC struct vir_region *map_proc_kernel(struct vmproc *vmp)
{
	struct vir_region *vr;

	/* We assume these are the first regions to be mapped to
	 * make the function a bit simpler (free all regions on error).
	 */
	vm_assert(!vmp->vm_regions);
	vm_assert(vmproc[VMP_SYSTEM].vm_flags & VMF_INUSE);
	vm_assert(!(KERNEL_TEXT % VM_PAGE_SIZE));
	vm_assert(!(KERNEL_TEXT_LEN % VM_PAGE_SIZE));
	vm_assert(!(KERNEL_DATA % VM_PAGE_SIZE));
	vm_assert(!(KERNEL_DATA_LEN % VM_PAGE_SIZE));

	if(!(vr = map_page_region(vmp, KERNEL_TEXT, 0, KERNEL_TEXT_LEN, 
		KERNEL_TEXT, VR_DIRECT | VR_WRITABLE | VR_NOPF, 0)) ||
	   !(vr = map_page_region(vmp, KERNEL_DATA, 0, KERNEL_DATA_LEN, 
		KERNEL_DATA, VR_DIRECT | VR_WRITABLE | VR_NOPF, 0))) {
		map_free_proc(vmp);
		return NULL;
	}

	return vr; /* Return pointer not useful, just non-NULL. */
}

/*========================================================================*
 *				map_region_extend	     	  	*
 *========================================================================*/
PUBLIC int map_region_extend(struct vir_region *vr, vir_bytes delta)
{
	vir_bytes end;

	vm_assert(vr);
	vm_assert(vr->flags & VR_ANON);
	vm_assert(!(delta % VM_PAGE_SIZE));

	if(!delta) return OK;
	end = vr->vaddr + vr->length;
	vm_assert(end >= vr->vaddr);

	if(end + delta <= end) {
		printf("VM: strange delta 0x%lx\n", delta);
		return ENOMEM;
	}

	if(!vr->next || end + delta <= vr->next->vaddr) {
		vr->length += delta;
		return OK;
	}

	return ENOMEM;
}

/*========================================================================*
 *				map_region_shrink	     	  	*
 *========================================================================*/
PUBLIC int map_region_shrink(struct vir_region *vr, vir_bytes delta)
{
	vm_assert(vr);
	vm_assert(vr->flags & VR_ANON);
	vm_assert(!(delta % VM_PAGE_SIZE));

	printf("VM: ignoring region shrink\n");

	return OK;
}

PUBLIC struct vir_region *map_region_lookup_tag(vmp, tag)
struct vmproc *vmp;
u32_t tag;
{
	struct vir_region *vr;

	for(vr = vmp->vm_regions; vr; vr = vr->next)
		if(vr->tag == tag)
			return vr;

	return NULL;
}

PUBLIC void map_region_set_tag(struct vir_region *vr, u32_t tag)
{
	vr->tag = tag;
}

PUBLIC u32_t map_region_get_tag(struct vir_region *vr)
{
	return vr->tag;
}

/*========================================================================*
 *				map_unmap_region	     	  	*
 *========================================================================*/
PUBLIC int map_unmap_region(struct vmproc *vmp, struct vir_region *region)
{
	struct vir_region *r, *nextr, *prev = NULL;

	SANITYCHECK(SCL_FUNCTIONS);

	for(r = vmp->vm_regions; r; r = r->next) {
		if(r == region)
			break;

		prev = r;
	}

	SANITYCHECK(SCL_DETAIL);

	if(r == NULL)
		vm_panic("map_unmap_region: region not found\n", NO_NUM);

	if(!prev)
		vmp->vm_regions = r->next;
	else
		prev->next = r->next;
	map_free(r);

	SANITYCHECK(SCL_DETAIL);

	if(pt_writemap(&vmp->vm_pt, r->vaddr,
	  0, r->length, 0, WMF_OVERWRITE) != OK) {
	    printf("VM: map_unmap_region: pt_writemap failed\n");
	    return ENOMEM;
	}

	SANITYCHECK(SCL_FUNCTIONS);

	return OK;
}
