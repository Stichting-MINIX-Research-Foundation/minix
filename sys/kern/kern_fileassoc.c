/* $NetBSD: kern_fileassoc.c,v 1.36 2014/07/10 15:00:28 christos Exp $ */

/*-
 * Copyright (c) 2006 Elad Efrat <elad@NetBSD.org>
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: kern_fileassoc.c,v 1.36 2014/07/10 15:00:28 christos Exp $");

#include "opt_fileassoc.h"

#include <sys/param.h>
#include <sys/mount.h>
#include <sys/queue.h>
#include <sys/vnode.h>
#include <sys/errno.h>
#include <sys/fileassoc.h>
#include <sys/specificdata.h>
#include <sys/hash.h>
#include <sys/kmem.h>
#include <sys/once.h>

#define	FILEASSOC_INITIAL_TABLESIZE	128

static specificdata_domain_t fileassoc_domain = NULL;
static specificdata_key_t fileassoc_mountspecific_key;
static ONCE_DECL(control);

/*
 * Assoc entry.
 * Includes the assoc name for identification and private clear callback.
 */
struct fileassoc {
	LIST_ENTRY(fileassoc) assoc_list;
	const char *assoc_name;				/* Name. */
	fileassoc_cleanup_cb_t assoc_cleanup_cb;	/* Clear callback. */
	specificdata_key_t assoc_key;
};

static LIST_HEAD(, fileassoc) fileassoc_list;

/* An entry in the per-mount hash table. */
struct fileassoc_file {
	fhandle_t *faf_handle;				/* File handle */
	specificdata_reference faf_data;		/* Assoc data. */
	u_int faf_nassocs;				/* # of assocs. */
	LIST_ENTRY(fileassoc_file) faf_list;		/* List pointer. */
};

LIST_HEAD(fileassoc_hash_entry, fileassoc_file);

struct fileassoc_table {
	struct fileassoc_hash_entry *tbl_hash;
	u_long tbl_mask;				/* Hash table mask. */
	size_t tbl_nslots;				/* Number of slots. */
	size_t tbl_nused;				/* # of used slots. */
	specificdata_reference tbl_data;
};

/*
 * Hashing function: Takes a number modulus the mask to give back an
 * index into the hash table.
 */
#define FILEASSOC_HASH(tbl, handle)	\
	(hash32_buf((handle), FHANDLE_SIZE(handle), HASH32_BUF_INIT) \
	 & ((tbl)->tbl_mask))

static void *
file_getdata(struct fileassoc_file *faf, const struct fileassoc *assoc)
{

	return specificdata_getspecific(fileassoc_domain, &faf->faf_data,
	    assoc->assoc_key);
}

static void
file_setdata(struct fileassoc_file *faf, const struct fileassoc *assoc,
    void *data)
{

	specificdata_setspecific(fileassoc_domain, &faf->faf_data,
	    assoc->assoc_key, data);
}

static void
file_cleanup(struct fileassoc_file *faf, const struct fileassoc *assoc)
{
	fileassoc_cleanup_cb_t cb;
	void *data;

	cb = assoc->assoc_cleanup_cb;
	if (cb == NULL) {
		return;
	}
	data = file_getdata(faf, assoc);
	(*cb)(data);
}

static void
file_free(struct fileassoc_file *faf)
{
	struct fileassoc *assoc;

	LIST_REMOVE(faf, faf_list);

	LIST_FOREACH(assoc, &fileassoc_list, assoc_list) {
		file_cleanup(faf, assoc);
	}
	vfs_composefh_free(faf->faf_handle);
	specificdata_fini(fileassoc_domain, &faf->faf_data);
	kmem_free(faf, sizeof(*faf));
}

static void
table_dtor(void *v)
{
	struct fileassoc_table *tbl = v;
	u_long i;

	/* Remove all entries from the table and lists */
	for (i = 0; i < tbl->tbl_nslots; i++) {
		struct fileassoc_file *faf;

		while ((faf = LIST_FIRST(&tbl->tbl_hash[i])) != NULL) {
			file_free(faf);
		}
	}

	/* Remove hash table and sysctl node */
	hashdone(tbl->tbl_hash, HASH_LIST, tbl->tbl_mask);
	specificdata_fini(fileassoc_domain, &tbl->tbl_data);
	kmem_free(tbl, sizeof(*tbl));
}

