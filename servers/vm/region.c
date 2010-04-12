
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
#include "memlist.h"

/* Should a physblock be mapped writable? */
#define WRITABLE(r, pb) \
	(((r)->flags & (VR_DIRECT | VR_SHARED)) ||	\
	 (((r)->flags & VR_WRITABLE) && (pb)->refcount == 1))

FORWARD _PROTOTYPE(int map_new_physblock, (struct vmproc *vmp,
	struct vir_region *region, vir_bytes offset, vir_bytes length,
	phys_bytes what, u32_t allocflags, int written));

FORWARD _PROTOTYPE(int map_ph_writept, (struct vmproc *vmp, struct vir_region *vr,
	struct phys_region *pr));

FORWARD _PROTOTYPE(struct vir_region *map_copy_region, (struct vmproc *vmp, struct vir_region *vr));

FORWARD _PROTOTYPE(struct phys_region *map_clone_ph_block, (struct vmproc *vmp,
        struct vir_region *region, struct phys_region *ph, physr_iter *iter));

PRIVATE char *map_name(struct vir_region *vr)
{
	static char name[100];
	char *typename, *tag;
	int type = vr->flags & (VR_ANON|VR_DIRECT);
	switch(type) {
		case VR_ANON:
			typename = "anonymous";
			break;
		case VR_DIRECT:
			typename = "direct";
			break;
		default:
			panic("unknown mapping type: %d", type);
	}

	switch(vr->tag) {
		case VRT_TEXT:
			tag = "text";
			break;
		case VRT_STACK:
			tag = "stack";
			break;
		case VRT_HEAP:
			tag = "heap";
			break;
		case VRT_NONE:
			tag = "untagged";
			break;
		default:
			tag = "unknown tag value";
			break;
	}

	sprintf(name, "%s, %s", typename, tag);

	return name;
}

PUBLIC void map_printregion(struct vmproc *vmp, struct vir_region *vr)
{
	physr_iter iter;
	struct phys_region *ph;
	printf("map_printmap: map_name: %s\n", map_name(vr));
	printf("\t%s (len 0x%lx, %dkB), %s\n",
		arch_map2str(vmp, vr->vaddr), vr->length,
			vr->length/1024, map_name(vr));
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
	int r;

	if(!(vmp->vm_flags & VMF_HASPT))
		return OK;

	if(WRITABLE(vr, pb))
		rw = PTF_WRITE;
	else
		rw = 0;

	r = pt_writemap(&vmp->vm_pt, vr->vaddr + pr->offset,
	  pb->phys, pb->length, PTF_PRESENT | PTF_USER | rw, WMF_VERIFY);

	if(r != OK) {
		printf("proc %d phys_region 0x%lx sanity check failed\n",
			vmp->vm_endpoint, pr->offset);
		map_printregion(vmp, vr);
	}

	return r;
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

	assert(!(vr->vaddr % VM_PAGE_SIZE));
	assert(!(pb->length % VM_PAGE_SIZE));
	assert(!(pr->offset % VM_PAGE_SIZE));
	assert(pb->refcount > 0);

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
	assert(vm_paged);

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
			map_printmap(vmp);
                        return (vir_bytes) -1;
                }
        }

	/* Basic input sanity checks. */
	assert(!(length % VM_PAGE_SIZE));
	if(minv >= maxv) {
		printf("VM: 1 minv: 0x%lx maxv: 0x%lx length: 0x%lx\n",
			minv, maxv, length);
	}
	assert(minv < maxv);
	assert(minv + length <= maxv);

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
	if(prevregion) assert(prevregion->vaddr < startv);
