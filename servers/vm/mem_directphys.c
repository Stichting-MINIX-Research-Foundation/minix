
/* This file implements the methods of direct physical mapping.
 * 
 * A direct physical mapping is done by accepting the physical
 * memory address and range from the caller and allowing direct
 * access to it. Most significantly, no physical memory is allocated
 * when it's mapped or freed when it's unmapped. E.g. device memory.
 */

#include "vm.h"

/* These functions are static so as to not pollute the
 * global namespace, and are accessed through their function
 * pointers.
 */

static int phys_unreference(struct phys_region *pr);
static int phys_writable(struct phys_region *pr);
static int phys_pagefault(struct vmproc *vmp, struct vir_region *region,
        struct phys_region *ph, int write, vfs_callback_t cb, void *, int);
static int phys_copy(struct vir_region *vr, struct vir_region *newvr);

struct mem_type mem_type_directphys = {
	.name = "physical memory mapping",
	.ev_copy = phys_copy,
	.ev_unreference = phys_unreference,
	.writable = phys_writable,
	.ev_pagefault = phys_pagefault
};

static int phys_unreference(struct phys_region *pr)
{
	return OK;
}

static int phys_pagefault(struct vmproc *vmp, struct vir_region *region,
    struct phys_region *ph, int write, vfs_callback_t cb, void *st, int len)
{
	phys_bytes arg = region->param.phys, phmem;
	assert(arg != MAP_NONE);
	assert(ph->ph->phys == MAP_NONE);
	phmem = arg + ph->offset;
	assert(phmem != MAP_NONE);
	ph->ph->phys = phmem;
	return OK;
}

static int phys_writable(struct phys_region *pr)
{
        assert(pr->ph->refcount > 0);
        return pr->ph->phys != MAP_NONE;
}

void phys_setphys(struct vir_region *vr, phys_bytes phys)
{
	vr->param.phys = phys;
}

static int phys_copy(struct vir_region *vr, struct vir_region *newvr)
{
	newvr->param.phys = vr->param.phys;

	return OK;
}
