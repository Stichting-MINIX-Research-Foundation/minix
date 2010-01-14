
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

	/* Search for the first phys region in the source process. */
	physr_start_iter(vrs->phys, &iter, offset_s, AVL_EQUAL);
	prs = physr_get_iter(&iter);
	if(!prs)
		vm_panic("map_memory: no aligned phys region.", 0);

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
			map_copy_ph_block(vms, vrs, prs);
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
			map_copy_ph_block(vmd, vrd, newphysr);
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
	struct phys_region *prs, *prd;
	physr_iter iters, iterd;
	vir_bytes offset_s, offset_d;
	int p;

	if(vm_isokendpt(sour, &p) != OK)
		vm_panic("handle_memory: endpoint wrong", sour);
	vms = &vmproc[p];
	if(vm_isokendpt(dest, &p) != OK)
		vm_panic("handle_memory: endpoint wrong", dest);
	vmd = &vmproc[p];

	vrs = map_lookup(vms, virt_s);
	vm_assert(vrs);
	vrd = map_lookup(vmd, virt_d);
	vm_assert(vrd);

	/* Linear address -> offset from start of vir region. */
	offset_s = virt_s - vrs->vaddr;
	offset_d = virt_d - vrd->vaddr;

	/* Make sure that the range in the source process has been mapped
	 * to physical memory.
	 */
	map_handle_memory(vms, vrs, offset_s, length, 0);

	/* Prepare work. */
	#define map_printregion(x, y) (x = x)
	#define printf(x, y, z) (z = z)
	printf("before clean with offset, length: %d, %d\n", offset_s, length);
	map_printregion(vms, vrs);
	clean_phys_regions(vrs, offset_s, length);
	printf("after clean with offset, length: %d, %d\n", offset_s, length);
	map_printregion(vms, vrs);

	printf("before clean with offset, length: %d, %d\n", offset_d, length);
	map_printregion(vmd, vrd);
	clean_phys_regions(vrd, offset_d, length);
	printf("after clean with offset, length: %d, %d\n", offset_d, length);
	map_printregion(vmd, vrd);

	rm_phys_regions(vrd, offset_d, length);
	printf("after rm with offset, length: %d, %d\n", offset_d, length);
	map_printregion(vmd, vrd);

	/* Map memory. */
	do_map_memory(vms, vmd, vrs, vrd, offset_s, offset_d, length, flag);
	printf("after map (dst) with offset, length: %d, %d\n", offset_d, length);
	map_printregion(vmd, vrd);
	#undef map_printregion
	#undef printf

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
		vm_panic("handle_memory: endpoint wrong", dest);
	vmd = &vmproc[p];

	vrd = map_lookup(vmd, virt_d);
	vm_assert(vrd);

	/* Search for the first phys region in the destination process. */
	off = virt_d - vrd->vaddr;
	physr_start_iter(vrd->phys, &iter, off, AVL_EQUAL);
	pr = physr_get_iter(&iter);
	if(!pr)
		vm_panic("map_memory: no aligned phys region.", 0);

	/* Copy the phys block now rather than doing COW. */
	end = off + length;
	while((pr = physr_get_iter(&iter)) && off < end) {
		pb = pr->ph;
		vm_assert(pb->refcount > 1);
		vm_assert(pb->share_flag == PBSH_SMAP);

		map_copy_ph_block(vmd, vrd, pr);

		physr_incr_iter(&iter);
		off += pb->length;
	}

	return OK;
}

