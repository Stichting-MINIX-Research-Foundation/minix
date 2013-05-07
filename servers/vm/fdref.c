
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

struct fdref *fdref_new(struct vmproc *owner, ino_t ino, dev_t dev, int fd)
{
	struct fdref *fdref;

	if(!SLABALLOC(fdref)) return NULL;

	fdref->fd = fd;
	fdref->owner = owner;
	fdref->refcount = 0;
	fdref->dev = dev;
	fdref->ino = ino;
	fdref->next = owner->fdrefs;
	owner->fdrefs = fdref;

	return fdref;
}

void fdref_ref(struct fdref *ref, struct vir_region *region)
{
	assert(ref);
	assert(ref->owner == region->parent);
	region->param.file.fdref = ref;
	ref->refcount++;
}

void fdref_deref(struct vir_region *region)
{
	struct fdref *ref = region->param.file.fdref;
	int fd;

	assert(ref);
	assert(ref->refcount > 0);
	assert(ref->owner == region->parent);

	fd = ref->fd;
	region->param.file.fdref = NULL;
	ref->refcount--;
	assert(ref->refcount >= 0);
	if(ref->refcount > 0) return;

	if(region->parent->fdrefs == ref) region->parent->fdrefs = ref->next;
	else {
		struct fdref *r;
		for(r = region->parent->fdrefs; r->next != ref; r = r->next)
			;
		assert(r);
		assert(r->next == ref);
		r->next = ref->next;
	}

	SLABFREE(ref);
	ref = NULL;

	if(region->parent->vm_flags & (VMF_EXITING|VMF_EXECING)) {
		return;
	}

	/* If the last reference has disappeared, free the
	 * ref object and asynchronously close the fd in VFS.
	 *
	 * We don't need a callback as a close failing, although
	 * unexpected, isn't a problem and can't be handled. VFS
	 * will print a diagnostic.
	 */
#if 1
	if(vfs_request(VMVFSREQ_FDCLOSE, fd, region->parent,
		0, 0, NULL, NULL, NULL, 0) != OK) {
		panic("fdref_deref: could not send close request");
	}
#endif
}

struct fdref *fdref_dedup_or_new(struct vmproc *owner,
	ino_t ino, dev_t dev, int fd)
{
	struct fdref *fr;

	for(fr = owner->fdrefs; fr; fr = fr->next) {
		assert(fr->owner == owner);
		if(fr->fd == fd) {
			assert(ino == fr->ino);
			assert(dev == fr->dev);
			return fr;
		}
		if(ino == fr->ino && dev == fr->dev) {
			assert(fd != fr->fd);
#if 1
			if(vfs_request(VMVFSREQ_FDCLOSE, fd, owner,
				0, 0, NULL, NULL, NULL, 0) != OK) {
				printf("fdref_dedup_or_new: could not close\n");
			}
#endif
			return fr;
		}
	}

	return fdref_new(owner, ino, dev, fd);
}