#endif

	/* However we got it, startv must be in the requested range. */
	assert(startv >= minv);
	assert(startv < maxv);
	assert(startv + length <= maxv);

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

	assert(!(length % VM_PAGE_SIZE));

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
		assert(what);	/* mapping in 0 is unlikely to be right */
		assert(!(what % VM_PAGE_SIZE));
		assert(!(startv % VM_PAGE_SIZE));
		assert(!(mapflags & MF_PREALLOC));
		if(map_new_physblock(vmp, newregion, 0, length,
			what, PAF_CLEAR, 0) != OK) {
			printf("VM: map_new_physblock failed\n");
			USE(newregion,
				SLABFREE(newregion->phys););
			SLABFREE(newregion);
			return NULL;
		}
	}

	if((flags & VR_ANON) && (mapflags & MF_PREALLOC)) {
		if(map_handle_memory(vmp, newregion, 0, length, 1) != OK) {
			printf("VM: map_page_region: prealloc failed\n");
			USE(newregion,
				SLABFREE(newregion->phys););
			SLABFREE(newregion);
			return NULL;
		}
	}

	/* Link it. */
	if(prevregion) {
		assert(prevregion->vaddr < newregion->vaddr);
		USE(newregion, newregion->next = prevregion->next;);
		USE(prevregion, prevregion->next = newregion;);
	} else {
		USE(newregion, newregion->next = vmp->vm_regions;);
		vmp->vm_regions = newregion;
	}

#if SANITYCHECKS
	assert(startv == newregion->vaddr);
	if(newregion->next) {
		assert(newregion->vaddr < newregion->next->vaddr);
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
	assert(pb->refcount > 0);
	USE(pb, pb->refcount--;);
	assert(pb->refcount >= 0);

	if(pb->firstregion == pr) {
		USE(pb, pb->firstregion = pr->next_ph_list;);
	} else {
		struct phys_region *others;

		for(others = pb->firstregion; others;
			others = others->next_ph_list) {
			assert(others->ph == pb);
			if(others->next_ph_list == pr) {
				USE(others, others->next_ph_list = pr->next_ph_list;);
				break;
			}
		}

		assert(others); /* Otherwise, wasn't on the list. */
	}

	if(pb->refcount == 0) {
		assert(!pb->firstregion);
		if(region->flags & VR_ANON) {
			free_mem(ABS2CLICK(pb->phys),
				ABS2CLICK(pb->length));
		} else if(region->flags & VR_DIRECT) {
			; /* No action required. */
		} else {
			panic("strange phys flags");
		}
		SLABFREE(pb);
	} else {
		struct phys_region *others;
		int n = 0;

		for(others = pb->firstregion; others;
			others = others->next_ph_list) {
			if(WRITABLE(region, others->ph)) {
				if(map_ph_writept(others->parent->parent,
					others->parent, others) != OK) {
					printf("VM: map_ph_writept failed unexpectedly\n");
				}
			} 
			n++;
		}
		assert(n == pb->refcount);
	}
}

PRIVATE struct phys_region *reset_physr_iter(struct vir_region *region,
	physr_iter *iter, vir_bytes offset)
{
	struct phys_region *ph;

	physr_start_iter(region->phys, iter, offset, AVL_EQUAL);
	ph = physr_get_iter(iter);
	assert(ph);
	assert(ph->offset == offset);

	return ph;
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
			assert(others->ph == pb);
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
			assert(len > pr->offset);
			assert(len < pr->offset + pr->ph->length);
			assert(pr->ph->refcount > 0);
			sublen = len - pr->offset;
			assert(!(sublen % VM_PAGE_SIZE));
			assert(sublen < pr->ph->length);
			if(pr->ph->refcount > 1) {
				int r;
				if(!(pr = map_clone_ph_block(vmp, region,
					pr, &iter)))
					return ENOMEM;
			}
			assert(pr->ph->refcount == 1);
			if(!(region->flags & VR_DIRECT)) {
				free_mem(ABS2CLICK(pr->ph->phys), ABS2CLICK(sublen));
			}
			USE(pr, pr->offset += sublen;);
			USE(pr->ph,
				pr->ph->phys += sublen;
				pr->ph->length -= sublen;);
			assert(!(pr->offset % VM_PAGE_SIZE));
			assert(!(pr->ph->phys % VM_PAGE_SIZE));
			assert(!(pr->ph->length % VM_PAGE_SIZE));
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

	if((r=map_subfree(vmp, region, region->length)) != OK) {
		printf("%d\n", __LINE__);
		return r;
	}

	USE(region,
		SLABFREE(region->phys););
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
		panic("process has no regions: %d", vmp->vm_endpoint);

	for(r = vmp->vm_regions; r; r = r->next) {
		if(offset >= r->vaddr && offset < r->vaddr + r->length)
			return r;
	}

	SANITYCHECK(SCL_FUNCTIONS);

	return NULL;
}