/*
 * Initialize the fileassoc subsystem.
 */
static int
fileassoc_init(void)
{
	int error;

	error = mount_specific_key_create(&fileassoc_mountspecific_key,
	    table_dtor);
	if (error) {
		return error;
	}
	fileassoc_domain = specificdata_domain_create();

	return 0;
}

/*
 * Register a new assoc.
 */
int
fileassoc_register(const char *name, fileassoc_cleanup_cb_t cleanup_cb,
    fileassoc_t *result)
{
	int error;
	specificdata_key_t key;
	struct fileassoc *assoc;

	error = RUN_ONCE(&control, fileassoc_init);
	if (error) {
		return error;
	}
	error = specificdata_key_create(fileassoc_domain, &key, NULL);
	if (error) {
		return error;
	}
	assoc = kmem_alloc(sizeof(*assoc), KM_SLEEP);
	assoc->assoc_name = name;
	assoc->assoc_cleanup_cb = cleanup_cb;
	assoc->assoc_key = key;

	LIST_INSERT_HEAD(&fileassoc_list, assoc, assoc_list);

	*result = assoc;

	return 0;
}

/*
 * Deregister an assoc.
 */
int
fileassoc_deregister(fileassoc_t assoc)
{

	LIST_REMOVE(assoc, assoc_list);
	specificdata_key_delete(fileassoc_domain, assoc->assoc_key);
	kmem_free(assoc, sizeof(*assoc));

	return 0;
}

/*
 * Get the hash table for the specified device.
 */
static struct fileassoc_table *
fileassoc_table_lookup(struct mount *mp)
{
	int error;

	error = RUN_ONCE(&control, fileassoc_init);
	if (error) {
		return NULL;
	}
	return mount_getspecific(mp, fileassoc_mountspecific_key);
}

/*
 * Perform a lookup on a hash table.  If hint is non-zero then use the value
 * of the hint as the identifier instead of performing a lookup for the
 * fileid.
 */
static struct fileassoc_file *
fileassoc_file_lookup(struct vnode *vp, fhandle_t *hint)
{
	struct fileassoc_table *tbl;
	struct fileassoc_hash_entry *hash_entry;
	struct fileassoc_file *faf;
	size_t indx;
	fhandle_t *th;
	int error;

	tbl = fileassoc_table_lookup(vp->v_mount);
	if (tbl == NULL) {
		return NULL;
	}

	if (hint == NULL) {
		error = vfs_composefh_alloc(vp, &th);
		if (error)
			return (NULL);
	} else {
		th = hint;
	}

	indx = FILEASSOC_HASH(tbl, th);
	hash_entry = &(tbl->tbl_hash[indx]);

	LIST_FOREACH(faf, hash_entry, faf_list) {
		if (((FHANDLE_FILEID(faf->faf_handle)->fid_len ==
		     FHANDLE_FILEID(th)->fid_len)) &&
		    (memcmp(FHANDLE_FILEID(faf->faf_handle), FHANDLE_FILEID(th),
			   (FHANDLE_FILEID(th))->fid_len) == 0)) {
			break;
		}
	}

	if (hint == NULL)
		vfs_composefh_free(th);

	return faf;
}

/*
 * Return assoc data associated with a vnode.
 */
void *
fileassoc_lookup(struct vnode *vp, fileassoc_t assoc)
{
	struct fileassoc_file *faf;

	faf = fileassoc_file_lookup(vp, NULL);
	if (faf == NULL)
		return (NULL);

	return file_getdata(faf, assoc);
}

