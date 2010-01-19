
#define _SYSTEM 1

#include <minix/com.h>
#include <minix/callnr.h>
#include <minix/type.h>
#include <minix/config.h>
#include <minix/const.h>
#include <minix/sysutil.h>
#include <minix/syslib.h>
#include <minix/debug.h>
#include <minix/bitmap.h>

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
#include "physravl.h"

/* Should a physblock be mapped writable? */
#define WRITABLE(r, pb) \
	(((r)->flags & (VR_DIRECT | VR_SHARED)) ||	\
	 (((r)->flags & VR_WRITABLE) && (pb)->refcount == 1))

FORWARD _PROTOTYPE(struct phys_region *map_new_physblock, (struct vmproc *vmp,
	struct vir_region *region, vir_bytes offset, vir_bytes length,
	phys_bytes what));

FORWARD _PROTOTYPE(int map_ph_writept, (struct vmproc *vmp, struct vir_region *vr,
	struct phys_region *pr));

FORWARD _PROTOTYPE(struct vir_region *map_copy_region, (struct vmproc *vmp, struct vir_region *vr));

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

PUBLIC void map_printregion(struct vmproc *vmp, struct vir_region *vr)
{
	physr_iter iter;
	struct phys_region *ph;
	printf("map_printmap: map_name: %s\n", map_name(vr));
	printf("\t%s (len 0x%lx), %s\n",
		arch_map2str(vmp, vr->vaddr), vr->length,
		map_name(vr));
	printf("\t\tphysblocks:\n");
	physr_start_iter_least(vr->phys, &iter);
	while((ph = physr_get_iter(&iter))) {
		printf("\t\t@ %s (refs %d): phys 0x%lx len 0x%lx\n",
			arch_map2str(vmp, vr->vaddr + ph->offset),
			ph->ph->refcount, ph->ph->phys, ph->ph->length);
		physr_incr_iter(&iter);
	}
}

/*===========================================================================*
 *				map_printmap				     *
 *===========================================================================*/
PUBLIC void map_printmap(vmp)
struct vmproc *vmp;
{
	struct vir_region *vr;

	printf("memory regions in process %d:\n", vmp->vm_endpoint);
	for(vr = vmp->vm_regions; vr; vr = vr->next) {
		map_printregion(vmp, vr);
	}
}


#if SANITYCHECKS


/*===========================================================================*
 *				map_sanitycheck_pt			     *
 *===========================================================================*/
PRIVATE int map_sanitycheck_pt(struct vmproc *vmp,
	struct vir_region *vr, struct phys_region *pr)
{
	struct phys_block *pb = pr->ph;
	int rw;

	if(!(vmp->vm_flags & VMF_HASPT))
		return OK;

	if(WRITABLE(vr, pb))
		rw = PTF_WRITE;
	else
		rw = 0;

	return pt_writemap(&vmp->vm_pt, vr->vaddr + pr->offset,
	  pb->phys, pb->length, PTF_PRESENT | PTF_USER | rw, WMF_VERIFY);
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
	for(vmp = vmproc; vmp < &vmproc[VMP_NR]; vmp++) {	\
		struct vir_region *vr;				\
		if(!(vmp->vm_flags & VMF_INUSE))		\
			continue;				\
		for(vr = vmp->vm_regions; vr; vr = vr->next) {	\
			physr_iter iter;			\
			struct phys_region *pr;			\
			regioncode;				\
			physr_start_iter_least(vr->phys, &iter); \
			while((pr = physr_get_iter(&iter))) {	\
				physcode;			\
				physr_incr_iter(&iter);		\
			}					\
		}						\
	}

#define MYSLABSANE(s) MYASSERT(slabsane_f(__FILE__, __LINE__, s, sizeof(*(s))))
	/* Basic pointers check. */
	ALLREGIONS(MYSLABSANE(vr),MYSLABSANE(pr); MYSLABSANE(pr->ph);MYSLABSANE(pr->parent));
	ALLREGIONS(/* MYASSERT(vr->parent == vmp) */,MYASSERT(pr->parent == vr););

	/* Do counting for consistency check. */
	ALLREGIONS(;,USE(pr->ph, pr->ph->seencount = 0;););
	ALLREGIONS(;,USE(pr->ph, pr->ph->seencount++;);
		if(pr->ph->seencount == 1) {
			MYASSERT(usedpages_add(pr->ph->phys,
				pr->ph->length) == OK);
		}
	);

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
				vr, pr->offset,
				pr->offset + pr->ph->length,
				pr->ph->refcount, pr->ph->seencount);
		}
		{
			int n_others = 0;
			struct phys_region *others;
			if(pr->ph->refcount > 0) {
				MYASSERT(pr->ph->firstregion);
				if(pr->ph->refcount == 1) {
					MYASSERT(pr->ph->firstregion == pr);
				}
			} else {
				MYASSERT(!pr->ph->firstregion);
			}
			for(others = pr->ph->firstregion; others;
				others = others->next_ph_list) {
				MYSLABSANE(others);
				MYASSERT(others->ph == pr->ph);
				n_others++;
			}
			MYASSERT(pr->ph->refcount == n_others);
		}
		MYASSERT(pr->ph->refcount == pr->ph->seencount);
		MYASSERT(!(pr->offset % VM_PAGE_SIZE));
		MYASSERT(!(pr->ph->length % VM_PAGE_SIZE)););
	ALLREGIONS(,MYASSERT(map_sanitycheck_pt(vmp, vr, pr) == OK));
}
#endif


