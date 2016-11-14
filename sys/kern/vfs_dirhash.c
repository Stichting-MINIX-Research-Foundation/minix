/* $NetBSD: vfs_dirhash.c,v 1.12 2014/09/05 05:57:21 matt Exp $ */

/*
 * Copyright (c) 2008 Reinoud Zandijk
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 */


#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: vfs_dirhash.c,v 1.12 2014/09/05 05:57:21 matt Exp $");

/* CLEAN UP! */
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/buf.h>
#include <sys/dirent.h>
#include <sys/hash.h>
#include <sys/mutex.h>
#include <sys/pool.h>
#include <sys/queue.h>
#include <sys/vnode.h>
#include <sys/sysctl.h>

#include <sys/dirhash.h>

#if 1
#	define DPRINTF(a) ;
#else
#	define DPRINTF(a) printf a;
#endif

/*
 * The locking protocol of the dirhash structures is fairly simple:
 *
 * The global dirhash_queue is protected by the dirhashmutex. This lock is
 * internal only and is FS/mountpoint/vnode independent. On exit of the
 * exported functions this mutex is not helt.
 *
 * The dirhash structure is considered part of the vnode/inode/udf_node
 * structure and will thus use the lock that protects that vnode/inode.
 *
 * The dirhash entries are considered part of the dirhash structure and thus
 * are on the same lock.
 */

static struct sysctllog *sysctl_log;
static struct pool dirhash_pool;
static struct pool dirhash_entry_pool;

static kmutex_t dirhashmutex;
static uint32_t maxdirhashsize = DIRHASH_SIZE;
static uint32_t dirhashsize    = 0;
static TAILQ_HEAD(_dirhash, dirhash) dirhash_queue;


void
dirhash_init(void)
{
	const struct sysctlnode *rnode, *cnode;
	size_t sz;
	uint32_t max_entries;

	/* initialise dirhash queue */
	TAILQ_INIT(&dirhash_queue);

	/* init dirhash pools */
	sz = sizeof(struct dirhash);
	pool_init(&dirhash_pool, sz, 0, 0, 0,
		"dirhpl", NULL, IPL_NONE);

	sz = sizeof(struct dirhash_entry);
	pool_init(&dirhash_entry_pool, sz, 0, 0, 0,
		"dirhepl", NULL, IPL_NONE);

	mutex_init(&dirhashmutex, MUTEX_DEFAULT, IPL_NONE);
	max_entries = maxdirhashsize / sz;
	pool_sethiwat(&dirhash_entry_pool, max_entries);
	dirhashsize = 0;

	/* create sysctl knobs and dials */
	sysctl_log = NULL;
	sysctl_createv(&sysctl_log, 0, NULL, &rnode,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_NODE, "dirhash", NULL,
		       NULL, 0, NULL, 0,
		       CTL_VFS, VFS_GENERIC, CTL_CREATE, CTL_EOL);
	sysctl_createv(&sysctl_log, 0, &rnode, &cnode,
		       CTLFLAG_PERMANENT,
		       CTLTYPE_INT, "memused",
		       SYSCTL_DESCR("current dirhash memory usage"),
		       NULL, 0, &dirhashsize, 0,
		       CTL_CREATE, CTL_EOL);
	sysctl_createv(&sysctl_log, 0, &rnode, &cnode,
		       CTLFLAG_PERMANENT | CTLFLAG_READWRITE,
		       CTLTYPE_INT, "maxmem",
		       SYSCTL_DESCR("maximum dirhash memory usage"),
		       NULL, 0, &maxdirhashsize, 0,
		       CTL_CREATE, CTL_EOL);
}


#if 0
void
dirhash_finish(void)
{
	pool_destroy(&dirhash_pool);
	pool_destroy(&dirhash_entry_pool);

	mutex_destroy(&dirhashmutex);

	/* sysctl_teardown(&sysctl_log); */
}
#endif


/*
 * generic dirhash implementation
 */