static struct fileassoc_table *
fileassoc_table_resize(struct fileassoc_table *tbl)
{
	struct fileassoc_table *newtbl;
	u_long i;

	/*
	 * Allocate a new table. Like the condition in fileassoc_file_add(),
	 * this is also temporary -- just double the number of slots.
	 */
	newtbl = kmem_zalloc(sizeof(*newtbl), KM_SLEEP);
	newtbl->tbl_nslots = (tbl->tbl_nslots * 2);
	if (newtbl->tbl_nslots < tbl->tbl_nslots)
		newtbl->tbl_nslots = tbl->tbl_nslots;
	newtbl->tbl_hash = hashinit(newtbl->tbl_nslots, HASH_LIST,
	    true, &newtbl->tbl_mask);
	newtbl->tbl_nused = 0;
	specificdata_init(fileassoc_domain, &newtbl->tbl_data);

	/* XXX we need to make sure nothing uses fileassoc here! */

	for (i = 0; i < tbl->tbl_nslots; i++) {
		struct fileassoc_file *faf;

		while ((faf = LIST_FIRST(&tbl->tbl_hash[i])) != NULL) {
			struct fileassoc_hash_entry *hash_entry;
			size_t indx;

			LIST_REMOVE(faf, faf_list);

			indx = FILEASSOC_HASH(newtbl, faf->faf_handle);
			hash_entry = &(newtbl->tbl_hash[indx]);

			LIST_INSERT_HEAD(hash_entry, faf, faf_list);

			newtbl->tbl_nused++;
		}
	}

	if (tbl->tbl_nused != newtbl->tbl_nused)
		panic("fileassoc_table_resize: inconsistency detected! "
		    "needed %zu entries, got %zu", tbl->tbl_nused,
		    newtbl->tbl_nused);

	hashdone(tbl->tbl_hash, HASH_LIST, tbl->tbl_mask);
	specificdata_fini(fileassoc_domain, &tbl->tbl_data);
	kmem_free(tbl, sizeof(*tbl));

	return (newtbl);
}

/*
 * Create a new fileassoc table.
 */
static struct fileassoc_table *
fileassoc_table_add(struct mount *mp)
{
	struct fileassoc_table *tbl;

	/* Check for existing table for device. */
	tbl = fileassoc_table_lookup(mp);
	if (tbl != NULL)
		return (tbl);

	/* Allocate and initialize a table. */
	tbl = kmem_zalloc(sizeof(*tbl), KM_SLEEP);
	tbl->tbl_nslots = FILEASSOC_INITIAL_TABLESIZE;
	tbl->tbl_hash = hashinit(tbl->tbl_nslots, HASH_LIST, true,
	    &tbl->tbl_mask);
	tbl->tbl_nused = 0;
	specificdata_init(fileassoc_domain, &tbl->tbl_data);

	mount_setspecific(mp, fileassoc_mountspecific_key, tbl);

	return (tbl);
}

/*
 * Delete a table.
 */
int
fileassoc_table_delete(struct mount *mp)
{
	struct fileassoc_table *tbl;

	tbl = fileassoc_table_lookup(mp);
	if (tbl == NULL)
		return (EEXIST);

	mount_setspecific(mp, fileassoc_mountspecific_key, NULL);
	table_dtor(tbl);

	return (0);
}

/*
 * Run a callback for each assoc in a table.
 */
int
fileassoc_table_run(struct mount *mp, fileassoc_t assoc, fileassoc_cb_t cb,
    void *cookie)
{
	struct fileassoc_table *tbl;
	u_long i;

	tbl = fileassoc_table_lookup(mp);
	if (tbl == NULL)
		return (EEXIST);

	for (i = 0; i < tbl->tbl_nslots; i++) {
		struct fileassoc_file *faf;

		LIST_FOREACH(faf, &tbl->tbl_hash[i], faf_list) {
			void *data;

			data = file_getdata(faf, assoc);
			if (data != NULL)
				cb(data, cookie);
		}
	}

	return (0);
}

/*
 * Clear a table for a given assoc.
 */