/*=========================================================================*
 *				map_ph_writept				*
 *=========================================================================*/
PRIVATE int map_ph_writept(struct vmproc *vmp, struct vir_region *vr,
	struct phys_region *pr)
{
	int rw;
	struct phys_block *pb = pr->ph;

	vm_assert(!(vr->vaddr % VM_PAGE_SIZE));
	vm_assert(!(pb->length % VM_PAGE_SIZE));
	vm_assert(!(pr->offset % VM_PAGE_SIZE));
	vm_assert(pb->refcount > 0);

	if(WRITABLE(vr, pb))
		rw = PTF_WRITE;
	else
		rw = 0;

	if(pt_writemap(&vmp->vm_pt, vr->vaddr + pr->offset,
	  pb->phys, pb->length, PTF_PRESENT | PTF_USER | rw,
#if SANITYCHECKS
	  	!pr->written ? 0 :
#endif
	  	WMF_OVERWRITE) != OK) {
	    printf("VM: map_writept: pt_writemap failed\n");
	    return ENOMEM;
	}

#if SANITYCHECKS
	USE(pr, pr->written = 1;);
#endif

	return OK;
}

/*===========================================================================*
 *				region_find_slot			     *
 *===========================================================================*/
PRIVATE vir_bytes region_find_slot(struct vmproc *vmp,
		vir_bytes minv, vir_bytes maxv, vir_bytes length,
		struct vir_region **prev)
{
	struct vir_region *firstregion = vmp->vm_regions, *prevregion = NULL;
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
                        printf("region_find_slot: minv 0x%lx and bytes 0x%lx\n",
                                minv, length);
			map_printmap(vmp);
                        return (vir_bytes) -1;
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
		struct vir_region *vr;
		for(vr = vmp->vm_regions; vr && !foundflag; vr = vr->next) {
			FREEVRANGE(vr->vaddr + vr->length,
			  vr->next ? vr->next->vaddr : VM_DATATOP,
				prevregion = vr;);
		}
	}

	if(!foundflag) {
		printf("VM: region_find_slot: no 0x%lx bytes found for %d between 0x%lx and 0x%lx\n",
			length, vmp->vm_endpoint, minv, maxv);
		map_printmap(vmp);
		return (vir_bytes) -1;
	}

#if SANITYCHECKS
	if(prevregion) vm_assert(prevregion->vaddr < startv);
#endif

	/* However we got it, startv must be in the requested range. */
	vm_assert(startv >= minv);
	vm_assert(startv < maxv);
	vm_assert(startv + length <= maxv);

	if (prev)
		*prev = prevregion;
	return startv;
}

/*===========================================================================*
 *				map_page_region				     *
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
	struct vir_region *prevregion = NULL, *newregion;
	vir_bytes startv;
	struct phys_region *ph;
	physr_avl *phavl;

	SANITYCHECK(SCL_FUNCTIONS);

	startv = region_find_slot(vmp, minv, maxv, length, &prevregion);
	if (startv == (vir_bytes) -1)
		return NULL;

	/* Now we want a new region. */
	if(!SLABALLOC(newregion)) {
		printf("VM: map_page_region: allocating region failed\n");
		return NULL;
	}

	/* Fill in node details. */