void
dirhash_purge_entries(struct dirhash *dirh)
{
	struct dirhash_entry *dirh_e;
	uint32_t hashline;

	if (dirh == NULL)
		return;

	if (dirh->size == 0)
		return;

	for (hashline = 0; hashline < DIRHASH_HASHSIZE; hashline++) {
		while ((dirh_e =
		    LIST_FIRST(&dirh->entries[hashline])) != NULL) {
			LIST_REMOVE(dirh_e, next);
			pool_put(&dirhash_entry_pool, dirh_e);
		}
	}

	while ((dirh_e = LIST_FIRST(&dirh->free_entries)) != NULL) {
		LIST_REMOVE(dirh_e, next);
		pool_put(&dirhash_entry_pool, dirh_e);
	}

	dirh->flags &= ~DIRH_COMPLETE;
	dirh->flags |=  DIRH_PURGED;
	dirh->num_files = 0;

	dirhashsize -= dirh->size;
	dirh->size = 0;
}


void
dirhash_purge(struct dirhash **dirhp)
{
	struct dirhash *dirh = *dirhp;

	if (dirh == NULL)
		return;

	/* purge its entries */
	dirhash_purge_entries(dirh);

	/* recycle */
	mutex_enter(&dirhashmutex);
	TAILQ_REMOVE(&dirhash_queue, dirh, next);
	mutex_exit(&dirhashmutex);

	pool_put(&dirhash_pool, dirh);
	*dirhp = NULL;
}


void
dirhash_get(struct dirhash **dirhp)
{
	struct dirhash *dirh;
	uint32_t hashline;

	/* if no dirhash was given, allocate one */
	dirh = *dirhp;
	if (dirh == NULL) {
		dirh = pool_get(&dirhash_pool, PR_WAITOK);
		memset(dirh, 0, sizeof(struct dirhash));
		for (hashline = 0; hashline < DIRHASH_HASHSIZE; hashline++) {
			LIST_INIT(&dirh->entries[hashline]);
		}
	}

	/* implement LRU on the dirhash queue */
	mutex_enter(&dirhashmutex);
	if (*dirhp) {
		/* remove from queue to be requeued */
		TAILQ_REMOVE(&dirhash_queue, dirh, next);
	}
	dirh->refcnt++;
	TAILQ_INSERT_HEAD(&dirhash_queue, dirh, next);
	mutex_exit(&dirhashmutex);

	*dirhp = dirh;
}


void
dirhash_put(struct dirhash *dirh)
{

	mutex_enter(&dirhashmutex);
	dirh->refcnt--;
	mutex_exit(&dirhashmutex);
}


void
dirhash_enter(struct dirhash *dirh,
	struct dirent *dirent, uint64_t offset, uint32_t entry_size, int new_p)
{
	struct dirhash *del_dirh, *prev_dirh;
	struct dirhash_entry *dirh_e;
	uint32_t hashvalue, hashline;
	int entrysize;

	/* make sure we have a dirhash to work on */
	KASSERT(dirh);
	KASSERT(dirh->refcnt > 0);

	/* are we trying to re-enter an entry? */
	if (!new_p && (dirh->flags & DIRH_COMPLETE))
		return;

	/* calculate our hash */
	hashvalue = hash32_strn(dirent->d_name, dirent->d_namlen, HASH32_STR_INIT);
	hashline  = hashvalue & DIRHASH_HASHMASK;

	/* lookup and insert entry if not there yet */
	LIST_FOREACH(dirh_e, &dirh->entries[hashline], next) {
		/* check for hash collision */
		if (dirh_e->hashvalue != hashvalue)
			continue;
		if (dirh_e->offset != offset)
			continue;
		/* got it already */
		KASSERT(dirh_e->d_namlen == dirent->d_namlen);
		KASSERT(dirh_e->entry_size == entry_size);
		return;
	}

	DPRINTF(("dirhash enter %"PRIu64", %d, %d for `%*.*s`\n",
		offset, entry_size, dirent->d_namlen,
		dirent->d_namlen, dirent->d_namlen, dirent->d_name));

