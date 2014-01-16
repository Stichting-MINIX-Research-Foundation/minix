
/* This file implements the disk cache.
 *
 * If they exist anywhere, cached pages are always in a private
 * VM datastructure.
 *
 * They might also be any combination of:
 *    - be mapped in by a filesystem for reading/writing by it
 *    - be mapped in by a process as the result of an mmap call (future)
 *
 * This file manages the datastructure of all cache blocks, and
 * mapping them in and out of filesystems.
 */

#include <assert.h>
#include <string.h>

#include <minix/hash.h>

#include <machine/vmparam.h>

#include "proto.h"
#include "vm.h"
#include "region.h"
#include "glo.h"
#include "cache.h"

static int cache_reference(struct phys_region *pr, struct phys_region *pr2);
static int cache_unreference(struct phys_region *pr);
static int cache_sanitycheck(struct phys_region *pr, const char *file, int line);
static int cache_writable(struct phys_region *pr);
static int cache_resize(struct vmproc *vmp, struct vir_region *vr, vir_bytes l);
static int cache_pagefault(struct vmproc *vmp, struct vir_region *region, 
        struct phys_region *ph, int write, vfs_callback_t cb, void *state,
	int len, int *io);
static int cache_pt_flags(struct vir_region *vr);

struct mem_type mem_type_cache = {
	.name = "cache memory",
	.ev_reference = cache_reference,
	.ev_unreference = cache_unreference,
	.ev_resize = cache_resize,
	.ev_sanitycheck = cache_sanitycheck,
	.ev_pagefault = cache_pagefault,
	.writable = cache_writable,
	.pt_flags = cache_pt_flags,
};

static int cache_pt_flags(struct vir_region *vr){
#if defined(__arm__)
	return ARM_VM_PTE_CACHED;
#else
	return 0;
#endif
}


static int cache_reference(struct phys_region *pr, struct phys_region *pr2)
{
	return OK;
}

static int cache_unreference(struct phys_region *pr)
{
	return mem_type_anon.ev_unreference(pr);
}

static int cache_sanitycheck(struct phys_region *pr, const char *file, int line)
{
	MYASSERT(usedpages_add(pr->ph->phys, VM_PAGE_SIZE) == OK);
	return OK;
}

static int cache_writable(struct phys_region *pr)
{
	/* Cache blocks are at the moment only used by filesystems so always writable. */
	assert(pr->ph->refcount > 0);
	return pr->ph->phys != MAP_NONE;
}

static int cache_resize(struct vmproc *vmp, struct vir_region *vr, vir_bytes l)
{
	printf("VM: cannot resize cache blocks.\n");
	return ENOMEM;
}

int
do_mapcache(message *msg)
{
	dev_t dev = msg->m_vmmcp.dev;
	off_t dev_off = msg->m_vmmcp.dev_offset;
	off_t ino_off = msg->m_vmmcp.ino_offset;
	int n;
	phys_bytes bytes = msg->m_vmmcp.pages * VM_PAGE_SIZE;
	struct vir_region *vr;
	struct vmproc *caller;
	vir_bytes offset;
	int io = 0;

	if(dev_off % PAGE_SIZE || ino_off % PAGE_SIZE) {
		printf("VM: unaligned cache operation\n");
		return EFAULT;
	}

	if(vm_isokendpt(msg->m_source, &n) != OK) panic("bogus source");
	caller = &vmproc[n];

	if(bytes < VM_PAGE_SIZE) return EINVAL;

	if(!(vr = map_page_region(caller, VM_PAGE_SIZE, VM_DATATOP, bytes,
		VR_ANON | VR_WRITABLE, 0, &mem_type_cache))) {
		printf("VM: map_page_region failed\n");
		return ENOMEM;
	}

	assert(vr->length == bytes);

	for(offset = 0; offset < bytes; offset += VM_PAGE_SIZE) {
		struct cached_page *hb;

		assert(vr->length == bytes);
		assert(offset < vr->length);

		if(!(hb = find_cached_page_bydev(dev, dev_off + offset,
			msg->m_vmmcp.ino, ino_off + offset, 1))) {
			map_unmap_region(caller, vr, 0, bytes);
			return ENOENT;
		}

		assert(!vr->param.pb_cache);
		vr->param.pb_cache = hb->page;

		assert(vr->length == bytes);
		assert(offset < vr->length);

		if(map_pf(caller, vr, offset, 1, NULL, NULL, 0, &io) != OK) {
			map_unmap_region(caller, vr, 0, bytes);
			printf("VM: map_pf failed\n");
			return ENOMEM;
		}
		assert(!vr->param.pb_cache);
	}

	memset(msg, 0, sizeof(*msg));

	msg->m_vmmcp_reply.addr = (void *) vr->vaddr;
 
 	assert(vr);

#if CACHE_SANITY
	cache_sanitycheck_internal();
#endif

	return OK;
}

