
/* This file implements the methods of memory-mapped files. */

#include <assert.h>

#include "proto.h"
#include "vm.h"
#include "region.h"
#include "glo.h"
#include "cache.h"

/* These functions are static so as to not pollute the
 * global namespace, and are accessed through their function
 * pointers.
 */

static void mappedfile_split(struct vmproc *vmp, struct vir_region *vr,
	struct vir_region *r1, struct vir_region *r2);
static int mappedfile_unreference(struct phys_region *pr);
static int mappedfile_pagefault(struct vmproc *vmp, struct vir_region *region, 
       struct phys_region *ph, int write, vfs_callback_t callback, void *state,
       int len, int *io);
static int mappedfile_sanitycheck(struct phys_region *pr, const char *file, int line);
static int mappedfile_writable(struct phys_region *pr);
static int mappedfile_copy(struct vir_region *vr, struct vir_region *newvr);
static int mappedfile_lowshrink(struct vir_region *vr, vir_bytes len);
static void mappedfile_delete(struct vir_region *region);
static int mappedfile_pt_flags(struct vir_region *vr);

struct mem_type mem_type_mappedfile = {
	.name = "file-mapped memory",
	.ev_unreference = mappedfile_unreference,
	.ev_pagefault = mappedfile_pagefault,
	.ev_sanitycheck = mappedfile_sanitycheck,
	.ev_copy = mappedfile_copy,
	.writable = mappedfile_writable,
	.ev_split = mappedfile_split,
	.ev_lowshrink = mappedfile_lowshrink,
	.ev_delete = mappedfile_delete,
	.pt_flags = mappedfile_pt_flags,
};

static int mappedfile_pt_flags(struct vir_region *vr){
#if defined(__arm__)
	return ARM_VM_PTE_CACHED;
#else
	return 0;
#endif
}

static int mappedfile_unreference(struct phys_region *pr)
{
	assert(pr->ph->refcount == 0);
	if(pr->ph->phys != MAP_NONE)
		free_mem(ABS2CLICK(pr->ph->phys), 1);
	return OK;
}

static int cow_block(struct vmproc *vmp, struct vir_region *region,
	struct phys_region *ph, u16_t clearend)
{
	int r;

	if((r=mem_cow(region, ph, MAP_NONE, MAP_NONE)) != OK) {
		printf("mappedfile_pagefault: COW failed\n");
		return r;
	}

	/* After COW we are a normal piece of anonymous memory. */
	ph->memtype = &mem_type_anon;

	if(clearend) {
		phys_bytes phaddr = ph->ph->phys, po = VM_PAGE_SIZE-clearend;
		assert(clearend < VM_PAGE_SIZE);
		phaddr += po;
		if(sys_memset(NONE, 0, phaddr, clearend) != OK) {
			panic("cow_block: clearend failed\n");
		}
	}

	return OK;
}

static int mappedfile_pagefault(struct vmproc *vmp, struct vir_region *region,
	struct phys_region *ph, int write, vfs_callback_t cb,
	void *state, int statelen, int *io)
{
	u32_t allocflags;
	int procfd = region->param.file.fdref->fd;

	allocflags = vrallocflags(region->flags);

	assert(ph->ph->refcount > 0);
	assert(region->param.file.inited);
	assert(region->param.file.fdref);
	assert(region->param.file.fdref->dev != NO_DEV);

	/* Totally new block? Create it. */
	if(ph->ph->phys == MAP_NONE) {
		struct cached_page *cp;
		u64_t referenced_offset =
			region->param.file.offset + ph->offset;
		if(region->param.file.fdref->ino == VMC_NO_INODE) {
			cp = find_cached_page_bydev(region->param.file.fdref->dev,
				referenced_offset, VMC_NO_INODE, 0, 1);
		} else {
			cp = find_cached_page_byino(region->param.file.fdref->dev,
				region->param.file.fdref->ino, referenced_offset, 1);
		}
		/*
		 * Normally, a cache hit saves a round-trip to the file system
		 * to load the page.  However, if the page in the VM cache is
		 * marked for one-time use, then force a round-trip through the
		 * file system anyway, so that the FS can update the page by
		 * by readding it to the cache.  Thus, for one-time use pages,
		 * no caching is performed.  This approach is correct even in
		 * the light of concurrent requests and disappearing processes
		 * but relies on VM requests to VFS being fully serialized.
		 */
		if(cp && (!cb || !(cp->flags & VMSF_ONCE))) {
			int result = OK;
			pb_unreferenced(region, ph, 0);
			pb_link(ph, cp->page, ph->offset, region);

			if(roundup(ph->offset+region->param.file.clearend,
				VM_PAGE_SIZE) >= region->length) {
				result = cow_block(vmp, region, ph,
					region->param.file.clearend);
			} else if(result == OK && write) {
				result = cow_block(vmp, region, ph, 0);
			}

			/* Discard one-use pages after mapping them in. */
			if (result == OK && (cp->flags & VMSF_ONCE))
				rmcache(cp);

			return result;
		}

		if(!cb) {
#if 0
			printf("VM: mem_file: no callback, returning EFAULT\n");
			sys_diagctl_stacktrace(vmp->vm_endpoint);
#endif
			return EFAULT;
		}

                if(vfs_request(VMVFSREQ_FDIO, procfd, vmp, referenced_offset,
			VM_PAGE_SIZE, cb, NULL, state, statelen) != OK) {
			printf("VM: mappedfile_pagefault: vfs_request failed\n");
			return ENOMEM;
		}
		*io = 1;
		return SUSPEND;
	}

	if(!write) {
#if 0
		printf("mappedfile_pagefault: nonwrite fault?\n");
#endif
		return OK;
	}

	return cow_block(vmp, region, ph, 0);
}