PRIVATE u32_t vrallocflags(u32_t flags)
{
	u32_t allocflags = 0;

	if(flags & VR_PHYS64K)
		allocflags |= PAF_ALIGN64K;
	if(flags & VR_LOWER16MB)
		allocflags |= PAF_LOWER16MB;
	if(flags & VR_LOWER1MB)
		allocflags |= PAF_LOWER1MB;
	if(flags & VR_CONTIG)
		allocflags |= PAF_CONTIG;

	return allocflags;
}

/*===========================================================================*
 *				map_new_physblock			     *
 *===========================================================================*/
PRIVATE int map_new_physblock(vmp, region, start_offset, length,
	what_mem, allocflags, written)
struct vmproc *vmp;
struct vir_region *region;
vir_bytes start_offset;
vir_bytes length;
phys_bytes what_mem;
u32_t allocflags;
int written;
{
	struct memlist *memlist, given, *ml;
	int used_memlist, r;
	vir_bytes mapped = 0;
	vir_bytes offset = start_offset;

	SANITYCHECK(SCL_FUNCTIONS);

	assert(!(length % VM_PAGE_SIZE));

	if((region->flags & VR_CONTIG) &&
		(start_offset > 0 || length < region->length)) {
		printf("VM: map_new_physblock: non-full allocation requested\n");
		return EFAULT;
	}

	/* Memory for new physical block. */
	if(what_mem == MAP_NONE) {
		allocflags |= vrallocflags(region->flags);

		if(!(memlist = alloc_mem_in_list(length, allocflags))) {
			printf("map_new_physblock: couldn't allocate\n");
			return ENOMEM;
		}
		used_memlist = 1;
	} else {
		given.phys = what_mem;
		given.length = length;
		given.next = NULL;
		memlist = &given;
		used_memlist = 0;
		assert(given.phys);
		assert(given.length);
	}

	r = OK;

	for(ml = memlist; ml; ml = ml->next) {
		assert(ml->phys);
		assert(ml->length);
	}

	for(ml = memlist; ml; ml = ml->next) {
		struct phys_region *newphysr = NULL;
		struct phys_block *newpb = NULL;

		/* Allocate things necessary for this chunk of memory. */
		if(!SLABALLOC(newphysr) || !SLABALLOC(newpb)) {
			printf("map_new_physblock: no memory for the ph slabs\n");
			if(newphysr) SLABFREE(newphysr);
			if(newpb) SLABFREE(newpb);
			r = ENOMEM;
			break;
		}

		assert(ml->phys);
		assert(ml->length);

		/* New physical block. */
		assert(!(ml->phys % VM_PAGE_SIZE));

		USE(newpb,
		newpb->phys = ml->phys;
		newpb->refcount = 1;
		newpb->length = ml->length;
		newpb->firstregion = newphysr;);

		/* New physical region. */
		USE(newphysr,
		newphysr->offset = offset;
		newphysr->ph = newpb;
		newphysr->parent = region;
		/* No other references to this block. */
		newphysr->next_ph_list = NULL;);
#if SANITYCHECKS
		USE(newphysr, newphysr->written = written;);
#endif

		/* Update pagetable. */
		if(map_ph_writept(vmp, region, newphysr) != OK) {
			printf("map_new_physblock: map_ph_writept failed\n");
			r = ENOMEM;
			break;
		}
	
		physr_insert(region->phys, newphysr);

		offset += ml->length;
		mapped += ml->length;
	}

	if(used_memlist) {
		if(r != OK) {
			offset = start_offset;
			/* Things did not go well. Undo everything. */
			for(ml = memlist; ml; ml = ml->next) {
				struct phys_region *physr;
				offset += ml->length;
				if((physr = physr_search(region->phys, offset,
					AVL_EQUAL))) {
					assert(physr->ph->refcount == 1);
					pb_unreferenced(region, physr);
					physr_remove(region->phys, physr->offset);
					SLABFREE(physr);
				}
			}
		} else assert(mapped == length);

		/* Always clean up the memlist itself, even if everything
		 * worked we're not using the memlist nodes any more. And
		 * the memory they reference is either freed above or in use.
		 */
		free_mem_list(memlist, 0);
	}

	SANITYCHECK(SCL_FUNCTIONS);

	return r;
}

