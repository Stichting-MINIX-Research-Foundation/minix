
/* This file implements the methods of anonymous memory.
 * 
 * Anonymous memory is memory that is for private use to a process
 * and can not be related to a file (hence anonymous).
 */

#include <assert.h>

#include "proto.h"
#include "vm.h"
#include "region.h"
#include "glo.h"

/* These functions are static so as to not pollute the
 * global namespace, and are accessed through their function
 * pointers.
 */

static void anon_split(struct vmproc *vmp, struct vir_region *vr,
			struct vir_region *r1, struct vir_region *r2);
static int anon_lowshrink(struct vir_region *vr, vir_bytes len);
static int anon_unreference(struct phys_region *pr);
static int anon_pagefault(struct vmproc *vmp, struct vir_region *region, 
	struct phys_region *ph, int write, vfs_callback_t cb, void *, int);
static int anon_sanitycheck(struct phys_region *pr, char *file, int line);
static int anon_writable(struct phys_region *pr);
static int anon_resize(struct vmproc *vmp, struct vir_region *vr, vir_bytes l);
static u32_t anon_regionid(struct vir_region *region);
static int anon_refcount(struct vir_region *vr);

struct mem_type mem_type_anon = {
	.name = "anonymous memory",
	.ev_unreference = anon_unreference,
	.ev_pagefault = anon_pagefault,
	.ev_resize = anon_resize,
	.ev_sanitycheck = anon_sanitycheck,
	.ev_lowshrink = anon_lowshrink,
	.ev_split = anon_split,
	.regionid = anon_regionid,
	.writable = anon_writable,
	.refcount = anon_refcount
};

static int anon_unreference(struct phys_region *pr)
{
	assert(pr->ph->refcount == 0);
	if(pr->ph->phys != MAP_NONE)
		free_mem(ABS2CLICK(pr->ph->phys), 1);
	return OK;
}

static int anon_pagefault(struct vmproc *vmp, struct vir_region *region,
	struct phys_region *ph, int write, vfs_callback_t cb, void *st, int l)
{
	phys_bytes new_page, new_page_cl;
	u32_t allocflags;

	allocflags = vrallocflags(region->flags);

	assert(ph->ph->refcount > 0);

	if((new_page_cl = alloc_mem(1, allocflags)) == NO_MEM)
		return ENOMEM;
	new_page = CLICK2ABS(new_page_cl);

	/* Totally new block? Create it. */
	if(ph->ph->phys == MAP_NONE) {
		ph->ph->phys = new_page;
		assert(ph->ph->phys != MAP_NONE);

		return OK;
	}

	if(ph->ph->refcount < 2 || !write) {
		printf("anon_pagefault: %d refcount, %d write - not handling pagefault\n",
			ph->ph->refcount, write);
		return OK;
	}	

        assert(region->flags & VR_WRITABLE);

	return mem_cow(region, ph, new_page_cl, new_page);
}

static int anon_sanitycheck(struct phys_region *pr, char *file, int line)
{
	MYASSERT(usedpages_add(pr->ph->phys, VM_PAGE_SIZE) == OK);
	return OK;
}

static int anon_writable(struct phys_region *pr)
{
	assert(pr->ph->refcount > 0);
	if(pr->ph->phys == MAP_NONE)
		return 0;
	if(pr->parent->remaps > 0)
		return 1;
	return pr->ph->refcount == 1;
}

static int anon_resize(struct vmproc *vmp, struct vir_region *vr, vir_bytes l)
{
	/* Shrinking not implemented; silently ignored.
	 * (Which is ok for brk().)
	 */
	if(l <= vr->length)
		return OK;

        assert(vr);
        assert(vr->flags & VR_ANON);
        assert(!(l % VM_PAGE_SIZE));

        USE(vr, vr->length = l;);

	return OK;
}

static u32_t anon_regionid(struct vir_region *region)
{
	return region->id;
}

static int anon_lowshrink(struct vir_region *vr, vir_bytes len)
{
	return OK;
}

static int anon_refcount(struct vir_region *vr)
{
        return 1 + vr->remaps;
}

static void anon_split(struct vmproc *vmp, struct vir_region *vr,
			struct vir_region *r1, struct vir_region *r2)
{
	return;
}