int
fileassoc_table_clear(struct mount *mp, fileassoc_t assoc)
{
	struct fileassoc_table *tbl;
	u_long i;

	tbl = fileassoc_table_lookup(mp);
	if (tbl == NULL)
		return (EEXIST);

	for (i = 0; i < tbl->tbl_nslots; i++) {
		struct fileassoc_file *faf;

		LIST_FOREACH(faf, &tbl->tbl_hash[i], faf_list) {
			file_cleanup(faf, assoc);
			file_setdata(faf, assoc, NULL);
		}
	}

	return (0);
}

/*
 * Add a file entry to a table.
 */
static struct fileassoc_file *
fileassoc_file_add(struct vnode *vp, fhandle_t *hint)
{
	struct fileassoc_table *tbl;
	struct fileassoc_hash_entry *hash_entry;
	struct fileassoc_file *faf;
	size_t indx;
	fhandle_t *th;
	int error;

	if (hint == NULL) {
		error = vfs_composefh_alloc(vp, &th);
		if (error)
			return (NULL);
	} else
		th = hint;

	faf = fileassoc_file_lookup(vp, th);
	if (faf != NULL) {
		if (hint == NULL)
			vfs_composefh_free(th);

		return (faf);
	}

	tbl = fileassoc_table_lookup(vp->v_mount);
	if (tbl == NULL) {
		tbl = fileassoc_table_add(vp->v_mount);
	}

	indx = FILEASSOC_HASH(tbl, th);
	hash_entry = &(tbl->tbl_hash[indx]);

	faf = kmem_zalloc(sizeof(*faf), KM_SLEEP);
	faf->faf_handle = th;
	specificdata_init(fileassoc_domain, &faf->faf_data);
	LIST_INSERT_HEAD(hash_entry, faf, faf_list);

	/*
	 * This decides when we need to resize the table. For now,
	 * resize it whenever we "filled" up the number of slots it
	 * has. That's not really true unless of course we had zero
	 * collisions. Think positive! :)
	 */
	if (++(tbl->tbl_nused) == tbl->tbl_nslots) { 
		struct fileassoc_table *newtbl;

		newtbl = fileassoc_table_resize(tbl);
		mount_setspecific(vp->v_mount, fileassoc_mountspecific_key,
		    newtbl);
	}

	return (faf);
}

/*
 * Delete a file entry from a table.
 */
int
fileassoc_file_delete(struct vnode *vp)
{
	struct fileassoc_table *tbl;
	struct fileassoc_file *faf;

	/* Pre-check if fileassoc is used. XXX */
	if (!fileassoc_domain) {
		return ENOENT;
	}
	KERNEL_LOCK(1, NULL);

	faf = fileassoc_file_lookup(vp, NULL);
	if (faf == NULL) {
		KERNEL_UNLOCK_ONE(NULL);
		return (ENOENT);
	}

	file_free(faf);

	tbl = fileassoc_table_lookup(vp->v_mount);
	KASSERT(tbl != NULL);
	--(tbl->tbl_nused); /* XXX gc? */

	KERNEL_UNLOCK_ONE(NULL);

	return (0);
}

/*
 * Add an assoc to a vnode.
 */
int
fileassoc_add(struct vnode *vp, fileassoc_t assoc, void *data)
{
	struct fileassoc_file *faf;
	void *olddata;

	faf = fileassoc_file_lookup(vp, NULL);
	if (faf == NULL) {
		faf = fileassoc_file_add(vp, NULL);
		if (faf == NULL)
			return (ENOTDIR);
	}

	olddata = file_getdata(faf, assoc);
	if (olddata != NULL)
		return (EEXIST);

	file_setdata(faf, assoc, data);

	faf->faf_nassocs++;

	return (0);
}

/*
 * Clear an assoc from a vnode.
 */
int
fileassoc_clear(struct vnode *vp, fileassoc_t assoc)
{
	struct fileassoc_file *faf;

	faf = fileassoc_file_lookup(vp, NULL);
	if (faf == NULL)
		return (ENOENT);

	file_cleanup(faf, assoc);
	file_setdata(faf, assoc, NULL);

	--(faf->faf_nassocs); /* XXX gc? */

	return (0);
}