USE(newregion,
	newregion->vaddr = startv;
	newregion->length = length;
	newregion->flags = flags;
	newregion->tag = VRT_NONE;
	newregion->parent = vmp;);

	SLABALLOC(phavl);
	if(!phavl) {
		printf("VM: map_page_region: allocating phys avl failed\n");
		SLABFREE(newregion);
		return NULL;
	}
	USE(newregion, newregion->phys = phavl;);

	physr_init(newregion->phys);

	/* If we know what we're going to map to, map it right away. */
	if(what != MAP_NONE) {
		struct phys_region *pr;
		vm_assert(!(what % VM_PAGE_SIZE));
		vm_assert(!(length % VM_PAGE_SIZE));
		vm_assert(!(startv % VM_PAGE_SIZE));
		vm_assert(!(mapflags & MF_PREALLOC));
		if(!(pr=map_new_physblock(vmp, newregion, 0, length, what))) {
			printf("VM: map_new_physblock failed\n");
			SLABFREE(newregion->phys);
			SLABFREE(newregion);
			return NULL;
		}
		if(map_ph_writept(vmp, newregion, pr) != OK) {
			printf("VM: map_region_writept failed\n");
			SLABFREE(newregion);
			return NULL;
		}
	}

	if((flags & VR_ANON) && (mapflags & MF_PREALLOC)) {
		if(map_handle_memory(vmp, newregion, 0, length, 1) != OK) {
			printf("VM: map_page_region: prealloc failed\n");
			SLABFREE(newregion->phys);
			SLABFREE(newregion);
			return NULL;
		}
	}

	/* Link it. */
	if(prevregion) {
		vm_assert(prevregion->vaddr < newregion->vaddr);
		USE(newregion, newregion->next = prevregion->next;);
		USE(prevregion, prevregion->next = newregion;);
	} else {
		USE(newregion, newregion->next = vmp->vm_regions;);
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
 *				pb_unreferenced				     *
 *===========================================================================*/
PUBLIC void pb_unreferenced(struct vir_region *region, struct phys_region *pr)
{
	struct phys_block *pb;
	int remap = 0;

	pb = pr->ph;
	vm_assert(pb->refcount > 0);
	USE(pb, pb->refcount--;);
	vm_assert(pb->refcount >= 0);

	if(pb->firstregion == pr) {
		USE(pb, pb->firstregion = pr->next_ph_list;);
	} else {
		struct phys_region *others;

		for(others = pb->firstregion; others;
			others = others->next_ph_list) {
			vm_assert(others->ph == pb);
			if(others->next_ph_list == pr) {
				USE(others, others->next_ph_list = pr->next_ph_list;);
				break;
			}
		}

		vm_assert(others); /* Otherwise, wasn't on the list. */
	}

	if(pb->refcount == 0) {
		vm_assert(!pb->firstregion);
		if(region->flags & VR_ANON) {
			FREE_MEM(ABS2CLICK(pb->phys),
				ABS2CLICK(pb->length));
		} else if(region->flags & VR_DIRECT) {
			; /* No action required. */
		} else {
			vm_panic("strange phys flags", NO_NUM);
		}
		SLABFREE(pb);
	}
}

/*===========================================================================*
 *				map_subfree				     *
 *===========================================================================*/
PRIVATE int map_subfree(struct vmproc *vmp,
	struct vir_region *region, vir_bytes len)
{
	struct phys_region *pr, *nextpr;
	physr_iter iter;

#if SANITYCHECKS
	{
	physr_start_iter_least(region->phys, &iter);
	while((pr = physr_get_iter(&iter))) {
		struct phys_region *others;
		struct phys_block *pb;

		pb = pr->ph;

		for(others = pb->firstregion; others;
			others = others->next_ph_list) {
			vm_assert(others->ph == pb);
		}
		physr_incr_iter(&iter);
	}
	}
#endif

	physr_start_iter_least(region->phys, &iter);
	while((pr = physr_get_iter(&iter))) {
		physr_incr_iter(&iter);
		if(pr->offset >= len)
			break;
		if(pr->offset + pr->ph->length <= len) {
			pb_unreferenced(region, pr);
			physr_remove(region->phys, pr->offset);
			physr_start_iter_least(region->phys, &iter);
			SLABFREE(pr);
		} else {
			vir_bytes sublen;
			vm_assert(len > pr->offset);
			vm_assert(len < pr->offset + pr->ph->length);
			vm_assert(pr->ph->refcount > 0);
			sublen = len - pr->offset;
			vm_assert(!(sublen % VM_PAGE_SIZE));
			vm_assert(sublen < pr->ph->length);
			if(pr->ph->refcount > 1) {
				int r;
				r = map_copy_ph_block(vmp, region, pr);
				if(r != OK)
					return r;
			}
			vm_assert(pr->ph->refcount == 1);
			if(!(region->flags & VR_DIRECT)) {
				FREE_MEM(ABS2CLICK(pr->ph->phys), ABS2CLICK(sublen));
			}
			USE(pr, pr->offset += sublen;);
			USE(pr->ph,
				pr->ph->phys += sublen;
				pr->ph->length -= sublen;);
			vm_assert(!(pr->offset % VM_PAGE_SIZE));
			vm_assert(!(pr->ph->phys % VM_PAGE_SIZE));
			vm_assert(!(pr->ph->length % VM_PAGE_SIZE));
		}
	}

	return OK;
}

/*===========================================================================*
 *				map_free				     *
 *===========================================================================*/
PRIVATE int map_free(struct vmproc *vmp, struct vir_region *region)
{
	int r;

	if((r=map_subfree(vmp, region, region->length)) != OK)
		return r;

	SLABFREE(region->phys);
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
		SANITYCHECK(SCL_DETAIL);
#if SANITYCHECKS
		nocheck++;
#endif
		map_free(vmp, r);
		vmp->vm_regions = nextr;	/* For sanity checks. */
#if SANITYCHECKS
		nocheck--;
#endif
		SANITYCHECK(SCL_DETAIL);
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
PRIVATE struct phys_region *map_new_physblock(vmp, region, offset, length, what_mem)
struct vmproc *vmp;
struct vir_region *region;
vir_bytes offset;
vir_bytes length;
phys_bytes what_mem;
{
	struct phys_region *newphysr;
	struct phys_block *newpb;
	phys_bytes mem_clicks, clicks;
	vir_bytes mem;

	SANITYCHECK(SCL_FUNCTIONS);

	vm_assert(!(length % VM_PAGE_SIZE));

	NOTRUNNABLE(vmp->vm_endpoint);

	/* Allocate things necessary for this chunk of memory. */
	if(!SLABALLOC(newphysr))
		return NULL;
	if(!SLABALLOC(newpb)) {
		SLABFREE(newphysr);
		return NULL;
	}

	/* Memory for new physical block. */
	clicks = CLICKSPERPAGE * length / VM_PAGE_SIZE;
	if(what_mem == MAP_NONE) {
		u32_t af = PAF_CLEAR;
		if(region->flags & VR_PHYS64K)
			af |= PAF_ALIGN64K;
		if(region->flags & VR_LOWER16MB)
			af |= PAF_LOWER16MB;
		if(region->flags & VR_LOWER1MB)
			af |= PAF_LOWER1MB;
		if((mem_clicks = ALLOC_MEM(clicks, af)) == NO_MEM) {
			SLABFREE(newpb);
			SLABFREE(newphysr);
			printf("map_new_physblock: couldn't allocate\n");
			return NULL;
		}
		mem = CLICK2ABS(mem_clicks);
	} else {
		mem = what_mem;
	}
	SANITYCHECK(SCL_DETAIL);

	/* New physical block. */
	USE(newpb,
	newpb->phys = mem;
	newpb->refcount = 1;
	newpb->length = length;
	newpb->firstregion = newphysr;);

	/* New physical region. */
	USE(newphysr,
	newphysr->offset = offset;
	newphysr->ph = newpb;
	newphysr->parent = region;
	newphysr->next_ph_list = NULL;	/* No other references to this block. */);
#if SANITYCHECKS
	USE(newphysr, newphysr->written = 0;);
#endif

	/* Update pagetable. */
	vm_assert(!(length % VM_PAGE_SIZE));
	vm_assert(!(newpb->length % VM_PAGE_SIZE));
	SANITYCHECK(SCL_DETAIL);
	if(map_ph_writept(vmp, region, newphysr) != OK) {
		if(what_mem == MAP_NONE)
			FREE_MEM(mem_clicks, clicks);
		SLABFREE(newpb);
		SLABFREE(newphysr);
		printf("map_new_physblock: map_ph_writept failed\n");
		return NULL;
	}

	physr_insert(region->phys, newphysr);

	SANITYCHECK(SCL_FUNCTIONS);

	return newphysr;
}


/*===========================================================================*
 *				map_copy_ph_block			     *
 *===========================================================================*/
PUBLIC int map_copy_ph_block(vmp, region, ph)
struct vmproc *vmp;
struct vir_region *region;
struct phys_region *ph;
{
	int r;
	phys_bytes newmem, newmem_cl, clicks;
	struct phys_block *newpb;
	u32_t af = 0;

	/* This is only to be done if there is more than one copy. */
	vm_assert(ph->ph->refcount > 1);

	/* Do actual copy on write; allocate new physblock. */
	if(!SLABALLOC(newpb)) {
		printf("VM: map_copy_ph_block: couldn't allocate newpb\n");
		return ENOMEM;
	}

	clicks = CLICKSPERPAGE * ph->ph->length / VM_PAGE_SIZE;
	vm_assert(CLICK2ABS(clicks) == ph->ph->length);
	if(region->flags & VR_PHYS64K)
		af |= PAF_ALIGN64K;

	NOTRUNNABLE(vmp->vm_endpoint);

	if((newmem_cl = ALLOC_MEM(clicks, af)) == NO_MEM) {
		printf("VM: map_copy_ph_block: couldn't allocate new block\n");
		SLABFREE(newpb);
		return ENOMEM;
	}
	newmem = CLICK2ABS(newmem_cl);
	vm_assert(ABS2CLICK(newmem) == newmem_cl);

	pb_unreferenced(region, ph);
	vm_assert(ph->ph->refcount > 0);

USE(newpb,
	newpb->length = ph->ph->length;
	newpb->refcount = 1;
	newpb->phys = newmem;
	newpb->firstregion = ph;);

	USE(ph, ph->next_ph_list = NULL;);

	NOTRUNNABLE(vmp->vm_endpoint);

	/* Copy old memory to new memory. */
	if((r=sys_abscopy(ph->ph->phys, newpb->phys, newpb->length)) != OK) {
		printf("VM: map_copy_ph_block: sys_abscopy failed\n");
		return r;
	}

#if VMSTATS
	vmp->vm_bytecopies += newpb->length;
#endif

	/* Reference new block. */
	USE(ph, ph->ph = newpb;);

	/* Update pagetable with new address.
	 * This will also make it writable.
	 */
	r = map_ph_writept(vmp, region, ph);
	if(r != OK)
		vm_panic("map_copy_ph_block: map_ph_writept failed", r);

	return OK;
}

/*===========================================================================*
 *				map_pf			     *
 *===========================================================================*/
PUBLIC int map_pf(vmp, region, offset, write)
struct vmproc *vmp;
struct vir_region *region;
vir_bytes offset;
int write;
{
	vir_bytes virpage;
	struct phys_region *ph;
	int r = OK;

	vm_assert(offset >= 0);
	vm_assert(offset < region->length);

	vm_assert(region->flags & VR_ANON);
	vm_assert(!(region->vaddr % VM_PAGE_SIZE));

	virpage = offset - offset % VM_PAGE_SIZE;

	SANITYCHECK(SCL_FUNCTIONS);

	NOTRUNNABLE(vmp->vm_endpoint);

	if((ph = physr_search(region->phys, offset, AVL_LESS_EQUAL)) &&
	   (ph->offset <= offset && offset < ph->offset + ph->ph->length)) {
		/* Pagefault in existing block. Do copy-on-write. */
		vm_assert(write);
		vm_assert(region->flags & VR_WRITABLE);
		vm_assert(ph->ph->refcount > 0);

		if(WRITABLE(region, ph->ph)) {
			r = map_ph_writept(vmp, region, ph);
			if(r != OK)
				printf("map_ph_writept failed\n");
		} else {
			if(ph->ph->refcount > 0
				&& ph->ph->share_flag != PBSH_COW) {
				printf("VM: write RO mapped pages.\n");
				return EFAULT;
			} else {
				r = map_copy_ph_block(vmp, region, ph);
			}
		}
	} else {
		/* Pagefault in non-existing block. Map in new block. */
		if(!map_new_physblock(vmp, region, virpage, VM_PAGE_SIZE, MAP_NONE)) {
			printf("map_new_physblock failed\n");
			r = ENOMEM;
		}
	}

	SANITYCHECK(SCL_FUNCTIONS);

	if(r != OK) {
		printf("VM: map_pf: failed (%d)\n", r);
		return r;
	}

#if SANITYCHECKS
	if(OK != pt_checkrange(&vmp->vm_pt, region->vaddr+offset, VM_PAGE_SIZE, write)) {
		vm_panic("map_pf: pt_checkrange failed", r);
	}
#endif	

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
	struct phys_region *physr, *nextphysr;
	int changes = 0;
	physr_iter iter;

	NOTRUNNABLE(vmp->vm_endpoint);

#define FREE_RANGE_HERE(er1, er2) {					\
	struct phys_region *r1 = (er1), *r2 = (er2);			\
	vir_bytes start = offset, end = offset + length;		\
	if(r1) { 	 						\
		start = MAX(start, r1->offset + r1->ph->length); }	\
	if(r2) { 	 						\
		end   = MIN(end, r2->offset); }				\
	if(start < end) {						\
		int r;							\
		SANITYCHECK(SCL_DETAIL);				\
		if(!map_new_physblock(vmp, region, start,		\
			end-start, MAP_NONE) != OK) {			\
			SANITYCHECK(SCL_DETAIL);			\
			return ENOMEM;					\
		}							\
		changes++;						\
	} }


	SANITYCHECK(SCL_FUNCTIONS);

	vm_assert(region->flags & VR_ANON);
	vm_assert(!(region->vaddr % VM_PAGE_SIZE));
	vm_assert(!(offset % VM_PAGE_SIZE));
	vm_assert(!(length % VM_PAGE_SIZE));
	vm_assert(!write || (region->flags & VR_WRITABLE));

	physr_start_iter(region->phys, &iter, offset, AVL_LESS_EQUAL);
	physr = physr_get_iter(&iter);

	if(!physr) {
		physr_start_iter(region->phys, &iter, offset, AVL_GREATER_EQUAL);
		physr = physr_get_iter(&iter);
	}

#define RESET_ITER(it, where, what) {	\
	physr_start_iter(region->phys, &it, where, AVL_EQUAL);	\
	what = physr_get_iter(&it); \
	if(!what)  vm_panic("thing missing", NO_NUM); \
	if(what->offset != where) vm_panic("thing wrong", NO_NUM);	\
}

	FREE_RANGE_HERE(NULL, physr);

	if(physr) {
		RESET_ITER(iter, physr->offset, physr);
		if(physr->offset + physr->ph->length <= offset) {
			physr_incr_iter(&iter);
			physr = physr_get_iter(&iter);

			FREE_RANGE_HERE(NULL, physr);
			if(physr) {
				RESET_ITER(iter, physr->offset, physr);
			}
		}
	}

	while(physr) {
		int r;

		SANITYCHECK(SCL_DETAIL);

		if(write) {
		  vm_assert(physr->ph->refcount > 0);
		  if(!WRITABLE(region, physr->ph)) {
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
			if((r=map_ph_writept(vmp, region, physr)) != OK) {
				printf("VM: map_ph_writept failed\n");
				return r;
			}
			changes++;
			SANITYCHECK(SCL_DETAIL);
		  }
		}

		SANITYCHECK(SCL_DETAIL);
		physr_incr_iter(&iter);
		nextphysr = physr_get_iter(&iter);
		FREE_RANGE_HERE(physr, nextphysr);
		SANITYCHECK(SCL_DETAIL);
	 	if(nextphysr) {
			if(nextphysr->offset >= offset + length)
				break;
			RESET_ITER(iter, nextphysr->offset, nextphysr);
		}
		physr = nextphysr;
	}

	SANITYCHECK(SCL_FUNCTIONS);

	if(changes < 1) {
#if VERBOSE
		printf("region start at 0x%lx offset 0x%lx len 0x%lx write %d\n", 
			region->vaddr, offset, length, write);
		printf("no changes in map_handle_memory\n");
#endif
		return EFAULT;
	}

#if SANITYCHECKS
	if(OK != pt_checkrange(&vmp->vm_pt, region->vaddr+offset, length, write)) {
		printf("handle mem %s-", arch_map2str(vmp, region->vaddr+offset));
		printf("%s failed\n", arch_map2str(vmp, region->vaddr+offset+length));
		map_printregion(vmp, region);
		vm_panic("checkrange failed", NO_NUM);
	}
#endif

	return OK;
}