	/* check if entry is in free space list */
	LIST_FOREACH(dirh_e, &dirh->free_entries, next) {
		if (dirh_e->offset == offset) {
			DPRINTF(("\tremoving free entry\n"));
			LIST_REMOVE(dirh_e, next);
			pool_put(&dirhash_entry_pool, dirh_e);
			break;
		}
	}

	/* ensure we are not passing the dirhash limit */
	entrysize = sizeof(struct dirhash_entry);
	if (dirhashsize + entrysize > maxdirhashsize) {
		/* we walk the dirhash_queue, so need to lock it */
		mutex_enter(&dirhashmutex);
		del_dirh = TAILQ_LAST(&dirhash_queue, _dirhash);
		KASSERT(del_dirh);
		while (dirhashsize + entrysize > maxdirhashsize) {
			/* no use trying to delete myself */
			if (del_dirh == dirh)
				break;
			prev_dirh = TAILQ_PREV(del_dirh, _dirhash, next);
			if (del_dirh->refcnt == 0)
				dirhash_purge_entries(del_dirh);
			del_dirh = prev_dirh;
		}
		mutex_exit(&dirhashmutex);
	}

	/* add to the hashline */
	dirh_e = pool_get(&dirhash_entry_pool, PR_WAITOK);
	memset(dirh_e, 0, sizeof(struct dirhash_entry));

	dirh_e->hashvalue = hashvalue;
	dirh_e->offset    = offset;
	dirh_e->d_namlen  = dirent->d_namlen;
	dirh_e->entry_size  = entry_size;

	dirh->size  += sizeof(struct dirhash_entry);
	dirh->num_files++;
	dirhashsize += sizeof(struct dirhash_entry);
	LIST_INSERT_HEAD(&dirh->entries[hashline], dirh_e, next);
}


void
dirhash_enter_freed(struct dirhash *dirh, uint64_t offset,
	uint32_t entry_size)
{
	struct dirhash_entry *dirh_e;

	/* make sure we have a dirhash to work on */
	KASSERT(dirh);
	KASSERT(dirh->refcnt > 0);

	/* check for double entry of free space */
	LIST_FOREACH(dirh_e, &dirh->free_entries, next) {
		KASSERT(dirh_e->offset != offset);
	}

	DPRINTF(("dirhash enter FREED %"PRIu64", %d\n",
		offset, entry_size));
	dirh_e = pool_get(&dirhash_entry_pool, PR_WAITOK);
	memset(dirh_e, 0, sizeof(struct dirhash_entry));

	dirh_e->hashvalue = 0;		/* not relevant */
	dirh_e->offset    = offset;
	dirh_e->d_namlen  = 0;		/* not relevant */
	dirh_e->entry_size  = entry_size;

	/* XXX it might be preferable to append them at the tail */
	LIST_INSERT_HEAD(&dirh->free_entries, dirh_e, next);
	dirh->size  += sizeof(struct dirhash_entry);
	dirhashsize += sizeof(struct dirhash_entry);
}


void
dirhash_remove(struct dirhash *dirh, struct dirent *dirent,
	uint64_t offset, uint32_t entry_size)
{
	struct dirhash_entry *dirh_e;
	uint32_t hashvalue, hashline;

	DPRINTF(("dirhash remove %"PRIu64", %d for `%*.*s`\n",
		offset, entry_size, 
		dirent->d_namlen, dirent->d_namlen, dirent->d_name));

	/* make sure we have a dirhash to work on */
	KASSERT(dirh);
	KASSERT(dirh->refcnt > 0);

	/* calculate our hash */
	hashvalue = hash32_strn(dirent->d_name, dirent->d_namlen, HASH32_STR_INIT);
	hashline  = hashvalue & DIRHASH_HASHMASK;

	/* lookup entry */
	LIST_FOREACH(dirh_e, &dirh->entries[hashline], next) {
		/* check for hash collision */
		if (dirh_e->hashvalue != hashvalue)
			continue;
		if (dirh_e->offset != offset)
			continue;

		/* got it! */
		KASSERT(dirh_e->d_namlen == dirent->d_namlen);
		KASSERT(dirh_e->entry_size == entry_size);
		LIST_REMOVE(dirh_e, next);
		dirh->size -= sizeof(struct dirhash_entry);
		KASSERT(dirh->num_files > 0);
		dirh->num_files--;
		dirhashsize -= sizeof(struct dirhash_entry);

		dirhash_enter_freed(dirh, offset, entry_size);
		return;
	}

	/* not found! */
	panic("dirhash_remove couldn't find entry in hash table\n");
}