static int mappedfile_sanitycheck(struct phys_region *pr, const char *file, int line)
{
	MYASSERT(usedpages_add(pr->ph->phys, VM_PAGE_SIZE) == OK);
	return OK;
}

static int mappedfile_writable(struct phys_region *pr)
{
	/* We are never writable. */
	return 0;
}

int mappedfile_copy(struct vir_region *vr, struct vir_region *newvr)
{
	assert(vr->param.file.inited);
	mappedfile_setfile(newvr->parent, newvr, vr->param.file.fdref->fd,
		vr->param.file.offset,
		vr->param.file.fdref->dev, vr->param.file.fdref->ino,
		vr->param.file.clearend, 0, 0);
	assert(newvr->param.file.inited);

	return OK;
}

int mappedfile_setfile(struct vmproc *owner,
	struct vir_region *region, int fd, u64_t offset,
	dev_t dev, ino_t ino, u16_t clearend, int prefill, int mayclosefd)
{
	vir_bytes vaddr;
	struct fdref *newref;

	newref = fdref_dedup_or_new(owner, ino, dev, fd, mayclosefd);

	assert(newref);
	assert(!region->param.file.inited);
	assert(dev != NO_DEV);
	fdref_ref(newref, region);
	region->param.file.offset = offset;
	region->param.file.clearend = clearend;
	region->param.file.inited = 1;

	if(!prefill) return OK;

	for(vaddr = 0; vaddr < region->length; vaddr+=VM_PAGE_SIZE) {
		struct cached_page *cp = NULL;
		struct phys_region *pr;
		u64_t referenced_offset = offset + vaddr;

		if(roundup(vaddr+region->param.file.clearend,
			VM_PAGE_SIZE) >= region->length) {
			break;
		}

		if(ino == VMC_NO_INODE) {
			cp = find_cached_page_bydev(dev, referenced_offset,
			  	VMC_NO_INODE, 0, 1);
		} else {
			cp = find_cached_page_byino(dev, ino,
				referenced_offset, 1);
		}
		/*
		 * If we get a hit for a page that is to be used only once,
		 * then either we found a stale page (due to a process dying
		 * before a requested once-page could be mapped in) or this is
		 * a rare case of concurrent requests for the same page.  In
		 * both cases, force the page to be obtained from its FS later.
		 */
		if(!cp || (cp->flags & VMSF_ONCE)) continue;
		if(!(pr = pb_reference(cp->page, vaddr, region,
			&mem_type_mappedfile))) {
			printf("mappedfile_setfile: pb_reference failed\n");
			break;
		}
		if(map_ph_writept(region->parent, region, pr) != OK) {
			printf("mappedfile_setfile: map_ph_writept failed\n");
			break;
		}
	}

	return OK;
}

static void mappedfile_split(struct vmproc *vmp, struct vir_region *vr,
	struct vir_region *r1, struct vir_region *r2)
{
	assert(!r1->param.file.inited);
	assert(!r2->param.file.inited);
	assert(vr->param.file.inited);
	assert(r1->length + r2->length == vr->length);
	assert(vr->def_memtype == &mem_type_mappedfile);
	assert(r1->def_memtype == &mem_type_mappedfile);
	assert(r2->def_memtype == &mem_type_mappedfile);

	r1->param.file = vr->param.file;
	r2->param.file = vr->param.file;

	fdref_ref(vr->param.file.fdref, r1);
	fdref_ref(vr->param.file.fdref, r2);

	r1->param.file.clearend = 0;
	r2->param.file.offset += r1->length;

	assert(r1->param.file.inited);
	assert(r2->param.file.inited);
}

static int mappedfile_lowshrink(struct vir_region *vr, vir_bytes len)
{
	assert(vr->param.file.inited);
	vr->param.file.offset += len;
	return OK;
}

static void mappedfile_delete(struct vir_region *region)
{
	assert(region->def_memtype == &mem_type_mappedfile);
	assert(region->param.file.inited);
	assert(region->param.file.fdref);
	fdref_deref(region);
	region->param.file.inited = 0;
}