#if SANITYCHECKS
static int countregions(struct vir_region *vr)
{
	int n = 0;
	struct phys_region *ph;
	physr_iter iter;
	physr_start_iter_least(vr->phys, &iter);
	while((ph = physr_get_iter(&iter))) {
		n++;
		physr_incr_iter(&iter);
	}
	return n;
}
#endif

/*===========================================================================*
 *				map_copy_region			     	*
 *===========================================================================*/
PRIVATE struct vir_region *map_copy_region(struct vmproc *vmp, struct vir_region *vr)
{
	/* map_copy_region creates a complete copy of the vir_region
	 * data structure, linking in the same phys_blocks directly,
	 * but all in limbo, i.e., the caller has to link the vir_region
	 * to a process. Therefore it doesn't increase the refcount in
	 * the phys_block; the caller has to do this once it's linked.
	 * The reason for this is to keep the sanity checks working
	 * within this function.
	 */
	struct vir_region *newvr;
	struct phys_region *ph;
	physr_iter iter;
	physr_avl *phavl;
#if SANITYCHECKS
	int cr;
	cr = countregions(vr);
#endif

	if(!SLABALLOC(newvr))
		return NULL;
	SLABALLOC(phavl);
	if(!phavl) {
		SLABFREE(newvr);
		return NULL;
	}
	USE(newvr,
		*newvr = *vr;
		newvr->next = NULL;
		newvr->phys = phavl;
	);
	physr_init(newvr->phys);

	physr_start_iter_least(vr->phys, &iter);
	while((ph = physr_get_iter(&iter))) {
		struct phys_region *newph;
		if(!SLABALLOC(newph)) {
			map_free(vmp, newvr);
			return NULL;
		}
		USE(newph,
		newph->ph = ph->ph;
		newph->next_ph_list = NULL;
		newph->parent = newvr;
		newph->offset = ph->offset;);
#if SANITYCHECKS
		USE(newph, newph->written = 0;);
#endif
		physr_insert(newvr->phys, newph);
		vm_assert(countregions(vr) == cr);
		physr_incr_iter(&iter);
	}

	vm_assert(countregions(vr) == countregions(newvr));

	return newvr;
}

