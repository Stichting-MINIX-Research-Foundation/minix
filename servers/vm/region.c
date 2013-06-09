
#include <minix/com.h>
#include <minix/callnr.h>
#include <minix/type.h>
#include <minix/config.h>
#include <minix/const.h>
#include <minix/sysutil.h>
#include <minix/syslib.h>
#include <minix/debug.h>
#include <minix/bitmap.h>
#include <minix/hash.h>
#include <machine/multiboot.h>

#include <sys/mman.h>

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <sys/param.h>

#include "vm.h"
#include "proto.h"
#include "util.h"
#include "glo.h"
#include "region.h"
#include "sanitycheck.h"
#include "memlist.h"
#include "memtype.h"
#include "regionavl.h"

static struct vir_region *map_copy_region(struct vmproc *vmp, struct
	vir_region *vr);

void map_region_init(void)
{
}

void map_printregion(struct vir_region *vr)
{
	int i;
	struct phys_region *ph;
	printf("map_printmap: map_name: %s\n", vr->def_memtype->name);
	printf("\t%lx (len 0x%lx, %lukB), %p, %s\n",
		vr->vaddr, vr->length, vr->length/1024,
		vr->def_memtype->name,
		(vr->flags & VR_WRITABLE) ? "writable" : "readonly");
	printf("\t\tphysblocks:\n");
	for(i = 0; i < vr->length/VM_PAGE_SIZE; i++) {
		if(!(ph=vr->physblocks[i])) continue;
		printf("\t\t@ %lx (refs %d): phys 0x%lx, %s\n",
			(vr->vaddr + ph->offset),
			ph->ph->refcount, ph->ph->phys,
		pt_writable(vr->parent, vr->vaddr + ph->offset) ? "W" : "R");
		
	}
}

struct phys_region *physblock_get(struct vir_region *region, vir_bytes offset)
{
	int i;
	struct phys_region *foundregion;
	assert(!(offset % VM_PAGE_SIZE));
	assert(offset >= 0 && offset < region->length);
	i = offset/VM_PAGE_SIZE;
	if((foundregion =  region->physblocks[i]))
		assert(foundregion->offset == offset);
	return foundregion;
}

void physblock_set(struct vir_region *region, vir_bytes offset,
	struct phys_region *newphysr)
{
	int i;
	struct vmproc *proc;
	assert(!(offset % VM_PAGE_SIZE));
	assert(offset >= 0 && offset < region->length);
	i = offset/VM_PAGE_SIZE;
	proc = region->parent;
	assert(proc);
	if(newphysr) {
		assert(!region->physblocks[i]);
		assert(newphysr->offset == offset);
		proc->vm_total += VM_PAGE_SIZE;
		if (proc->vm_total > proc->vm_total_max)
			proc->vm_total_max = proc->vm_total;
	} else {
		assert(region->physblocks[i]);
		proc->vm_total -= VM_PAGE_SIZE;
	}
	region->physblocks[i] = newphysr;
}

/*===========================================================================*
 *				map_printmap				     *
 *===========================================================================*/
void map_printmap(vmp)
struct vmproc *vmp;
{
	struct vir_region *vr;
	region_iter iter;

	printf("memory regions in process %d:\n", vmp->vm_endpoint);

	region_start_iter_least(&vmp->vm_regions_avl, &iter);
	while((vr = region_get_iter(&iter))) {
		map_printregion(vr);
		region_incr_iter(&iter);
	}
}

static struct vir_region *getnextvr(struct vir_region *vr)
{
	struct vir_region *nextvr;
	region_iter v_iter;
	SLABSANE(vr);
	region_start_iter(&vr->parent->vm_regions_avl, &v_iter, vr->vaddr, AVL_EQUAL);
	assert(region_get_iter(&v_iter));
	assert(region_get_iter(&v_iter) == vr);
	region_incr_iter(&v_iter);
	nextvr = region_get_iter(&v_iter);
	if(!nextvr) return NULL;
	SLABSANE(nextvr);
	assert(vr->parent == nextvr->parent);
	assert(vr->vaddr < nextvr->vaddr);
	assert(vr->vaddr + vr->length <= nextvr->vaddr);
	return nextvr;
}

int pr_writable(struct vir_region *vr, struct phys_region *pr)
{
	assert(pr->memtype->writable);
	return ((vr->flags & VR_WRITABLE) && pr->memtype->writable(pr));
}

#if SANITYCHECKS

/*===========================================================================*
 *				map_sanitycheck_pt			     *
 *===========================================================================*/
static int map_sanitycheck_pt(struct vmproc *vmp,
	struct vir_region *vr, struct phys_region *pr)
{
	struct phys_block *pb = pr->ph;
	int rw;
	int r;

	if(pr_writable(vr, pr))
		rw = PTF_WRITE;
	else
		rw = PTF_READ;

	r = pt_writemap(vmp, &vmp->vm_pt, vr->vaddr + pr->offset,
	  pb->phys, VM_PAGE_SIZE, PTF_PRESENT | PTF_USER | rw, WMF_VERIFY);

	if(r != OK) {
		printf("proc %d phys_region 0x%lx sanity check failed\n",
			vmp->vm_endpoint, pr->offset);
		map_printregion(vr);
	}

	return r;
}

/*===========================================================================*
 *				map_sanitycheck			     *
 *===========================================================================*/