static int cache_pagefault(struct vmproc *vmp, struct vir_region *region, 
        struct phys_region *ph, int write, vfs_callback_t cb,
	void *state, int len, int *io)
{
	vir_bytes offset = ph->offset;
	assert(ph->ph->phys == MAP_NONE);
	assert(region->param.pb_cache);
	pb_unreferenced(region, ph, 0);
	pb_link(ph, region->param.pb_cache, offset, region);
	region->param.pb_cache = NULL;

	return OK;
}

int
do_setcache(message *msg)
{
	int r;
	dev_t dev = msg->m_vmmcp.dev;
	off_t dev_off = msg->m_vmmcp.dev_offset;
	off_t ino_off = msg->m_vmmcp.ino_offset;
	int n;
	struct vmproc *caller;
	phys_bytes offset;
	phys_bytes bytes = msg->m_vmmcp.pages * VM_PAGE_SIZE;

	if(bytes < VM_PAGE_SIZE) return EINVAL;

	if(dev_off % PAGE_SIZE || ino_off % PAGE_SIZE) {
		printf("VM: unaligned cache operation\n");
		return EFAULT;
	}

	if(vm_isokendpt(msg->m_source, &n) != OK) panic("bogus source");
	caller = &vmproc[n];

	for(offset = 0; offset < bytes; offset += VM_PAGE_SIZE) {
		struct vir_region *region;
		struct phys_region *phys_region = NULL;
		vir_bytes v = (vir_bytes) msg->m_vmmcp.block + offset;
                struct cached_page *hb;

		if(!(region = map_lookup(caller, v, &phys_region))) {
			printf("VM: error: no reasonable memory region given (offset 0x%lx, 0x%lx)\n", offset, v);
			return EFAULT;
		}

		if(!phys_region) {
			printf("VM: error: no available memory region given\n");
			return EFAULT;
		}

		if((hb=find_cached_page_bydev(dev, dev_off + offset,
			msg->m_vmmcp.ino, ino_off + offset, 1))) {
			/* block inode info updated */
			if(hb->page != phys_region->ph) {
				/* previous cache entry has become
				 * obsolete; make a new one. rmcache
				 * removes it from the cache and frees
				 * the page if it isn't mapped in anywhere
				 * else.
				 */
                        	rmcache(hb);
			} else {
				/* block was already there, inode info might've changed which is fine */
				continue;
			}
		}

		if(phys_region->memtype != &mem_type_anon &&
			phys_region->memtype != &mem_type_anon_contig) {
			printf("VM: error: no reasonable memory type\n");
			return EFAULT;
		}

		if(phys_region->ph->refcount != 1) {
			printf("VM: error: no reasonable refcount\n");
			return EFAULT;
		}

		phys_region->memtype = &mem_type_cache;

		if((r=addcache(dev, dev_off + offset,
			msg->m_vmmcp.ino, ino_off + offset, phys_region->ph)) != OK) {
			printf("VM: addcache failed\n");
			return r;
		}
	}

#if CACHE_SANITY
	cache_sanitycheck_internal();
#endif

	return OK;
}

/*
 * A file system wants to invalidate all pages belonging to a certain device.
 */
int
do_clearcache(message *msg)
{
	dev_t dev;

	dev = msg->m_vmmcp.dev;

	clear_cache_bydev(dev);

	return OK;
}