/*=========================================================================*
 *				map_writept				*
 *=========================================================================*/
PUBLIC int map_writept(struct vmproc *vmp)
{
	struct vir_region *vr;
	struct phys_region *ph;
	int r;

	for(vr = vmp->vm_regions; vr; vr = vr->next) {
		physr_iter iter;
		physr_start_iter_least(vr->phys, &iter);
		while((ph = physr_get_iter(&iter))) {
			physr_incr_iter(&iter);

			/* If this phys block is shared as SMAP, then do
			 * not update the page table. */
			if(ph->ph->refcount > 1
				&& ph->ph->share_flag == PBSH_SMAP) {
				continue;
			}

			if((r=map_ph_writept(vmp, vr, ph)) != OK) {
				printf("VM: map_writept: failed\n");
				return r;
			}
		}
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

	PT_SANE(&src->vm_pt);

	for(vr = src->vm_regions; vr; vr = vr->next) {
		physr_iter iter_orig, iter_new;
		struct vir_region *newvr;
		struct phys_region *orig_ph, *new_ph;
		if(!(newvr = map_copy_region(dst, vr))) {
			map_free_proc(dst);
			return ENOMEM;
		}
		USE(newvr, newvr->parent = dst;);
		if(prevvr) { USE(prevvr, prevvr->next = newvr;); }
		else { dst->vm_regions = newvr; }
		physr_start_iter_least(vr->phys, &iter_orig);
		physr_start_iter_least(newvr->phys, &iter_new);
		while((orig_ph = physr_get_iter(&iter_orig))) {
			struct phys_block *pb;
			new_ph = physr_get_iter(&iter_new);
			/* Check two physregions both are nonnull,
			 * are different, and match physblocks.
			 */
			vm_assert(new_ph);
			vm_assert(orig_ph);
			vm_assert(orig_ph != new_ph);
			pb = orig_ph->ph;
			vm_assert(pb == new_ph->ph);

			/* Link in new physregion. */
			vm_assert(!new_ph->next_ph_list);
			USE(new_ph, new_ph->next_ph_list = pb->firstregion;);
			USE(pb, pb->firstregion = new_ph;);

			/* Increase phys block refcount */
			vm_assert(pb->refcount > 0);
			USE(pb, pb->refcount++;);
			vm_assert(pb->refcount > 1);

			/* If the phys block has been shared as SMAP,
			 * do the regular copy. */
			if(pb->refcount > 2 && pb->share_flag == PBSH_SMAP) {
				map_copy_ph_block(dst, newvr, new_ph);
			} else {
				pb->share_flag = PBSH_COW;
			}

			/* Get next new physregion */
			physr_incr_iter(&iter_orig);
			physr_incr_iter(&iter_new);
		}
		vm_assert(!physr_get_iter(&iter_new));
		prevvr = newvr;
	}

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
PUBLIC int map_region_extend(struct vmproc *vmp, struct vir_region *vr,
	vir_bytes delta)
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
		USE(vr, vr->length += delta;);
		return OK;
	}

	map_printmap(vmp);

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

#if 0
	printf("VM: ignoring region shrink\n");
#endif

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
	USE(vr, vr->tag = tag;);
}