void map_sanitycheck(char *file, int line)
{
	struct vmproc *vmp;

/* Macro for looping over all physical blocks of all regions of
 * all processes.
 */
#define ALLREGIONS(regioncode, physcode)			\
	for(vmp = vmproc; vmp < &vmproc[VMP_NR]; vmp++) {	\
		vir_bytes voffset;				\
		region_iter v_iter;				\
		struct vir_region *vr;				\
		if(!(vmp->vm_flags & VMF_INUSE))		\
			continue;				\
		region_start_iter_least(&vmp->vm_regions_avl, &v_iter);	\
		while((vr = region_get_iter(&v_iter))) {	\
			struct phys_region *pr;			\
			regioncode;				\
			for(voffset = 0; voffset < vr->length; \
				voffset += VM_PAGE_SIZE) {	\
				if(!(pr = physblock_get(vr, voffset))) 	\
					continue;	\
				physcode;			\
			}					\
			region_incr_iter(&v_iter);		\
		}						\
	}

#define MYSLABSANE(s) MYASSERT(slabsane_f(__FILE__, __LINE__, s, sizeof(*(s))))
	/* Basic pointers check. */
	ALLREGIONS(MYSLABSANE(vr),MYSLABSANE(pr); MYSLABSANE(pr->ph);MYSLABSANE(pr->parent));
	ALLREGIONS(/* MYASSERT(vr->parent == vmp) */,MYASSERT(pr->parent == vr););

	/* Do counting for consistency check. */
	ALLREGIONS(;,USE(pr->ph, pr->ph->seencount = 0;););
	ALLREGIONS(;,MYASSERT(pr->offset == voffset););
	ALLREGIONS(;,USE(pr->ph, pr->ph->seencount++;);
		if(pr->ph->seencount == 1) {
			if(pr->memtype->ev_sanitycheck)
				pr->memtype->ev_sanitycheck(pr, file, line);
		}
	);

	/* Do consistency check. */
	ALLREGIONS({ struct vir_region *nextvr = getnextvr(vr);
		if(nextvr) {
			MYASSERT(vr->vaddr < nextvr->vaddr);
			MYASSERT(vr->vaddr + vr->length <= nextvr->vaddr);
		}
		}
		MYASSERT(!(vr->vaddr % VM_PAGE_SIZE));,	
		if(pr->ph->flags & PBF_INCACHE) pr->ph->seencount++;
		if(pr->ph->refcount != pr->ph->seencount) {
			map_printmap(vmp);
			printf("ph in vr %p: 0x%lx  refcount %u "
				"but seencount %u\n", 
				vr, pr->offset,
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
			if(pr->ph->flags & PBF_INCACHE) n_others++;
			MYASSERT(pr->ph->refcount == n_others);
		}
		MYASSERT(pr->ph->refcount == pr->ph->seencount);
		MYASSERT(!(pr->offset % VM_PAGE_SIZE)););
	ALLREGIONS(,MYASSERT(map_sanitycheck_pt(vmp, vr, pr) == OK));
}

#endif

/*=========================================================================*
 *				map_ph_writept				*
 *=========================================================================*/
int map_ph_writept(struct vmproc *vmp, struct vir_region *vr,
	struct phys_region *pr)
{
	int flags = PTF_PRESENT | PTF_USER;
	struct phys_block *pb = pr->ph;

	assert(vr);
	assert(pr);
	assert(pb);

	assert(!(vr->vaddr % VM_PAGE_SIZE));
	assert(!(pr->offset % VM_PAGE_SIZE));
	assert(pb->refcount > 0);

	if(pr_writable(vr, pr))
		flags |= PTF_WRITE;
	else
		flags |= PTF_READ;

#if  defined(__arm__)
	if (pb->phys >= 0x80000000 && pb->phys < (0xc0000000 - VM_PAGE_SIZE)) {
		// LSC Do this only for actual RAM
		// KEJO:fishy will need to look into this 
		flags |= ARM_VM_PTE_DEVICE;
	}
#endif

	if(pt_writemap(vmp, &vmp->vm_pt, vr->vaddr + pr->offset,
			pb->phys, VM_PAGE_SIZE, flags,
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

#define SLOT_FAIL ((vir_bytes) -1)

/*===========================================================================*
 *				region_find_slot_range			     *
 *===========================================================================*/
static vir_bytes region_find_slot_range(struct vmproc *vmp,
		vir_bytes minv, vir_bytes maxv, vir_bytes length)
{
	struct vir_region *lastregion;
	vir_bytes startv = 0;
	int foundflag = 0;
	region_iter iter;

	SANITYCHECK(SCL_FUNCTIONS);

	/* Length must be reasonable. */
	assert(length > 0);

	/* Special case: allow caller to set maxv to 0 meaning 'I want
	 * it to be mapped in right here.'
	 */
        if(maxv == 0) {
                maxv = minv + length;

                /* Sanity check. */
                if(maxv <= minv) {
                        printf("region_find_slot: minv 0x%lx and bytes 0x%lx\n",
                                minv, length);
                        return SLOT_FAIL;
                }
        }

	/* Basic input sanity checks. */
	assert(!(length % VM_PAGE_SIZE));
	if(minv >= maxv) {
		printf("VM: 1 minv: 0x%lx maxv: 0x%lx length: 0x%lx\n",
			minv, maxv, length);
	}

	assert(minv < maxv);

	if(minv + length > maxv)
		return SLOT_FAIL;

#define FREEVRANGE_TRY(rangestart, rangeend) {		\
	vir_bytes frstart = (rangestart), frend = (rangeend);	\
	frstart = MAX(frstart, minv);				\
	frend   = MIN(frend, maxv);				\
	if(frend > frstart && (frend - frstart) >= length) {	\
		startv = frend-length;				\
		foundflag = 1;					\
	} }

#define FREEVRANGE(start, end) {					\
	assert(!foundflag);						\
	FREEVRANGE_TRY(((start)+VM_PAGE_SIZE), ((end)-VM_PAGE_SIZE));	\
	if(!foundflag) {						\
		FREEVRANGE_TRY((start), (end));				\
	}								\
}

	/* find region after maxv. */
	region_start_iter(&vmp->vm_regions_avl, &iter, maxv, AVL_GREATER_EQUAL);
	lastregion = region_get_iter(&iter);

	if(!lastregion) {
		/* This is the free virtual address space after the last region. */
		region_start_iter(&vmp->vm_regions_avl, &iter, maxv, AVL_LESS);
		lastregion = region_get_iter(&iter);
		FREEVRANGE(lastregion ?
			lastregion->vaddr+lastregion->length : 0, VM_DATATOP);
	}

	if(!foundflag) {
		struct vir_region *vr;
		while((vr = region_get_iter(&iter)) && !foundflag) {
			struct vir_region *nextvr;
			region_decr_iter(&iter);
			nextvr = region_get_iter(&iter);
			FREEVRANGE(nextvr ? nextvr->vaddr+nextvr->length : 0,
			  vr->vaddr);
		}
	}

	if(!foundflag) {
		return SLOT_FAIL;
	}

	/* However we got it, startv must be in the requested range. */
	assert(startv >= minv);
	assert(startv < maxv);
	assert(startv + length <= maxv);

	/* remember this position as a hint for next time. */
	vmp->vm_region_top = startv + length;

	return startv;
}

/*===========================================================================*
 *				region_find_slot			     *
 *===========================================================================*/
static vir_bytes region_find_slot(struct vmproc *vmp,
		vir_bytes minv, vir_bytes maxv, vir_bytes length)
{
	vir_bytes v, hint = vmp->vm_region_top;

	/* use the top of the last inserted region as a minv hint if
	 * possible. remember that a zero maxv is a special case.
	 */

	if(maxv && hint < maxv && hint >= minv) {
		v = region_find_slot_range(vmp, minv, hint, length);

		if(v != SLOT_FAIL)
			return v;
	}

	return region_find_slot_range(vmp, minv, maxv, length);
}

static int phys_slot(vir_bytes len)
{
	assert(!(len % VM_PAGE_SIZE));
	return len / VM_PAGE_SIZE;
}

struct vir_region *region_new(struct vmproc *vmp, vir_bytes startv, vir_bytes length,
	int flags, mem_type_t *memtype)
{
	struct vir_region *newregion;
	struct phys_region **physregions;
	static u32_t id;
	int slots = phys_slot(length);

	if(!(SLABALLOC(newregion))) {
		printf("vm: region_new: could not allocate\n");
		return NULL;
	}

	/* Fill in node details. */
USE(newregion,
	memset(newregion, 0, sizeof(*newregion));
	newregion->vaddr = startv;
	newregion->length = length;
	newregion->flags = flags;
	newregion->def_memtype = memtype;
	newregion->remaps = 0;
	newregion->id = id++;
	newregion->lower = newregion->higher = NULL;
	newregion->parent = vmp;);

	if(!(physregions = calloc(slots, sizeof(struct phys_region *)))) {
		printf("VM: region_new: allocating phys blocks failed\n");
		SLABFREE(newregion);
		return NULL;
	}

	USE(newregion, newregion->physblocks = physregions;);

	return newregion;
}

/*===========================================================================*
 *				map_page_region				     *
 *===========================================================================*/
struct vir_region *map_page_region(vmp, minv, maxv, length,
	flags, mapflags, memtype)
struct vmproc *vmp;
vir_bytes minv;
vir_bytes maxv;
vir_bytes length;
u32_t flags;
int mapflags;
mem_type_t *memtype;
{
	struct vir_region *newregion;
	vir_bytes startv;

	assert(!(length % VM_PAGE_SIZE));

	SANITYCHECK(SCL_FUNCTIONS);

	startv = region_find_slot(vmp, minv, maxv, length);
	if (startv == SLOT_FAIL)
		return NULL;

	/* Now we want a new region. */
	if(!(newregion = region_new(vmp, startv, length, flags, memtype))) {
		printf("VM: map_page_region: allocating region failed\n");
		return NULL;
	}

	/* If a new event is specified, invoke it. */
	if(newregion->def_memtype->ev_new) {
		if(newregion->def_memtype->ev_new(newregion) != OK) {
			/* ev_new will have freed and removed the region */
			return NULL;
		}
	}

	if(mapflags & MF_PREALLOC) {
		if(map_handle_memory(vmp, newregion, 0, length, 1,
			NULL, 0, 0) != OK) {
			printf("VM: map_page_region: prealloc failed\n");
			free(newregion->physblocks);
			USE(newregion,
				newregion->physblocks = NULL;);
			SLABFREE(newregion);
			return NULL;
		}
	}

	/* Pre-allocations should be uninitialized, but after that it's a
	 * different story.
	 */
	USE(newregion, newregion->flags &= ~VR_UNINITIALIZED;);

	/* Link it. */
	region_insert(&vmp->vm_regions_avl, newregion);

#if SANITYCHECKS
	assert(startv == newregion->vaddr);
	{
		struct vir_region *nextvr;
		if((nextvr = getnextvr(newregion))) {
			assert(newregion->vaddr < nextvr->vaddr);
		}
	}
#endif

	SANITYCHECK(SCL_FUNCTIONS);

	return newregion;
}

/*===========================================================================*
 *				map_subfree				     *
 *===========================================================================*/
static int map_subfree(struct vir_region *region, 
	vir_bytes start, vir_bytes len)
{
	struct phys_region *pr;
	vir_bytes end = start+len;
	vir_bytes voffset;

#if SANITYCHECKS
	SLABSANE(region);
	for(voffset = 0; voffset < phys_slot(region->length);
		voffset += VM_PAGE_SIZE) {
		struct phys_region *others;
		struct phys_block *pb;

		if(!(pr = physblock_get(region, voffset)))
			continue;

		pb = pr->ph;

		for(others = pb->firstregion; others;
			others = others->next_ph_list) {
			assert(others->ph == pb);
		}
	}
#endif

	for(voffset = start; voffset < end; voffset+=VM_PAGE_SIZE) {
		if(!(pr = physblock_get(region, voffset)))
			continue;
		assert(pr->offset >= start);
		assert(pr->offset < end);
		pb_unreferenced(region, pr, 1);
		SLABFREE(pr);
	}

	return OK;
}

/*===========================================================================*
 *				map_free				     *
 *===========================================================================*/
int map_free(struct vir_region *region)
{
	int r;

	if((r=map_subfree(region, 0, region->length)) != OK) {
		printf("%d\n", __LINE__);
		return r;
	}

	if(region->def_memtype->ev_delete)
		region->def_memtype->ev_delete(region);
	free(region->physblocks);
	region->physblocks = NULL;
	SLABFREE(region);

	return OK;
}

/*========================================================================*
 *				map_free_proc				  *
 *========================================================================*/
int map_free_proc(vmp)
struct vmproc *vmp;
{
	struct vir_region *r;

	while((r = region_search_root(&vmp->vm_regions_avl))) {
		SANITYCHECK(SCL_DETAIL);
#if SANITYCHECKS
		nocheck++;
#endif
		region_remove(&vmp->vm_regions_avl, r->vaddr); /* For sanity checks. */
		map_free(r);
#if SANITYCHECKS
		nocheck--;
#endif
		SANITYCHECK(SCL_DETAIL);
	}

	region_init(&vmp->vm_regions_avl);

	SANITYCHECK(SCL_FUNCTIONS);

	return OK;
}

/*===========================================================================*
 *				map_lookup				     *
 *===========================================================================*/
struct vir_region *map_lookup(vmp, offset, physr)
struct vmproc *vmp;
vir_bytes offset;
struct phys_region **physr;
{
	struct vir_region *r;

	SANITYCHECK(SCL_FUNCTIONS);

#if SANITYCHECKS
	if(!region_search_root(&vmp->vm_regions_avl))
		panic("process has no regions: %d", vmp->vm_endpoint);
#endif

	if((r = region_search(&vmp->vm_regions_avl, offset, AVL_LESS_EQUAL))) {
		vir_bytes ph;
		if(offset >= r->vaddr && offset < r->vaddr + r->length) {
			ph = offset - r->vaddr;
			if(physr) {
				*physr = physblock_get(r, ph);
				if(*physr) assert((*physr)->offset == ph);
			}
			return r;
		}
	}

	SANITYCHECK(SCL_FUNCTIONS);

	return NULL;
}

u32_t vrallocflags(u32_t flags)
{
	u32_t allocflags = 0;

	if(flags & VR_PHYS64K)
		allocflags |= PAF_ALIGN64K;
	if(flags & VR_LOWER16MB)
		allocflags |= PAF_LOWER16MB;
	if(flags & VR_LOWER1MB)
		allocflags |= PAF_LOWER1MB;
	if(!(flags & VR_UNINITIALIZED))
		allocflags |= PAF_CLEAR;

	return allocflags;
}

/*===========================================================================*
 *				map_pf			     *
 *===========================================================================*/
int map_pf(vmp, region, offset, write, pf_callback, state, len, io)
struct vmproc *vmp;
struct vir_region *region;
vir_bytes offset;
int write;
vfs_callback_t pf_callback;
void *state;
int len;
int *io;
{
	struct phys_region *ph;
	int r = OK;

	offset -= offset % VM_PAGE_SIZE;

	assert(offset >= 0);
	assert(offset < region->length);

	assert(!(region->vaddr % VM_PAGE_SIZE));
	assert(!(write && !(region->flags & VR_WRITABLE)));

	SANITYCHECK(SCL_FUNCTIONS);

	if(!(ph = physblock_get(region, offset))) {
		struct phys_block *pb;

		/* New block. */

		if(!(pb = pb_new(MAP_NONE))) {
			printf("map_pf: pb_new failed\n");
			return ENOMEM;
		}

		if(!(ph = pb_reference(pb, offset, region,
			region->def_memtype))) {
			printf("map_pf: pb_reference failed\n");
			pb_free(pb);
			return ENOMEM;
		}	
	}

	assert(ph);
	assert(ph->ph);

	/* If we're writing and the block is already
	 * writable, nothing to do.
	 */

	assert(ph->memtype->writable);

	if(!write || !ph->memtype->writable(ph)) {
		assert(ph->memtype->ev_pagefault);
		assert(ph->ph);

		if((r = ph->memtype->ev_pagefault(vmp,
			region, ph, write, pf_callback, state, len, io)) == SUSPEND) {
			return SUSPEND;
		}

		if(r != OK) {
			printf("map_pf: pagefault in %s failed\n", ph->memtype->name);
			if(ph)
				pb_unreferenced(region, ph, 1);
			return r;
		}

		assert(ph);
		assert(ph->ph);
		assert(ph->ph->phys != MAP_NONE);
	}

	assert(ph->ph);
	assert(ph->ph->phys != MAP_NONE);

	if((r = map_ph_writept(vmp, region, ph)) != OK) {
		printf("map_pf: writept failed\n");
		return r;
	}

	SANITYCHECK(SCL_FUNCTIONS);

#if SANITYCHECKS
	if(OK != pt_checkrange(&vmp->vm_pt, region->vaddr+offset,
		VM_PAGE_SIZE, write)) {
		panic("map_pf: pt_checkrange failed: %d", r);
	}
#endif	

	return r;
}

int map_handle_memory(vmp, region, start_offset, length, write,
	cb, state, statelen)
struct vmproc *vmp;
struct vir_region *region;
vir_bytes start_offset;
vir_bytes length;
int write;
vfs_callback_t cb;
void *state;
int statelen;
{
	vir_bytes offset, lim;
	int r;
	int io = 0;

	assert(length > 0);
	lim = start_offset + length;
	assert(lim > start_offset);

	for(offset = start_offset; offset < lim; offset += VM_PAGE_SIZE)
		if((r = map_pf(vmp, region, offset, write,
		   cb, state, statelen, &io)) != OK)
			return r;

	return OK;
}

/*===========================================================================*
 *				map_pin_memory      			     *
 *===========================================================================*/
int map_pin_memory(struct vmproc *vmp)
{
	struct vir_region *vr;
	int r;
	region_iter iter;
	region_start_iter_least(&vmp->vm_regions_avl, &iter);
	/* Scan all memory regions. */
	while((vr = region_get_iter(&iter))) {
		/* Make sure region is mapped to physical memory and writable.*/
		r = map_handle_memory(vmp, vr, 0, vr->length, 1, NULL, 0, 0);
		if(r != OK) {
		    panic("map_pin_memory: map_handle_memory failed: %d", r);
		}
		region_incr_iter(&iter);
	}
	return OK;
}

/*===========================================================================*
 *				map_copy_region			     	*
 *===========================================================================*/
struct vir_region *map_copy_region(struct vmproc *vmp, struct vir_region *vr)
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
	int r;
#if SANITYCHECKS
	int cr;
	cr = physregions(vr);
#endif
	vir_bytes p;

	if(!(newvr = region_new(vr->parent, vr->vaddr, vr->length, vr->flags, vr->def_memtype)))
		return NULL;

	USE(newvr, newvr->parent = vmp;);

	if(vr->def_memtype->ev_copy && (r=vr->def_memtype->ev_copy(vr, newvr)) != OK) {
		map_free(newvr);
		printf("VM: memtype-specific copy failed (%d)\n", r);
		return NULL;
	}

	for(p = 0; p < phys_slot(vr->length); p++) {
		struct phys_region *newph;

		if(!(ph = physblock_get(vr, p*VM_PAGE_SIZE))) continue;
		newph = pb_reference(ph->ph, ph->offset, newvr,
			vr->def_memtype);

		if(!newph) { map_free(newvr); return NULL; }

		if(ph->memtype->ev_reference)
			ph->memtype->ev_reference(ph, newph);

#if SANITYCHECKS
		USE(newph, newph->written = 0;);
		assert(physregions(vr) == cr);
#endif
	}

#if SANITYCHECKS
	assert(physregions(vr) == physregions(newvr));
#endif

	return newvr;
}

/*===========================================================================*
 *				copy_abs2region			     	*
 *===========================================================================*/
int copy_abs2region(phys_bytes abs, struct vir_region *destregion,
	phys_bytes offset, phys_bytes len)

{
	assert(destregion);
	assert(destregion->physblocks);
	while(len > 0) {
		phys_bytes sublen, suboffset;
		struct phys_region *ph;
		assert(destregion);
		assert(destregion->physblocks);
		if(!(ph = physblock_get(destregion, offset))) {
			printf("VM: copy_abs2region: no phys region found (1).\n");
			return EFAULT;
		}
		assert(ph->offset <= offset);
		if(ph->offset+VM_PAGE_SIZE <= offset) {
			printf("VM: copy_abs2region: no phys region found (2).\n");
			return EFAULT;
		}
		suboffset = offset - ph->offset;
		assert(suboffset < VM_PAGE_SIZE);
		sublen = len;
		if(sublen > VM_PAGE_SIZE - suboffset)
			sublen = VM_PAGE_SIZE - suboffset;
		assert(suboffset + sublen <= VM_PAGE_SIZE);
		if(ph->ph->refcount != 1) {
			printf("VM: copy_abs2region: refcount not 1.\n");
			return EFAULT;
		}

		if(sys_abscopy(abs, ph->ph->phys + suboffset, sublen) != OK) {
			printf("VM: copy_abs2region: abscopy failed.\n");
			return EFAULT;
		}
		abs += sublen;
		offset += sublen;
		len -= sublen;
	}

	return OK;
}

/*=========================================================================*
 *				map_writept				*
 *=========================================================================*/
int map_writept(struct vmproc *vmp)
{
	struct vir_region *vr;
	struct phys_region *ph;
	int r;
	region_iter v_iter;
	region_start_iter_least(&vmp->vm_regions_avl, &v_iter);

	while((vr = region_get_iter(&v_iter))) {
		vir_bytes p;
		for(p = 0; p < vr->length; p += VM_PAGE_SIZE) {
			if(!(ph = physblock_get(vr, p))) continue;

			if((r=map_ph_writept(vmp, vr, ph)) != OK) {
				printf("VM: map_writept: failed\n");
				return r;
			}
		}
		region_incr_iter(&v_iter);
	}

	return OK;
}

/*========================================================================*
 *			       map_proc_copy			     	  *
 *========================================================================*/
int map_proc_copy(dst, src)
struct vmproc *dst;
struct vmproc *src;
{
/* Copy all the memory regions from the src process to the dst process. */
	region_init(&dst->vm_regions_avl);

	return map_proc_copy_from(dst, src, NULL);
}

/*========================================================================*
 *			     map_proc_copy_from			     	  *
 *========================================================================*/
int map_proc_copy_from(dst, src, start_src_vr)
struct vmproc *dst;
struct vmproc *src;
struct vir_region *start_src_vr;
{
	struct vir_region *vr;
	region_iter v_iter;

	if(!start_src_vr)
		start_src_vr = region_search_least(&src->vm_regions_avl);

	assert(start_src_vr);
	assert(start_src_vr->parent == src);
	region_start_iter(&src->vm_regions_avl, &v_iter,
		start_src_vr->vaddr, AVL_EQUAL);
	assert(region_get_iter(&v_iter) == start_src_vr);

	/* Copy source regions after the destination's last region (if any). */

	SANITYCHECK(SCL_FUNCTIONS);

	while((vr = region_get_iter(&v_iter))) {
		struct vir_region *newvr;
		if(!(newvr = map_copy_region(dst, vr))) {
			map_free_proc(dst);
			return ENOMEM;
		}
		region_insert(&dst->vm_regions_avl, newvr);
		assert(vr->length == newvr->length);

#if SANITYCHECKS
	{
		vir_bytes vaddr;
		struct phys_region *orig_ph, *new_ph;
		assert(vr->physblocks != newvr->physblocks);
		for(vaddr = 0; vaddr < vr->length; vaddr += VM_PAGE_SIZE) {
			orig_ph = physblock_get(vr, vaddr);
			new_ph = physblock_get(newvr, vaddr);
			if(!orig_ph) { assert(!new_ph); continue;}
			assert(new_ph);
			assert(orig_ph != new_ph);
			assert(orig_ph->ph == new_ph->ph);
		}
	}
#endif
		region_incr_iter(&v_iter);
	}

	map_writept(src);
	map_writept(dst);

	SANITYCHECK(SCL_FUNCTIONS);
	return OK;
}

int map_region_extend_upto_v(struct vmproc *vmp, vir_bytes v)
{
	vir_bytes offset = v, limit, extralen;
	struct vir_region *vr, *nextvr;
	struct phys_region **newpr;
	int newslots, prevslots, addedslots, r;

	offset = roundup(offset, VM_PAGE_SIZE);

	if(!(vr = region_search(&vmp->vm_regions_avl, offset, AVL_LESS))) {
		printf("VM: nothing to extend\n");
		return ENOMEM;
	}

	if(vr->vaddr + vr->length >= v) return OK;

	limit = vr->vaddr + vr->length;

	assert(vr->vaddr <= offset);
	newslots = phys_slot(offset - vr->vaddr);
	prevslots = phys_slot(vr->length);
	assert(newslots >= prevslots);
	addedslots = newslots - prevslots;
	extralen = offset - limit;
	assert(extralen > 0);

	if((nextvr = getnextvr(vr))) {
		assert(offset <= nextvr->vaddr);
	}

	if(nextvr && nextvr->vaddr < offset) {
		printf("VM: can't grow into next region\n");
		return ENOMEM;
	}

	if(!vr->def_memtype->ev_resize) {
		if(!map_page_region(vmp, limit, 0, extralen,
			VR_WRITABLE | VR_ANON,
			0, &mem_type_anon)) {
			printf("resize: couldn't put anon memory there\n");
			return ENOMEM;
		}
		return OK;
	}

	if(!(newpr = realloc(vr->physblocks,
		newslots * sizeof(struct phys_region *)))) {
		printf("VM: map_region_extend_upto_v: realloc failed\n");
		return ENOMEM;
	}

	vr->physblocks = newpr;
	memset(vr->physblocks + prevslots, 0,
		addedslots * sizeof(struct phys_region *));

	r = vr->def_memtype->ev_resize(vmp, vr, offset - vr->vaddr);

	return r;
}

/*========================================================================*
 *				map_unmap_region	     	  	*
 *========================================================================*/
int map_unmap_region(struct vmproc *vmp, struct vir_region *r,
	vir_bytes offset, vir_bytes len)
{
/* Shrink the region by 'len' bytes, from the start. Unreference
 * memory it used to reference if any.
 */
	vir_bytes regionstart;
	int freeslots = phys_slot(len);

	SANITYCHECK(SCL_FUNCTIONS);

	if(offset+len > r->length || (len % VM_PAGE_SIZE)) {
		printf("VM: bogus length 0x%lx\n", len);
		return EINVAL;
	}

	regionstart = r->vaddr + offset;

	/* unreference its memory */
	map_subfree(r, offset, len);

	/* if unmap was at start/end of this region, it actually shrinks */
	if(r->length == len) {
		/* Whole region disappears. Unlink and free it. */
		region_remove(&vmp->vm_regions_avl, r->vaddr);
		map_free(r);
	} else if(offset == 0) {
		struct phys_region *pr;
		vir_bytes voffset;
		int remslots;

		if(!r->def_memtype->ev_lowshrink) {
			printf("VM: low-shrinking not implemented for %s\n",
				r->def_memtype->name);
			return EINVAL;
		}

		if(r->def_memtype->ev_lowshrink(r, len) != OK) {
			printf("VM: low-shrinking failed for %s\n",
				r->def_memtype->name);
			return EINVAL;
		}

		region_remove(&vmp->vm_regions_avl, r->vaddr);

		USE(r,
		r->vaddr += len;
		r->length -= len;);

		remslots = phys_slot(r->length);

		region_insert(&vmp->vm_regions_avl, r);

		/* vaddr has increased; to make all the phys_regions
		 * point to the same addresses, make them shrink by the
		 * same amount.
		 */
		for(voffset = offset; voffset < r->length;
			voffset += VM_PAGE_SIZE) {
			if(!(pr = physblock_get(r, voffset))) continue;
			assert(pr->offset >= offset);
			USE(pr, pr->offset -= len;);
		}
		if(remslots)
			memmove(r->physblocks, r->physblocks + freeslots,
				remslots * sizeof(struct phys_region *));
	} else if(offset + len == r->length) {
		assert(len <= r->length);
		r->length -= len;
	}

	SANITYCHECK(SCL_DETAIL);

	if(pt_writemap(vmp, &vmp->vm_pt, regionstart,
	  MAP_NONE, len, 0, WMF_OVERWRITE) != OK) {
	    printf("VM: map_unmap_region: pt_writemap failed\n");
	    return ENOMEM;
	}

	SANITYCHECK(SCL_FUNCTIONS);

	return OK;
}

int split_region(struct vmproc *vmp, struct vir_region *vr,
	struct vir_region **vr1, struct vir_region **vr2, vir_bytes split_len)
{
	struct vir_region *r1 = NULL, *r2 = NULL;
	vir_bytes rem_len = vr->length - split_len;
	int slots1, slots2;
	vir_bytes voffset;
	int n1 = 0, n2 = 0;

	assert(!(split_len % VM_PAGE_SIZE));
	assert(!(rem_len % VM_PAGE_SIZE));
	assert(!(vr->vaddr % VM_PAGE_SIZE));
	assert(!(vr->length % VM_PAGE_SIZE));

	if(!vr->def_memtype->ev_split) {
		printf("VM: split region not implemented for %s\n",
			vr->def_memtype->name);
		return EINVAL;
	}

	slots1 = phys_slot(split_len);
	slots2 = phys_slot(rem_len);

	if(!(r1 = region_new(vmp, vr->vaddr, split_len, vr->flags,
		vr->def_memtype))) {
		goto bail;
	}

	if(!(r2 = region_new(vmp, vr->vaddr+split_len, rem_len, vr->flags,
		vr->def_memtype))) {
		map_free(r1);
		goto bail;
	}

	for(voffset = 0; voffset < r1->length; voffset += VM_PAGE_SIZE) {
		struct phys_region *ph, *phn;
		if(!(ph = physblock_get(vr, voffset))) continue;
		if(!(phn = pb_reference(ph->ph, voffset, r1, ph->memtype)))
			goto bail;
		n1++;
	}

	for(voffset = 0; voffset < r2->length; voffset += VM_PAGE_SIZE) {
		struct phys_region *ph, *phn;
		if(!(ph = physblock_get(vr, split_len + voffset))) continue;
		if(!(phn = pb_reference(ph->ph, voffset, r2, ph->memtype)))
			goto bail;
		n2++;
	}

	vr->def_memtype->ev_split(vmp, vr, r1, r2);

	region_remove(&vmp->vm_regions_avl, vr->vaddr);
	map_free(vr);
	region_insert(&vmp->vm_regions_avl, r1);
	region_insert(&vmp->vm_regions_avl, r2);

	*vr1 = r1;
	*vr2 = r2;

	return OK;

  bail:
	if(r1) map_free(r1);
	if(r2) map_free(r2);

	printf("split_region: failed\n");

	return ENOMEM;
}

int map_unmap_range(struct vmproc *vmp, vir_bytes unmap_start, vir_bytes length)
{
	vir_bytes o = unmap_start % VM_PAGE_SIZE, unmap_limit;
	region_iter v_iter;
	struct vir_region *vr, *nextvr;

	unmap_start -= o;
	length += o;
	length = roundup(length, VM_PAGE_SIZE);
	unmap_limit = length + unmap_start;

	if(length < VM_PAGE_SIZE) return EINVAL;
	if(unmap_limit <= unmap_start) return EINVAL;

	region_start_iter(&vmp->vm_regions_avl, &v_iter, unmap_start, AVL_LESS_EQUAL);

	if(!(vr = region_get_iter(&v_iter))) {
		region_start_iter(&vmp->vm_regions_avl, &v_iter, unmap_start, AVL_GREATER);
		if(!(vr = region_get_iter(&v_iter))) {
			return OK;
		}
	}

	assert(vr);

	for(; vr && vr->vaddr < unmap_limit; vr = nextvr) {
		vir_bytes thislimit = vr->vaddr + vr->length;
		vir_bytes this_unmap_start, this_unmap_limit;
		vir_bytes remainlen;
		int r;

		region_incr_iter(&v_iter);
		nextvr = region_get_iter(&v_iter);

		assert(thislimit > vr->vaddr);

		this_unmap_start = MAX(unmap_start, vr->vaddr);
		this_unmap_limit = MIN(unmap_limit, thislimit);

		if(this_unmap_start >= this_unmap_limit) continue;

		if(this_unmap_start > vr->vaddr && this_unmap_limit < thislimit) {
			int r;
			struct vir_region *vr1, *vr2;
			vir_bytes split_len = this_unmap_limit - vr->vaddr;
			assert(split_len > 0);
			assert(split_len < vr->length);
			if((r=split_region(vmp, vr, &vr1, &vr2, split_len)) != OK) {
				printf("VM: unmap split failed\n");
				return r;
			}
			vr = vr1;
			thislimit = vr->vaddr + vr->length;
		}

		remainlen = this_unmap_limit - vr->vaddr;

		assert(this_unmap_start >= vr->vaddr);
		assert(this_unmap_limit <= thislimit);
		assert(remainlen > 0);

		r = map_unmap_region(vmp, vr, this_unmap_start - vr->vaddr,
			this_unmap_limit - this_unmap_start);

		if(r != OK) {
			printf("map_unmap_range: map_unmap_region failed\n");
			return r;
		}

		region_start_iter(&vmp->vm_regions_avl, &v_iter, nextvr->vaddr, AVL_EQUAL);
		assert(region_get_iter(&v_iter) == nextvr);
	}

	return OK;

}

/*========================================================================*
 *				map_get_phys				  *
 *========================================================================*/
int map_get_phys(struct vmproc *vmp, vir_bytes addr, phys_bytes *r)
{
	struct vir_region *vr;

	if (!(vr = map_lookup(vmp, addr, NULL)) ||
		(vr->vaddr != addr))
		return EINVAL;

	if (!vr->def_memtype->regionid)
		return EINVAL;

	if(r)
		*r = vr->def_memtype->regionid(vr);

	return OK;
}

/*========================================================================*
 *				map_get_ref				  *
 *========================================================================*/
int map_get_ref(struct vmproc *vmp, vir_bytes addr, u8_t *cnt)
{
	struct vir_region *vr;

	if (!(vr = map_lookup(vmp, addr, NULL)) ||
		(vr->vaddr != addr) || !vr->def_memtype->refcount)
		return EINVAL;

	if (cnt)
		*cnt = vr->def_memtype->refcount(vr);

	return OK;
}

void get_usage_info_kernel(struct vm_usage_info *vui)
{
	memset(vui, 0, sizeof(*vui));
	vui->vui_total = kernel_boot_info.kernel_allocated_bytes +
		kernel_boot_info.kernel_allocated_bytes_dynamic;
}

static void get_usage_info_vm(struct vm_usage_info *vui)
{
	memset(vui, 0, sizeof(*vui));
	vui->vui_total = kernel_boot_info.vm_allocated_bytes +
		get_vm_self_pages() * VM_PAGE_SIZE;
}

/*========================================================================*
 *				get_usage_info				  *
 *========================================================================*/
void get_usage_info(struct vmproc *vmp, struct vm_usage_info *vui)
{
	struct vir_region *vr;
	struct phys_region *ph;
	region_iter v_iter;
	region_start_iter_least(&vmp->vm_regions_avl, &v_iter);
	vir_bytes voffset;

	memset(vui, 0, sizeof(*vui));

	if(vmp->vm_endpoint == VM_PROC_NR) {
		get_usage_info_vm(vui);
		return;
	}

	if(vmp->vm_endpoint < 0) {
		get_usage_info_kernel(vui);
		return;
	}

	while((vr = region_get_iter(&v_iter))) {
		for(voffset = 0; voffset < vr->length; voffset += VM_PAGE_SIZE) {
			if(!(ph = physblock_get(vr, voffset))) continue;
			/* All present pages are counted towards the total. */
			vui->vui_total += VM_PAGE_SIZE;

			if (ph->ph->refcount > 1) {
				/* Any page with a refcount > 1 is common. */
				vui->vui_common += VM_PAGE_SIZE;
	
				/* Any common, non-COW page is shared. */
				if (vr->flags & VR_SHARED)
					vui->vui_shared += VM_PAGE_SIZE;
			}
		}
		region_incr_iter(&v_iter);
	}
}

/*===========================================================================*
 *				get_region_info				     *
 *===========================================================================*/
int get_region_info(struct vmproc *vmp, struct vm_region_info *vri,
	int max, vir_bytes *nextp)
{
	struct vir_region *vr;
	vir_bytes next;
	int count;
	region_iter v_iter;

	next = *nextp;

	if (!max) return 0;

	region_start_iter(&vmp->vm_regions_avl, &v_iter, next, AVL_GREATER_EQUAL);
	if(!(vr = region_get_iter(&v_iter))) return 0;

	for(count = 0; (vr = region_get_iter(&v_iter)) && count < max;
	   region_incr_iter(&v_iter)) {
		struct phys_region *ph1 = NULL, *ph2 = NULL;
		vir_bytes voffset;

		/* where to start on next iteration, regardless of what we find now */
		next = vr->vaddr + vr->length;

		/* Report part of the region that's actually in use. */

		/* Get first and last phys_regions, if any */
		for(voffset = 0; voffset < vr->length; voffset += VM_PAGE_SIZE) {
			struct phys_region *ph;
			if(!(ph = physblock_get(vr, voffset))) continue;
			if(!ph1) ph1 = ph;
			ph2 = ph;
		}

		if(!ph1 || !ph2) {
			printf("skipping empty region 0x%lx-0x%lx\n",
				vr->vaddr, vr->vaddr+vr->length);
			continue;
		}

		/* Report start+length of region starting from lowest use. */
		vri->vri_addr = vr->vaddr + ph1->offset;
		vri->vri_prot = PROT_READ;
		vri->vri_length = ph2->offset + VM_PAGE_SIZE - ph1->offset;

		/* "AND" the provided protection with per-page protection. */
		if (vr->flags & VR_WRITABLE)
			vri->vri_prot |= PROT_WRITE;
		count++;
		vri++;
	}

	*nextp = next;
	return count;
}

/*========================================================================*
 *				regionprintstats			  *
 *========================================================================*/
void printregionstats(struct vmproc *vmp)
{
	struct vir_region *vr;
	struct phys_region *pr;
	vir_bytes used = 0, weighted = 0;
	region_iter v_iter;
	region_start_iter_least(&vmp->vm_regions_avl, &v_iter);

	while((vr = region_get_iter(&v_iter))) {
		vir_bytes voffset;
		region_incr_iter(&v_iter);
		if(vr->flags & VR_DIRECT)
			continue;
		for(voffset = 0; voffset < vr->length; voffset+=VM_PAGE_SIZE) {
			if(!(pr = physblock_get(vr, voffset))) continue;
			used += VM_PAGE_SIZE;
			weighted += VM_PAGE_SIZE / pr->ph->refcount;
		}
	}

	printf("%6lukB  %6lukB\n", used/1024, weighted/1024);

	return;
}

void map_setparent(struct vmproc *vmp)
{
	region_iter iter;
	struct vir_region *vr;
        region_start_iter_least(&vmp->vm_regions_avl, &iter);
        while((vr = region_get_iter(&iter))) {
                USE(vr, vr->parent = vmp;);
                region_incr_iter(&iter);
        }
}

int physregions(struct vir_region *vr)
{
	int n =  0;
	vir_bytes voffset;
	for(voffset = 0; voffset < vr->length; voffset += VM_PAGE_SIZE) {
		if(physblock_get(vr, voffset))
			n++;
	}
	return n;
}
