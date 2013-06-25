
/* This file implements the methods of physically contiguous anonymous memory. */

#include <assert.h>

#include "proto.h"
#include "vm.h"
#include "region.h"
#include "glo.h"

static int anon_contig_reference(struct phys_region *, struct phys_region *);
static int anon_contig_unreference(struct phys_region *pr);
static int anon_contig_pagefault(struct vmproc *vmp, struct vir_region *region, 
	struct phys_region *ph, int write, vfs_callback_t cb, void *state,
	int len, int *io);
static int anon_contig_sanitycheck(struct phys_region *pr, char *file, int line);
static int anon_contig_writable(struct phys_region *pr);
static int anon_contig_resize(struct vmproc *vmp, struct vir_region *vr, vir_bytes l);
static int anon_contig_new(struct vir_region *vr);

struct mem_type mem_type_anon_contig = {
	.name = "anonymous memory (physically contiguous)",
	.ev_new = anon_contig_new,
	.ev_reference = anon_contig_reference,
	.ev_unreference = anon_contig_unreference,
	.ev_pagefault = anon_contig_pagefault,
	.ev_resize = anon_contig_resize,
	.ev_sanitycheck = anon_contig_sanitycheck,
	.writable = anon_contig_writable
};

static int anon_contig_pagefault(struct vmproc *vmp, struct vir_region *region,
	struct phys_region *ph, int write, vfs_callback_t cb, void *state,
	int len, int *io)
{
	panic("anon_contig_pagefault: pagefault cannot happen");
}

static int anon_contig_new(struct vir_region *region)
{
        u32_t allocflags;
	phys_bytes new_pages, new_page_cl, cur_ph;
	int p, pages;

        allocflags = vrallocflags(region->flags);

	pages = region->length/VM_PAGE_SIZE;

	assert(physregions(region) == 0);

	for(p = 0; p < pages; p++) {
		struct phys_block *pb = pb_new(MAP_NONE);
		struct phys_region *pr = NULL;
		if(pb)
			pr = pb_reference(pb, p * VM_PAGE_SIZE, region, &mem_type_anon_contig);
		if(!pr) {
			if(pb) pb_free(pb);
			map_free(region);
			return ENOMEM;
		}
	}

	assert(physregions(region) == pages);

	if((new_page_cl = alloc_mem(pages, allocflags)) == NO_MEM) {
		map_free(region);
		return ENOMEM;
	}

	cur_ph = new_pages = CLICK2ABS(new_page_cl);

	for(p = 0; p < pages; p++) {
		struct phys_region *pr = physblock_get(region, p * VM_PAGE_SIZE);
		assert(pr);
		assert(pr->ph);
		assert(pr->ph->phys == MAP_NONE);
		assert(pr->offset == p * VM_PAGE_SIZE);
		pr->ph->phys = cur_ph + pr->offset;
	}

	return OK;
}

static int anon_contig_resize(struct vmproc *vmp, struct vir_region *vr, vir_bytes l)
{
	printf("VM: cannot resize physically contiguous memory.\n");
	return ENOMEM;
}

static int anon_contig_reference(struct phys_region *pr,
	struct phys_region *newpr)
{
	printf("VM: cannot fork with physically contig memory.\n");
	return ENOMEM;
}

/* Methods inherited from the anonymous memory methods. */

static int anon_contig_unreference(struct phys_region *pr)
{
	return mem_type_anon.ev_unreference(pr);
}

static int anon_contig_sanitycheck(struct phys_region *pr, char *file, int line)
{
	return mem_type_anon.ev_sanitycheck(pr, file, line);
}

static int anon_contig_writable(struct phys_region *pr)
{
	return mem_type_anon.writable(pr);
}