PUBLIC u32_t map_region_get_tag(struct vir_region *vr)
{
	return vr->tag;
}

/*========================================================================*
 *				map_unmap_region	     	  	*
 *========================================================================*/
PUBLIC int map_unmap_region(struct vmproc *vmp, struct vir_region *region,
	vir_bytes len)
{
/* Shrink the region by 'len' bytes, from the start. Unreference
 * memory it used to reference if any.
 */
	struct vir_region *r, *nextr, *prev = NULL;
	vir_bytes regionstart;

	SANITYCHECK(SCL_FUNCTIONS);

	for(r = vmp->vm_regions; r; r = r->next) {
		if(r == region)
			break;

		prev = r;
	}

	SANITYCHECK(SCL_DETAIL);

	if(r == NULL)
		vm_panic("map_unmap_region: region not found\n", NO_NUM);

	if(len > r->length || (len % VM_PAGE_SIZE)) {
		printf("VM: bogus length 0x%lx\n", len);
		return EINVAL;
	}

	if(!(r->flags & (VR_ANON|VR_DIRECT))) {
		printf("VM: only unmap anonymous or direct memory\n");
		return EINVAL;
	}

	regionstart = r->vaddr;

	if(len == r->length) {
		/* Whole region disappears. Unlink and free it. */
		if(!prev) {
			vmp->vm_regions = r->next;
		} else {
			USE(prev, prev->next = r->next;);
		}
		map_free(vmp, r);
	} else {
		struct phys_region *pr;
		physr_iter iter;
		/* Region shrinks. First unreference its memory
		 * and then shrink the region.
		 */
		map_subfree(vmp, r, len);
		USE(r,
		r->vaddr += len;
		r->length -= len;);
		physr_start_iter_least(r->phys, &iter);

		/* vaddr has increased; to make all the phys_regions
		 * point to the same addresses, make them shrink by the
		 * same amount.
		 */
		while((pr = physr_get_iter(&iter))) {
			vm_assert(pr->offset >= len);
			USE(pr, pr->offset -= len;);
			physr_incr_iter(&iter);
		}
	}

	SANITYCHECK(SCL_DETAIL);

	if(pt_writemap(&vmp->vm_pt, regionstart,
	  MAP_NONE, len, 0, WMF_OVERWRITE) != OK) {
	    printf("VM: map_unmap_region: pt_writemap failed\n");
	    return ENOMEM;
	}

	SANITYCHECK(SCL_FUNCTIONS);

	return OK;
}