/*
 * BUGALERT: don't use result longer than needed, never past the node lock.
 * Call with NULL *result initially and it will return nonzero if again.
 */
int
dirhash_lookup(struct dirhash *dirh, const char *d_name, int d_namlen,
	struct dirhash_entry **result)
{
	struct dirhash_entry *dirh_e;
	uint32_t hashvalue, hashline;

	/* make sure we have a dirhash to work on */
	KASSERT(dirh);
	KASSERT(dirh->refcnt > 0);

	/* start where we were */
	if (*result) {
		dirh_e = *result;

		/* retrieve information to avoid recalculation and advance */
		hashvalue = dirh_e->hashvalue;
		dirh_e = LIST_NEXT(*result, next);
	} else {
		/* calculate our hash and lookup all entries in hashline */
		hashvalue = hash32_strn(d_name, d_namlen, HASH32_STR_INIT);
		hashline  = hashvalue & DIRHASH_HASHMASK;
		dirh_e = LIST_FIRST(&dirh->entries[hashline]);
	}

	for (; dirh_e; dirh_e = LIST_NEXT(dirh_e, next)) {
		/* check for hash collision */
		if (dirh_e->hashvalue != hashvalue)
			continue;
		if (dirh_e->d_namlen != d_namlen)
			continue;
		/* might have an entry in the cache */
		*result = dirh_e;
		return 1;
	}

	*result = NULL;
	return 0;
}


/*
 * BUGALERT: don't use result longer than needed, never past the node lock.
 * Call with NULL *result initially and it will return nonzero if again.
 */

int
dirhash_lookup_freed(struct dirhash *dirh, uint32_t min_entrysize,
	struct dirhash_entry **result)
{
	struct dirhash_entry *dirh_e;

	/* make sure we have a dirhash to work on */
	KASSERT(dirh);
	KASSERT(dirh->refcnt > 0);

	/* start where we were */
	if (*result) {
		dirh_e = LIST_NEXT(*result, next);
	} else {
		/* lookup all entries that match */
		dirh_e = LIST_FIRST(&dirh->free_entries);
	}

	for (; dirh_e; dirh_e = LIST_NEXT(dirh_e, next)) {
		/* check for minimum size */
		if (dirh_e->entry_size < min_entrysize)
			continue;
		/* might be a candidate */
		*result = dirh_e;
		return 1;
	}

	*result = NULL;
	return 0;
}


bool
dirhash_dir_isempty(struct dirhash *dirh)
{
#ifdef DEBUG
	struct dirhash_entry *dirh_e;
	int hashline, num;

	num = 0;
	for (hashline = 0; hashline < DIRHASH_HASHSIZE; hashline++) {
		LIST_FOREACH(dirh_e, &dirh->entries[hashline], next) {
			num++;
		}
	}

	if (dirh->num_files != num) {
		printf("dirhash_dir_isempy: dirhash_counter failed: "
			"dirh->num_files = %d, counted %d\n",
			dirh->num_files, num);
		assert(dirh->num_files == num);
	}
#endif
	/* assert the directory hash info is valid */
	KASSERT(dirh->flags & DIRH_COMPLETE);

	/* the directory is empty when only '..' lifes in it or is absent */
	return (dirh->num_files <= 1);
}

