
/* This file implements the methods of shared memory.  */

#include <assert.h>

#include "proto.h"
#include "vm.h"
#include "region.h"
#include "glo.h"

/* These functions are static so as to not pollute the
 * global namespace, and are accessed through their function
 * pointers.
 */

static int shared_unreference(struct phys_region *pr);
static int shared_pagefault(struct vmproc *vmp, struct vir_region *region, 
	struct phys_region *ph, int write, vfs_callback_t cb, void *state,
	int len, int *io);
static int shared_sanitycheck(struct phys_region *pr, char *file, int line);
static int shared_writable(struct phys_region *pr);
static void shared_delete(struct vir_region *region);
static u32_t shared_regionid(struct vir_region *region);
static int shared_copy(struct vir_region *vr, struct vir_region *newvr);
static int shared_refcount(struct vir_region *vr);

struct mem_type mem_type_shared = {
	.name = "shared memory",
	.ev_copy = shared_copy,
	.ev_unreference = shared_unreference,
	.ev_pagefault = shared_pagefault,
	.ev_sanitycheck = shared_sanitycheck,
	.ev_delete = shared_delete,
	.regionid = shared_regionid,
	.refcount = shared_refcount,
	.writable = shared_writable
};

static int shared_unreference(struct phys_region *pr)
{
	return mem_type_anon.ev_unreference(pr);
}

static int getsrc(struct vir_region *region,
	struct vmproc **vmp, struct vir_region **r)
{
	int srcproc;

	if(region->def_memtype != &mem_type_shared) {
		printf("shared region hasn't shared type but %s.\n",
			region->def_memtype->name);
		return EINVAL;
	}

	if(!region->param.shared.ep || !region->param.shared.vaddr) {
		printf("shared region has not defined source region.\n");
		util_stacktrace();
		return EINVAL;
	}

        if(vm_isokendpt((endpoint_t) region->param.shared.ep, &srcproc) != OK) {
		printf("VM: shared memory with missing source process.\n");
		util_stacktrace();
                return EINVAL;
	}

	*vmp = &vmproc[srcproc];

	if(!(*r=map_lookup(*vmp, region->param.shared.vaddr, NULL))) {
		printf("VM: shared memory with missing vaddr 0x%lx.\n",
			region->param.shared.vaddr);
                return EINVAL;
	}

	if((*r)->def_memtype != &mem_type_anon) {
		printf("source region hasn't anon type but %s.\n",
			(*r)->def_memtype->name);
		return EINVAL;
	}

	if(region->param.shared.id != (*r)->id) {
		printf("source region has no matching id\n");
		return EINVAL;
	}

	return OK;
}

static u32_t shared_regionid(struct vir_region *vr)
{
	struct vir_region *src_region;
	struct vmproc *src_vmp;

	if(getsrc(vr, &src_vmp, &src_region) != OK)
		return 0;

	return src_region->id;
}

static void shared_delete(struct vir_region *region)
{
	struct vir_region *src_region;
	struct vmproc *src_vmp;

	if(getsrc(region, &src_vmp, &src_region) != OK)
		return;

	assert(src_region->remaps > 0);
	src_region->remaps--;
}

static int shared_pagefault(struct vmproc *vmp, struct vir_region *region,
	struct phys_region *ph, int write, vfs_callback_t cb,
	void *state, int statelen, int *io)
{
	struct vir_region *src_region;
	struct vmproc *src_vmp;
	struct phys_region *pr;

	if(getsrc(region, &src_vmp, &src_region) != OK) {
		return EINVAL;
	}

	assert(ph->ph->phys == MAP_NONE);
	pb_free(ph->ph);

	if(!(pr = physblock_get(src_region, ph->offset))) {
		int r;
		if((r=map_pf(src_vmp, src_region, ph->offset, write,
			NULL, NULL, 0, io)) != OK)
			return r;
		if(!(pr = physblock_get(src_region, ph->offset))) {
			panic("missing region after pagefault handling");
		}
	}

	pb_link(ph, pr->ph, ph->offset, region);

	return OK;
}

static int shared_sanitycheck(struct phys_region *pr, char *file, int line)
{
	return OK;
}

static int shared_writable(struct phys_region *pr)
{
	assert(pr->ph->refcount > 0);
	return pr->ph->phys != MAP_NONE;
}

void shared_setsource(struct vir_region *vr, endpoint_t ep,
	struct vir_region *src_vr)
{
	struct vmproc *vmp;
	struct vir_region *srcvr;
	int id = src_vr->id;
	vir_bytes vaddr = src_vr->vaddr;

	assert(vr->def_memtype == &mem_type_shared);

	if(!ep || !vaddr || !id) {
		printf("VM: shared_setsource: zero ep/vaddr/id - ignoring\n");
		return;
	}

	vr->param.shared.ep = ep;
	vr->param.shared.vaddr = vaddr;
	vr->param.shared.id = id;

	if(getsrc(vr, &vmp, &srcvr) != OK)
		panic("initial getsrc failed");

	assert(srcvr == src_vr);

	srcvr->remaps++;
}

static int shared_copy(struct vir_region *vr, struct vir_region *newvr)
{
	struct vmproc *vmp;
	struct vir_region *srcvr;

	if(getsrc(vr, &vmp, &srcvr) != OK)
		panic("copy: original getsrc failed");

	shared_setsource(newvr, vr->param.shared.ep, srcvr);

	return OK;
}

static int shared_refcount(struct vir_region *vr)
{
	return 1 + vr->remaps;
}