/*========================================================================*
 *				map_remap				  *
 *========================================================================*/
PUBLIC int map_remap(struct vmproc *dvmp, vir_bytes da, size_t size,
		struct vir_region *region, vir_bytes *r)
{
	struct vir_region *vr, *prev;
	struct phys_region *ph;
	vir_bytes startv, dst_addr;
	physr_iter iter;

	SANITYCHECK(SCL_FUNCTIONS);

	/* da is handled differently */
	if (!da)
		dst_addr = dvmp->vm_stacktop;
	else
		dst_addr = da;
	dst_addr = arch_vir2map(dvmp, dst_addr);

	prev = NULL;
	/* round up to page size */
	if (size % I386_PAGE_SIZE)
		size += I386_PAGE_SIZE - size % I386_PAGE_SIZE;
	startv = region_find_slot(dvmp, dst_addr, VM_DATATOP, size, &prev);
	if (startv == (vir_bytes) -1) {
		printf("map_remap: search %x...\n", dst_addr);
		map_printmap(dvmp);
		return ENOMEM;
	}
	/* when the user specifies the address, we cannot change it */
	if (da && (startv != dst_addr))
		return EINVAL;

	vr = map_copy_region(dvmp, region);
	if(!vr)
		return ENOMEM;

	USE(vr,
	vr->vaddr = startv;
	vr->length = size;
	vr->flags = region->flags;
	vr->tag = VRT_NONE;
	vr->parent = dvmp;);
	vm_assert(vr->flags & VR_SHARED);