/*===========================================================================*
 *				map_clone_ph_block			     *
 *===========================================================================*/
PRIVATE struct phys_region *map_clone_ph_block(vmp, region, ph, iter)
struct vmproc *vmp;
struct vir_region *region;
struct phys_region *ph;
physr_iter *iter;
{
	vir_bytes offset, length;
	struct memlist *ml;
	u32_t allocflags;
	phys_bytes physaddr;
	struct phys_region *newpr;
	int written = 0; 
#if SANITYCHECKS
	written = ph->written;
#endif
	SANITYCHECK(SCL_FUNCTIONS);

	/* Warning: this function will free the passed
	 * phys_region *ph and replace it (in the same offset)
	 * with one or more others! So both the pointer to it
	 * and any iterators over the phys_regions in the vir_region
	 * will be invalid on successful return. (Iterators over
	 * the vir_region could be invalid on unsuccessful return too.)
	 */

	/* This function takes a physical block, copies its contents
	 * into newly allocated memory, and replaces the single physical
	 * block by one or more physical blocks with refcount 1 with the
	 * same contents as the original. In other words, a fragmentable
	 * version of map_copy_ph_block().
	 */

	/* Remember where and how much. */
	offset = ph->offset;
	length = ph->ph->length;
	physaddr = ph->ph->phys;

	/* Now unlink the original physical block so we can replace
	 * it with new ones.
	 */

	SANITYCHECK(SCL_DETAIL);
	SLABSANE(ph);
	SLABSANE(ph->ph);
	assert(ph->ph->refcount > 1);
	pb_unreferenced(region, ph);
	assert(ph->ph->refcount >= 1);
	physr_remove(region->phys, offset);
	SLABFREE(ph);

	SANITYCHECK(SCL_DETAIL);

	/* Put new free memory in. */
	allocflags = vrallocflags(region->flags);
	assert(!(allocflags & PAF_CONTIG));
	assert(!(allocflags & PAF_CLEAR));

	if(map_new_physblock(vmp, region, offset, length,
		MAP_NONE, allocflags, written) != OK) {
		/* XXX original range now gone. */
		free_mem_list(ml, 0);
		printf("VM: map_clone_ph_block: map_new_physblock failed.\n");
		return NULL;
	}

	/* Copy the block to the new memory.
	 * Can only fail if map_new_physblock didn't do what we asked.
	 */
	if(copy_abs2region(physaddr, region, offset, length) != OK)
		panic("copy_abs2region failed, no good reason for that");

	newpr = physr_search(region->phys, offset, AVL_EQUAL);
	assert(newpr);
	assert(newpr->offset == offset);

	if(iter) {
		physr_start_iter(region->phys, iter, offset, AVL_EQUAL);
		assert(physr_get_iter(iter) == newpr);
	}

	SANITYCHECK(SCL_FUNCTIONS);

	return newpr;
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

	assert(offset >= 0);
	assert(offset < region->length);

	assert(region->flags & VR_ANON);
	assert(!(region->vaddr % VM_PAGE_SIZE));

	virpage = offset - offset % VM_PAGE_SIZE;

	SANITYCHECK(SCL_FUNCTIONS);

	if((ph = physr_search(region->phys, offset, AVL_LESS_EQUAL)) &&
	   (ph->offset <= offset && offset < ph->offset + ph->ph->length)) {
		phys_bytes blockoffset = ph->offset;
		/* Pagefault in existing block. Do copy-on-write. */
		assert(write);
		assert(region->flags & VR_WRITABLE);
		assert(ph->ph->refcount > 0);

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
				if(!map_clone_ph_block(vmp, region, ph, NULL))
					r = ENOMEM;
			}
		}
	} else {
		/* Pagefault in non-existing block. Map in new block. */
		if(map_new_physblock(vmp, region, virpage,
			VM_PAGE_SIZE, MAP_NONE, PAF_CLEAR, 0) != OK) {
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
	if(OK != pt_checkrange(&vmp->vm_pt, region->vaddr+virpage,
		VM_PAGE_SIZE, write)) {
		panic("map_pf: pt_checkrange failed: %d", r);
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
		if(map_new_physblock(vmp, region, start,		\
			end-start, MAP_NONE, PAF_CLEAR, 0) != OK) {	\
			SANITYCHECK(SCL_DETAIL);			\
			return ENOMEM;					\
		}							\
		changes++;						\
	} }


	SANITYCHECK(SCL_FUNCTIONS);

	assert(region->flags & VR_ANON);
	assert(!(region->vaddr % VM_PAGE_SIZE));
	assert(!(offset % VM_PAGE_SIZE));
	assert(!(length % VM_PAGE_SIZE));
	assert(!write || (region->flags & VR_WRITABLE));

	physr_start_iter(region->phys, &iter, offset, AVL_LESS_EQUAL);
	physr = physr_get_iter(&iter);

	if(!physr) {
		physr_start_iter(region->phys, &iter, offset, AVL_GREATER_EQUAL);
		physr = physr_get_iter(&iter);
	}

	FREE_RANGE_HERE(NULL, physr);

	if(physr) {
		physr = reset_physr_iter(region, &iter, physr->offset);
		if(physr->offset + physr->ph->length <= offset) {
			physr_incr_iter(&iter);
			physr = physr_get_iter(&iter);

			FREE_RANGE_HERE(NULL, physr);
			if(physr) {
				physr = reset_physr_iter(region, &iter,
					physr->offset);
			}
		}
	}

	while(physr) {
		int r;

		SANITYCHECK(SCL_DETAIL);

		if(write) {
		  assert(physr->ph->refcount > 0);
		  if(!WRITABLE(region, physr->ph)) {
			if(!(physr = map_clone_ph_block(vmp, region,
				physr, &iter))) {
				printf("VM: map_handle_memory: no copy\n");
				return ENOMEM;
			}
			changes++;
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
			nextphysr = reset_physr_iter(region, &iter,
				nextphysr->offset);
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
		panic("checkrange failed");
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
#if SANITYCHECKS
		assert(countregions(vr) == cr);
#endif
		physr_incr_iter(&iter);
	}

#if SANITYCHECKS
	assert(countregions(vr) == countregions(newvr));
#endif

	return newvr;
}

/*===========================================================================*
 *				copy_abs2region			     	*
 *===========================================================================*/
PUBLIC int copy_abs2region(phys_bytes abs, struct vir_region *destregion,
	phys_bytes offset, phys_bytes len)

{
	assert(destregion);
	assert(destregion->phys);
	while(len > 0) {
		phys_bytes sublen, suboffset;
		struct phys_region *ph;
		assert(destregion);
		assert(destregion->phys);
		if(!(ph = physr_search(destregion->phys, offset, AVL_LESS_EQUAL))) {
			printf("VM: copy_abs2region: no phys region found (1).\n");
			return EFAULT;
		}
		assert(ph->offset <= offset);
		if(ph->offset+ph->ph->length <= offset) {
			printf("VM: copy_abs2region: no phys region found (2).\n");
			return EFAULT;
		}
		suboffset = offset - ph->offset;
		assert(suboffset < ph->ph->length);
		sublen = len;
		if(sublen > ph->ph->length - suboffset)
			sublen = ph->ph->length - suboffset;
		assert(suboffset + sublen <= ph->ph->length);
		if(ph->ph->refcount != 1) {
			printf("VM: copy_abs2region: no phys region found (3).\n");
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
			assert(new_ph);
			assert(orig_ph);
			assert(orig_ph != new_ph);
			pb = orig_ph->ph;
			assert(pb == new_ph->ph);

			/* Link in new physregion. */
			assert(!new_ph->next_ph_list);
			USE(new_ph, new_ph->next_ph_list = pb->firstregion;);
			USE(pb, pb->firstregion = new_ph;);

			/* Increase phys block refcount */
			assert(pb->refcount > 0);
			USE(pb, pb->refcount++;);
			assert(pb->refcount > 1);

			/* If the phys block has been shared as SMAP,
			 * do the regular copy. */
			if(pb->refcount > 2 && pb->share_flag == PBSH_SMAP) {
				map_clone_ph_block(dst, newvr,new_ph,
					&iter_new);
			} else {
				USE(pb, pb->share_flag = PBSH_COW;);
			}

			/* Get next new physregion */
			physr_incr_iter(&iter_orig);
			physr_incr_iter(&iter_new);
		}
		assert(!physr_get_iter(&iter_new));
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
	assert(!vmp->vm_regions);
	assert(vmproc[VMP_SYSTEM].vm_flags & VMF_INUSE);
	assert(!(KERNEL_TEXT % VM_PAGE_SIZE));
	assert(!(KERNEL_TEXT_LEN % VM_PAGE_SIZE));
	assert(!(KERNEL_DATA % VM_PAGE_SIZE));
	assert(!(KERNEL_DATA_LEN % VM_PAGE_SIZE));

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

	assert(vr);
	assert(vr->flags & VR_ANON);
	assert(!(delta % VM_PAGE_SIZE));

	if(!delta) return OK;
	end = vr->vaddr + vr->length;
	assert(end >= vr->vaddr);

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
	assert(vr);
	assert(vr->flags & VR_ANON);
	assert(!(delta % VM_PAGE_SIZE));

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
		panic("map_unmap_region: region not found");

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
			assert(pr->offset >= len);
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

	assert(region->flags & VR_SHARED);

	/* da is handled differently */
	if (!da)
		dst_addr = dvmp->vm_stacktop;
	else
		dst_addr = da;
	dst_addr = arch_vir2map(dvmp, dst_addr);

	prev = NULL;
	/* round up to page size */
	assert(!(size % VM_PAGE_SIZE));
	startv = region_find_slot(dvmp, dst_addr, VM_DATATOP, size, &prev);
	if (startv == (vir_bytes) -1) {
		printf("map_remap: search 0x%x...\n", dst_addr);
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
	assert(vr->flags & VR_SHARED);

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
		assert(!ph->next_ph_list);
		USE(ph, ph->next_ph_list = pb->firstregion;);
		USE(pb, pb->firstregion = ph;);
		USE(pb, pb->refcount++;);
		if(map_ph_writept(dvmp, vr, ph) != OK) {
			panic("map_remap: map_ph_writept failed");
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

	assert(ph);
	assert(ph->ph);
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

	assert(ph);
	assert(ph->ph);
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

/*===========================================================================*
 *				do_map_memory				     *
 *===========================================================================*/
PRIVATE int do_map_memory(struct vmproc *vms, struct vmproc *vmd,
	struct vir_region *vrs, struct vir_region *vrd,
	vir_bytes offset_s, vir_bytes offset_d,
	vir_bytes length, int flag)
{
	struct phys_region *prs;
	struct phys_region *newphysr;
	struct phys_block *pb;
	physr_iter iter;
	u32_t pt_flag = PTF_PRESENT | PTF_USER;
	vir_bytes end;

	SANITYCHECK(SCL_FUNCTIONS);

	/* Search for the first phys region in the source process. */
	physr_start_iter(vrs->phys, &iter, offset_s, AVL_EQUAL);
	prs = physr_get_iter(&iter);
	if(!prs)
		panic("do_map_memory: no aligned phys region: %d", 0);

	/* flag: 0 -> read-only
	 *       1 -> writable
	 *      -1 -> share as COW, so read-only
	 */
	if(flag > 0)
		pt_flag |= PTF_WRITE;

	/* Map phys blocks in the source process to the destination process. */
	end = offset_d + length;
	while((prs = physr_get_iter(&iter)) && offset_d < end) {
		/* If a SMAP share was requested but the phys block has already
		 * been shared as COW, copy the block for the source phys region
		 * first.
		 */
		pb = prs->ph;
		if(flag >= 0 && pb->refcount > 1
			&& pb->share_flag == PBSH_COW) {
			if(!(prs = map_clone_ph_block(vms, vrs, prs, &iter)))
				return ENOMEM;
			pb = prs->ph;
		}

		/* Allocate a new phys region. */
		if(!SLABALLOC(newphysr))
			return ENOMEM;

		/* Set and link the new phys region to the block. */
		newphysr->ph = pb;
		newphysr->offset = offset_d;
		newphysr->parent = vrd;
		newphysr->next_ph_list = pb->firstregion;
		pb->firstregion = newphysr;
		physr_insert(newphysr->parent->phys, newphysr);
		pb->refcount++;

		/* If a COW share was requested but the phys block has already
		 * been shared as SMAP, give up on COW and copy the block for
		 * the destination phys region now.
		 */
		if(flag < 0 && pb->refcount > 1
			&& pb->share_flag == PBSH_SMAP) {
			if(!(newphysr = map_clone_ph_block(vmd, vrd,
				newphysr, NULL))) {
				return ENOMEM;
			}
		}
		else {
			/* See if this is a COW share or SMAP share. */
			if(flag < 0) {			/* COW share */
				pb->share_flag = PBSH_COW;
				/* Update the page table for the src process. */
				pt_writemap(&vms->vm_pt, offset_s + vrs->vaddr,
					pb->phys, pb->length,
					pt_flag, WMF_OVERWRITE);
			}
			else {				/* SMAP share */
				pb->share_flag = PBSH_SMAP;
			}
			/* Update the page table for the destination process. */
			pt_writemap(&vmd->vm_pt, offset_d + vrd->vaddr,
				pb->phys, pb->length, pt_flag, WMF_OVERWRITE);
		}

		physr_incr_iter(&iter);
		offset_d += pb->length;
		offset_s += pb->length;
	}

	SANITYCHECK(SCL_FUNCTIONS);

	return OK;
}

/*===========================================================================*
 *				unmap_memory				     *
 *===========================================================================*/
PUBLIC int unmap_memory(endpoint_t sour, endpoint_t dest,
	vir_bytes virt_s, vir_bytes virt_d, vir_bytes length, int flag)
{
	struct vmproc *vmd;
	struct vir_region *vrd;
	struct phys_region *pr;
	struct phys_block *pb;
	physr_iter iter;
	vir_bytes off, end;
	int p;

	/* Use information on the destination process to unmap. */
	if(vm_isokendpt(dest, &p) != OK)
		panic("unmap_memory: bad endpoint: %d", dest);
	vmd = &vmproc[p];

	vrd = map_lookup(vmd, virt_d);
	assert(vrd);

	/* Search for the first phys region in the destination process. */
	off = virt_d - vrd->vaddr;
	physr_start_iter(vrd->phys, &iter, off, AVL_EQUAL);
	pr = physr_get_iter(&iter);
	if(!pr)
		panic("unmap_memory: no aligned phys region: %d", 0);

	/* Copy the phys block now rather than doing COW. */
	end = off + length;
	while((pr = physr_get_iter(&iter)) && off < end) {
		pb = pr->ph;
		assert(pb->refcount > 1);
		assert(pb->share_flag == PBSH_SMAP);

		if(!(pr = map_clone_ph_block(vmd, vrd, pr, &iter)))
			return ENOMEM;

		physr_incr_iter(&iter);
		off += pb->length;
	}

	return OK;
}

/*===========================================================================*
 *				split_phys				     *
 *===========================================================================*/
PRIVATE int split_phys(struct phys_region *pr, vir_bytes point)
{
	struct phys_region *newpr, *q, *prev;
	struct phys_block *newpb;
	struct phys_block *pb = pr->ph;
/* Split the phys region into 2 parts by @point. */

	if(pr->offset >= point || pr->offset + pb->length <= point)
		return OK;
	if(!SLABALLOC(newpb))
		return ENOMEM;

	/* Split phys block. */
	*newpb = *pb;
	pb->length = point - pr->offset;
	newpb->length -= pb->length;
	newpb->phys += pb->length;

	/* Split phys regions in a list. */
	for(q = pb->firstregion; q; q = q->next_ph_list) {
		if(!SLABALLOC(newpr))
			return ENOMEM;

		*newpr = *q;
		newpr->ph = newpb;
		newpr->offset += pb->length;

		/* Link to the vir region's phys region list. */
		physr_insert(newpr->parent->phys, newpr);

		/* Link to the next_ph_list. */
		if(q == pb->firstregion) {
			newpb->firstregion = newpr;
			prev = newpr;
		} else {
			prev->next_ph_list = newpr;
			prev = newpr;
		}
	}
	prev->next_ph_list = NULL;

	return OK;
}

/*===========================================================================*
 *				clean_phys_regions			     *
 *===========================================================================*/
PRIVATE void clean_phys_regions(struct vir_region *region,
	vir_bytes offset, vir_bytes length)
{
/* Consider @offset as the start address and @offset+length as the end address.
 * If there are phys regions crossing the start address or the end address,
 * split them into 2 parts.
 *
 * We assume that the phys regions are listed in order and don't overlap.
 */
	struct phys_region *pr;
	physr_iter iter;

	physr_start_iter_least(region->phys, &iter);
	while((pr = physr_get_iter(&iter))) {
		/* If this phys region crosses the start address, split it. */
		if(pr->offset < offset
			&& pr->offset + pr->ph->length > offset) {
			split_phys(pr, offset);
			physr_start_iter_least(region->phys, &iter);
		}
		/* If this phys region crosses the end address, split it. */
		else if(pr->offset < offset + length
			&& pr->offset + pr->ph->length > offset + length) {
			split_phys(pr, offset + length);
			physr_start_iter_least(region->phys, &iter);
		}
		else {
			physr_incr_iter(&iter);
		}
	}
}

/*===========================================================================*
 *				rm_phys_regions				     *
 *===========================================================================*/
PRIVATE void rm_phys_regions(struct vir_region *region,
	vir_bytes begin, vir_bytes length)
{
/* Remove all phys regions between @begin and @begin+length.
 *
 * Don't update the page table, because we will update it at map_memory()
 * later.
 */
	struct phys_region *pr;
	physr_iter iter;

	physr_start_iter(region->phys, &iter, begin, AVL_GREATER_EQUAL);
	while((pr = physr_get_iter(&iter)) && pr->offset < begin + length) {
		pb_unreferenced(region, pr);
		physr_remove(region->phys, pr->offset);
		physr_start_iter(region->phys, &iter, begin,
			AVL_GREATER_EQUAL);
		SLABFREE(pr);
	}
}

/*===========================================================================*
 *				map_memory				     *
 *===========================================================================*/
PUBLIC int map_memory(endpoint_t sour, endpoint_t dest,
	vir_bytes virt_s, vir_bytes virt_d, vir_bytes length, int flag)
{
/* This is the entry point. This function will be called by handle_memory() when
 * VM recieves a map-memory request.
 */
	struct vmproc *vms, *vmd;
	struct vir_region *vrs, *vrd;
	physr_iter iterd;
	vir_bytes offset_s, offset_d;
	int p;
	int r;

	if(vm_isokendpt(sour, &p) != OK)
		panic("map_memory: bad endpoint: %d", sour);
	vms = &vmproc[p];
	if(vm_isokendpt(dest, &p) != OK)
		panic("map_memory: bad endpoint: %d", dest);
	vmd = &vmproc[p];

	vrs = map_lookup(vms, virt_s);
	assert(vrs);
	vrd = map_lookup(vmd, virt_d);
	assert(vrd);

	/* Linear address -> offset from start of vir region. */
	offset_s = virt_s - vrs->vaddr;
	offset_d = virt_d - vrd->vaddr;

	/* Make sure that the range in the source process has been mapped
	 * to physical memory.
	 */
	map_handle_memory(vms, vrs, offset_s, length, 0);

	/* Prepare work. */
	clean_phys_regions(vrs, offset_s, length);
	clean_phys_regions(vrd, offset_d, length);
	rm_phys_regions(vrd, offset_d, length);

	/* Map memory. */
	r = do_map_memory(vms, vmd, vrs, vrd, offset_s, offset_d, length, flag);

	return r;
}

/*========================================================================*
 *				map_lookup_phys			  	*
 *========================================================================*/
phys_bytes
map_lookup_phys(struct vmproc *vmp, u32_t tag)
{
	struct vir_region *vr;
	struct phys_region *pr;
	physr_iter iter;

	if(!(vr = map_region_lookup_tag(vmp, tag))) {
		printf("VM: request for phys of missing region\n");
		return MAP_NONE;
	}

	physr_start_iter_least(vr->phys, &iter);

	if(!(pr = physr_get_iter(&iter))) {
		printf("VM: request for phys of unmapped region\n");
		return MAP_NONE;
	}

	if(pr->offset != 0 || pr->ph->length != vr->length) {
		printf("VM: request for phys of partially mapped region\n");
		return MAP_NONE;
	}

	return pr->ph->phys;
}




