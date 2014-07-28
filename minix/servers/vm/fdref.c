
/* File that implements the 'fdref' data structure. It keeps track
 * of how many times a particular fd (per process) is referenced by
 * mmapped objects.
 *
 * This is used to
 *  - have many references to the same file, without needing an FD each
 *  - deciding when we have to close an FD (last reference disappears)
 *
 * Examples:
 *  - if a file-mmapped region is split, the refcount increases; there are
 *    now two regions referencing the same FD. We can't simply close the
 *    FD once either region is unmapped, as the pagefaults for the other
 *    would stop working. So we increase the refcount to that fd.
 *  - if a new file-maped region is requested, we might find out it's the
 *    same dev/inode the same process already has referenced. we could
 *    decide to close the new reference and use an existing one, so 
 *    references to the same file aren't fd-limited.
 *  - if a file-mapped region is copied, we have to create a new
 *    fdref object, as the source process might disappear; we have to
 *    use the new process' fd for it.
 */

#include <assert.h>
#include <string.h>

#include <minix/hash.h>

#include "proto.h"
#include "vm.h"
#include "fdref.h"
#include "vmproc.h"
#include "glo.h"

static struct fdref *fdrefs;

void fdref_sanitycheck(void)
{
	struct vmproc *vmp;
	region_iter v_iter;
	struct fdref *fr;
	static int prevopen = 0;
	int openfd = 0;

	for(fr = fdrefs; fr; fr = fr->next) {
		struct fdref *fr2;
		for(fr2 = fdrefs; fr2; fr2 = fr2->next) {
			if(fr == fr2) continue;
			if(fr->fd == fr2->fd) {
				printf("equal fd omg\n");
				util_stacktrace();
			}
			if(fr->ino == fr2->ino && fr->dev == fr2->dev) {
				printf("equal metadata omg\n");
				util_stacktrace();
			}
		}
		openfd++;
	}

	for(fr = fdrefs; fr; fr = fr->next) {
		fr->counting = 0;
	}

	for(vmp = vmproc; vmp < &vmproc[VMP_NR]; vmp++) {
		struct vir_region *vr;
                if(!(vmp->vm_flags & VMF_INUSE))
			continue;
		region_start_iter_least(&vmp->vm_regions_avl, &v_iter);
		while((vr = region_get_iter(&v_iter))) {
			if(vr->def_memtype == &mem_type_mappedfile && vr->param.file.inited) {
				vr->param.file.fdref->counting++;
			}
			region_incr_iter(&v_iter);
		}

	}

	for(fr = fdrefs; fr; fr = fr->next) {
		if(fr->counting != fr->refcount) {
			printf("counting %d != refcount %d\n",
				fr->counting, fr->refcount);
			util_stacktrace();
		}
	}

	if(prevopen != openfd && openfd > 100) {
		printf("%d open\n", openfd);
		prevopen = openfd;
	}
}

struct fdref *fdref_new(struct vmproc *owner, ino_t ino, dev_t dev, int fd)
{
	struct fdref *nfdref;

	if(!SLABALLOC(nfdref)) return NULL;

	nfdref->fd = fd;
	nfdref->refcount = 0;
	nfdref->dev = dev;
	nfdref->ino = ino;
	nfdref->next = fdrefs;
	fdrefs = nfdref;

	return nfdref;
}

void fdref_ref(struct fdref *ref, struct vir_region *region)
{
	assert(ref);
	region->param.file.fdref = ref;
	ref->refcount++;
}

void fdref_deref(struct vir_region *region)
{
	struct fdref *ref = region->param.file.fdref;
	int fd;

	assert(ref);
	assert(ref->refcount > 0);

	fd = ref->fd;
	region->param.file.fdref = NULL;
	ref->refcount--;
	assert(ref->refcount >= 0);
	if(ref->refcount > 0) return;

	if(fdrefs == ref) fdrefs = ref->next;
	else {
		struct fdref *r;
		for(r = fdrefs; r->next != ref; r = r->next)
			;
		assert(r);
		assert(r->next == ref);
		r->next = ref->next;
	}

	SLABFREE(ref);
	ref = NULL;
	
	/* If the last reference has disappeared, free the
	 * ref object and asynchronously close the fd in VFS.
	 *
	 * We don't need a callback as a close failing, although
	 * unexpected, isn't a problem and can't be handled. VFS
	 * will print a diagnostic.
	 */
	if(vfs_request(VMVFSREQ_FDCLOSE, fd, region->parent,
		0, 0, NULL, NULL, NULL, 0) != OK) {
		panic("fdref_deref: could not send close request");
	}
}

struct fdref *fdref_dedup_or_new(struct vmproc *owner,
	ino_t ino, dev_t dev, int fd, int mayclose)
{
	struct fdref *fr;

	for(fr = fdrefs; fr; fr = fr->next) {
		if(ino == fr->ino && dev == fr->dev) {
			if(fd == fr->fd) {
				return fr;
			}
			if(!mayclose) continue;
			if(vfs_request(VMVFSREQ_FDCLOSE, fd, owner,
				0, 0, NULL, NULL, NULL, 0) != OK) {
				printf("fdref_dedup_or_new: could not close\n");
			}
			return fr;
		}
	}

	return fdref_new(owner, ino, dev, fd);
}