	if (prev) {
		USE(vr,
		vr->next = prev->next;);
		USE(prev, prev->next = vr;);
	} else {
		USE(vr,
		vr->next = dvmp->vm_regions;);
		dvmp->vm_regions = vr;
	}

	physr_start_iter_least(vr->phys, &iter);
	while((ph = physr_get_iter(&iter))) {
		struct phys_block *pb = ph->ph;
		USE(pb, pb->refcount++;);
		if(map_ph_writept(dvmp, vr, ph) != OK) {
			vm_panic("map_remap: map_ph_writept failed", NO_NUM);
		}

		physr_incr_iter(&iter);
	}

	*r = startv;

	SANITYCHECK(SCL_FUNCTIONS);

	return OK;
}

/*========================================================================*
 *				map_get_phys				  *
 *========================================================================*/
PUBLIC int map_get_phys(struct vmproc *vmp, vir_bytes addr, phys_bytes *r)
{
	struct vir_region *vr;
	struct phys_region *ph;
	physr_iter iter;

	if (!(vr = map_lookup(vmp, addr)) ||
		(vr->vaddr != addr))
		return EINVAL;

	if (!(vr->flags & VR_SHARED))
		return EINVAL;

	physr_start_iter_least(vr->phys, &iter);
	ph = physr_get_iter(&iter);

	vm_assert(ph);
	vm_assert(ph->ph);
	if (r)
		*r = ph->ph->phys;

	return OK;
}

/*========================================================================*
 *				map_get_ref				  *
 *========================================================================*/
PUBLIC int map_get_ref(struct vmproc *vmp, vir_bytes addr, u8_t *cnt)
{
	struct vir_region *vr;
	struct phys_region *ph;
	physr_iter iter;

	if (!(vr = map_lookup(vmp, addr)) ||
		(vr->vaddr != addr))
		return EINVAL;

	if (!(vr->flags & VR_SHARED))
		return EINVAL;

	physr_start_iter_least(vr->phys, &iter);
	ph = physr_get_iter(&iter);

	vm_assert(ph);
	vm_assert(ph->ph);
	if (cnt)
		*cnt = ph->ph->refcount;

	return OK;
}

/*========================================================================*
 *				get_usage_info				  *
 *========================================================================*/
PUBLIC void get_usage_info(struct vmproc *vmp, struct vm_usage_info *vui)
{
	struct vir_region *vr;
	physr_iter iter;
	struct phys_region *ph;
	vir_bytes len;

	memset(vui, 0, sizeof(*vui));

	for(vr = vmp->vm_regions; vr; vr = vr->next) {
		physr_start_iter_least(vr->phys, &iter);
		while((ph = physr_get_iter(&iter))) {
			len = ph->ph->length;

			/* All present pages are counted towards the total. */
			vui->vui_total += len;

			if (ph->ph->refcount > 1) {
				/* Any page with a refcount > 1 is common. */
				vui->vui_common += len;
	
				/* Any common, non-COW page is shared. */
				if (vr->flags & VR_SHARED ||
					ph->ph->share_flag == PBSH_SMAP)
					vui->vui_shared += len;
			}
			physr_incr_iter(&iter);
		}
	}
}

/*===========================================================================*
 *				get_region_info				     *
 *===========================================================================*/
PUBLIC int get_region_info(struct vmproc *vmp, struct vm_region_info *vri,
	int max, vir_bytes *nextp)
{
	struct vir_region *vr;
	vir_bytes next;
	int count;

	next = *nextp;

	if (!max) return 0;

	for(vr = vmp->vm_regions; vr; vr = vr->next)
		if (vr->vaddr >= next) break;

	if (!vr) return 0;

	for(count = 0; vr && count < max; vr = vr->next, count++, vri++) {
		vri->vri_addr = arch_map2info(vmp, vr->vaddr, &vri->vri_seg,
			&vri->vri_prot);
		vri->vri_length = vr->length;

		/* "AND" the provided protection with per-page protection. */
		if (!(vr->flags & VR_WRITABLE))
			vri->vri_prot &= ~PROT_WRITE;

		vri->vri_flags = (vr->flags & VR_SHARED) ? MAP_SHARED : 0;

		next = vr->vaddr + vr->length;
	}

	*nextp = next;
	return count;
}

/*========================================================================*
 *				regionprintstats			  *
 *========================================================================*/
PUBLIC void printregionstats(struct vmproc *vmp)
{
	struct vir_region *vr;
	struct phys_region *pr;
	physr_iter iter;
	vir_bytes used = 0, weighted = 0;

	for(vr = vmp->vm_regions; vr; vr = vr->next) {
		if(vr->flags & VR_DIRECT)
			continue;
		physr_start_iter_least(vr->phys, &iter);
		while((pr = physr_get_iter(&iter))) {
			physr_incr_iter(&iter);
			used += pr->ph->length;
			weighted += pr->ph->length / pr->ph->refcount;
		}
	}

	printf("%6dkB  %6dkB\n", used/1024, weighted/1024);

	return;
}

