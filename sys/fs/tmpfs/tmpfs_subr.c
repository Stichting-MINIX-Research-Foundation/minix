/*	$NetBSD: tmpfs_subr.c,v 1.100 2015/07/07 09:30:24 justin Exp $	*/

/*
 * Copyright (c) 2005-2013 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Julio M. Merino Vidal, developed as part of Google's Summer of Code
 * 2005 program, and by Mindaugas Rasiukevicius.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Efficient memory file system: interfaces for inode and directory entry
 * construction, destruction and manipulation.
 *
 * Reference counting
 *
 *	The link count of inode (tmpfs_node_t::tn_links) is used as a
 *	reference counter.  However, it has slightly different semantics.
 *
 *	For directories - link count represents directory entries, which
 *	refer to the directories.  In other words, it represents the count
 *	of sub-directories.  It also takes into account the virtual '.'
 *	entry (which has no real entry in the list).  For files - link count
 *	represents the hard links.  Since only empty directories can be
 *	removed - link count aligns the reference counting requirements
 *	enough.  Note: to check whether directory is not empty, the inode
 *	size (tmpfs_node_t::tn_size) can be used.
 *
 *	The inode itself, as an object, gathers its first reference when
 *	directory entry is attached via tmpfs_dir_attach(9).  For instance,
 *	after regular tmpfs_create(), a file would have a link count of 1,
 *	while directory after tmpfs_mkdir() would have 2 (due to '.').
 *
 * Reclamation
 *
 *	It should be noted that tmpfs inodes rely on a combination of vnode
 *	reference counting and link counting.  That is, an inode can only be
 *	destroyed if its associated vnode is inactive.  The destruction is
 *	done on vnode reclamation i.e. tmpfs_reclaim().  It should be noted
 *	that tmpfs_node_t::tn_links being 0 is a destruction criterion.
 *
 *	If an inode has references within the file system (tn_links > 0) and
 *	its inactive vnode gets reclaimed/recycled - then the association is
 *	broken in tmpfs_reclaim().  In such case, an inode will always pass
 *	tmpfs_lookup() and thus vcache_get() to associate a new vnode.
 *
 * Lock order
 *
 *	vnode_t::v_vlock ->
 *		vnode_t::v_interlock
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: tmpfs_subr.c,v 1.100 2015/07/07 09:30:24 justin Exp $");

#include <sys/param.h>
#include <sys/cprng.h>
#include <sys/dirent.h>
#include <sys/event.h>
#include <sys/kmem.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/systm.h>
#include <sys/vnode.h>
#include <sys/kauth.h>
#include <sys/atomic.h>

#include <uvm/uvm.h>

#include <miscfs/specfs/specdev.h>
#include <miscfs/genfs/genfs.h>
#include <fs/tmpfs/tmpfs.h>
#include <fs/tmpfs/tmpfs_fifoops.h>
#include <fs/tmpfs/tmpfs_specops.h>
#include <fs/tmpfs/tmpfs_vnops.h>

static void	tmpfs_dir_putseq(tmpfs_node_t *, tmpfs_dirent_t *);

/*
 * Initialize vnode with tmpfs node.
 */
static void
tmpfs_init_vnode(struct vnode *vp, tmpfs_node_t *node)
{
	kmutex_t *slock;

	KASSERT(node->tn_vnode == NULL);

	/* Share the interlock with the node. */
	if (node->tn_type == VREG) {
		slock = node->tn_spec.tn_reg.tn_aobj->vmobjlock;
		mutex_obj_hold(slock);
		uvm_obj_setlock(&vp->v_uobj, slock);
	}

	vp->v_tag = VT_TMPFS;
	vp->v_type = node->tn_type;

	/* Type-specific initialization. */
	switch (vp->v_type) {
	case VBLK:
	case VCHR:
		vp->v_op = tmpfs_specop_p;
		spec_node_init(vp, node->tn_spec.tn_dev.tn_rdev);
		break;
	case VFIFO:
		vp->v_op = tmpfs_fifoop_p;
		break;
	case VDIR:
		if (node->tn_spec.tn_dir.tn_parent == node)
			vp->v_vflag |= VV_ROOT;
		/* FALLTHROUGH */
	case VLNK:
	case VREG:
	case VSOCK:
		vp->v_op = tmpfs_vnodeop_p;
		break;
	default:
		panic("bad node type %d", vp->v_type);
		break;
	}

	vp->v_data = node;
	node->tn_vnode = vp;
	uvm_vnp_setsize(vp, node->tn_size);
}

/*
 * tmpfs_loadvnode: initialise a vnode for a specified inode.
 */
int
tmpfs_loadvnode(struct mount *mp, struct vnode *vp,
    const void *key, size_t key_len, const void **new_key)
{
	tmpfs_node_t *node;

	KASSERT(key_len == sizeof(node));
	memcpy(&node, key, key_len);

	if (node->tn_links == 0)
		return ENOENT;

	tmpfs_init_vnode(vp, node);

	*new_key = &vp->v_data;

	return 0;
}

/*
 * tmpfs_newvnode: allocate a new inode of a specified type and
 * attach the vonode.
 */
int
tmpfs_newvnode(struct mount *mp, struct vnode *dvp, struct vnode *vp,
    struct vattr *vap, kauth_cred_t cred,
    size_t *key_len, const void **new_key)
{
	tmpfs_mount_t *tmp = VFS_TO_TMPFS(mp);
	tmpfs_node_t *node, *dnode;

	if (dvp != NULL) {
		KASSERT(VOP_ISLOCKED(dvp));
		dnode = VP_TO_TMPFS_DIR(dvp);
		if (dnode->tn_links == 0)
			return ENOENT;
		if (vap->va_type == VDIR) {
			/* Check for maximum links limit. */
			if (dnode->tn_links == LINK_MAX)
				return EMLINK;
			KASSERT(dnode->tn_links < LINK_MAX);
		}
	} else
		dnode = NULL;

	node = tmpfs_node_get(tmp);
	if (node == NULL)
		return ENOSPC;

	/* Initially, no references and no associations. */
	node->tn_links = 0;
	node->tn_vnode = NULL;
	node->tn_holdcount = 0;
	node->tn_dirent_hint = NULL;

	/*
	 * XXX Where the pool is backed by a map larger than (4GB *
	 * sizeof(*node)), this may produce duplicate inode numbers
	 * for applications that do not understand 64-bit ino_t.
	 */
	node->tn_id = (ino_t)((uintptr_t)node / sizeof(*node));
	/*
	 * Make sure the generation number is not zero.
	 * tmpfs_inactive() uses generation zero to mark dead nodes.
	 */
	do {
		node->tn_gen = TMPFS_NODE_GEN_MASK & cprng_fast32();
	} while (node->tn_gen == 0);

	/* Generic initialization. */
	KASSERT((int)vap->va_type != VNOVAL);
	node->tn_type = vap->va_type;
	node->tn_size = 0;
	node->tn_flags = 0;
	node->tn_lockf = NULL;

	vfs_timestamp(&node->tn_atime);
	node->tn_birthtime = node->tn_atime;
	node->tn_ctime = node->tn_atime;
	node->tn_mtime = node->tn_atime;

	if (dvp == NULL) {
		KASSERT(vap->va_uid != VNOVAL && vap->va_gid != VNOVAL);
		node->tn_uid = vap->va_uid;
		node->tn_gid = vap->va_gid;
		vp->v_vflag |= VV_ROOT;
	} else {
		KASSERT(dnode != NULL);
		node->tn_uid = kauth_cred_geteuid(cred);
		node->tn_gid = dnode->tn_gid;
	}
	KASSERT(vap->va_mode != VNOVAL);
	node->tn_mode = vap->va_mode;

	/* Type-specific initialization. */
	switch (node->tn_type) {
	case VBLK:
	case VCHR:
		/* Character/block special device. */
		KASSERT(vap->va_rdev != VNOVAL);
		node->tn_spec.tn_dev.tn_rdev = vap->va_rdev;
		break;
	case VDIR:
		/* Directory. */
		TAILQ_INIT(&node->tn_spec.tn_dir.tn_dir);
		node->tn_spec.tn_dir.tn_parent = NULL;
		node->tn_spec.tn_dir.tn_seq_arena = NULL;
		node->tn_spec.tn_dir.tn_next_seq = TMPFS_DIRSEQ_START;
		node->tn_spec.tn_dir.tn_readdir_lastp = NULL;

		/* Extra link count for the virtual '.' entry. */
		node->tn_links++;
		break;
	case VFIFO:
	case VSOCK:
		break;
	case VLNK:
		node->tn_size = 0;
		node->tn_spec.tn_lnk.tn_link = NULL;
		break;
	case VREG:
		/* Regular file.  Create an underlying UVM object. */
		node->tn_spec.tn_reg.tn_aobj =
		    uao_create(INT32_MAX - PAGE_SIZE, 0);
		node->tn_spec.tn_reg.tn_aobj_pages = 0;
		break;
	default:
		panic("bad node type %d", vp->v_type);
		break;
	}

	tmpfs_init_vnode(vp, node);

	mutex_enter(&tmp->tm_lock);
	LIST_INSERT_HEAD(&tmp->tm_nodes, node, tn_entries);
	mutex_exit(&tmp->tm_lock);

	*key_len = sizeof(vp->v_data);
	*new_key = &vp->v_data;

	return 0;
}

/*
 * tmpfs_free_node: remove the inode from a list in the mount point and
 * destroy the inode structures.
 */
void
tmpfs_free_node(tmpfs_mount_t *tmp, tmpfs_node_t *node)
{
	size_t objsz;
	uint32_t hold;

	mutex_enter(&tmp->tm_lock);
	hold = atomic_or_32_nv(&node->tn_holdcount, TMPFS_NODE_RECLAIMED);
	/* Defer destruction to last thread holding this node. */
	if (hold != TMPFS_NODE_RECLAIMED) {
		mutex_exit(&tmp->tm_lock);
		return;
	}
	LIST_REMOVE(node, tn_entries);
	mutex_exit(&tmp->tm_lock);

	switch (node->tn_type) {
	case VLNK:
		if (node->tn_size > 0) {
			tmpfs_strname_free(tmp, node->tn_spec.tn_lnk.tn_link,
			    node->tn_size);
		}
		break;
	case VREG:
		/*
		 * Calculate the size of inode data, decrease the used-memory
		 * counter, and destroy the unerlying UVM object (if any).
		 */
		objsz = PAGE_SIZE * node->tn_spec.tn_reg.tn_aobj_pages;
		if (objsz != 0) {
			tmpfs_mem_decr(tmp, objsz);
		}
		if (node->tn_spec.tn_reg.tn_aobj != NULL) {
			uao_detach(node->tn_spec.tn_reg.tn_aobj);
		}
		break;
	case VDIR:
		KASSERT(node->tn_size == 0);
		KASSERT(node->tn_spec.tn_dir.tn_seq_arena == NULL);
		KASSERT(TAILQ_EMPTY(&node->tn_spec.tn_dir.tn_dir));
		KASSERT(node->tn_spec.tn_dir.tn_parent == NULL ||
		    node == tmp->tm_root);
		break;
	default:
		break;
	}
	KASSERT(node->tn_vnode == NULL);
	KASSERT(node->tn_links == 0);

	tmpfs_node_put(tmp, node);
}

/*
 * tmpfs_construct_node: allocate a new file of specified type and adds it
 * into the parent directory.
 *
 * => Credentials of the caller are used.
 */
int
tmpfs_construct_node(vnode_t *dvp, vnode_t **vpp, struct vattr *vap,
    struct componentname *cnp, char *target)
{
	tmpfs_mount_t *tmp = VFS_TO_TMPFS(dvp->v_mount);
	tmpfs_node_t *dnode = VP_TO_TMPFS_DIR(dvp), *node;
	tmpfs_dirent_t *de, *wde;
	char *slink = NULL;
	int ssize = 0;
	int error;

	/* Allocate symlink target. */
	if (target != NULL) {
		KASSERT(vap->va_type == VLNK);
		ssize = strlen(target);
		KASSERT(ssize < MAXPATHLEN);
		if (ssize > 0) {
			slink = tmpfs_strname_alloc(tmp, ssize);
			if (slink == NULL)
				return ENOSPC;
			memcpy(slink, target, ssize);
		}
	}

	/* Allocate a directory entry that points to the new file. */
	error = tmpfs_alloc_dirent(tmp, cnp->cn_nameptr, cnp->cn_namelen, &de);
	if (error) {
		if (slink != NULL)
			tmpfs_strname_free(tmp, slink, ssize);
		return error;
	}

	/* Allocate a vnode that represents the new file. */
	error = vcache_new(dvp->v_mount, dvp, vap, cnp->cn_cred, vpp);
	if (error) {
		if (slink != NULL)
			tmpfs_strname_free(tmp, slink, ssize);
		tmpfs_free_dirent(tmp, de);
		return error;
	}
	error = vn_lock(*vpp, LK_EXCLUSIVE);
	if (error) {
		vrele(*vpp);
		*vpp = NULL;
		if (slink != NULL)
			tmpfs_strname_free(tmp, slink, ssize);
		tmpfs_free_dirent(tmp, de);
		return error;
	}

	node = VP_TO_TMPFS_NODE(*vpp);

	if (slink != NULL) {
		node->tn_spec.tn_lnk.tn_link = slink;
		node->tn_size = ssize;
	}

	/* Remove whiteout before adding the new entry. */
	if (cnp->cn_flags & ISWHITEOUT) {
		wde = tmpfs_dir_lookup(dnode, cnp);
		KASSERT(wde != NULL && wde->td_node == TMPFS_NODE_WHITEOUT);
		tmpfs_dir_detach(dnode, wde);
		tmpfs_free_dirent(tmp, wde);
	}

	/* Associate inode and attach the entry into the directory. */
	tmpfs_dir_attach(dnode, de, node);

	/* Make node opaque if requested. */
	if (cnp->cn_flags & ISWHITEOUT)
		node->tn_flags |= UF_OPAQUE;

	/* Update the parent's timestamps. */
	tmpfs_update(dvp, TMPFS_UPDATE_MTIME | TMPFS_UPDATE_CTIME);

	VOP_UNLOCK(*vpp);

	return 0;
}

/*
 * tmpfs_alloc_dirent: allocates a new directory entry for the inode.
 * The directory entry contains a path name component.
 */
int
tmpfs_alloc_dirent(tmpfs_mount_t *tmp, const char *name, uint16_t len,
    tmpfs_dirent_t **de)
{
	tmpfs_dirent_t *nde;

	nde = tmpfs_dirent_get(tmp);
	if (nde == NULL)
		return ENOSPC;

	nde->td_name = tmpfs_strname_alloc(tmp, len);
	if (nde->td_name == NULL) {
		tmpfs_dirent_put(tmp, nde);
		return ENOSPC;
	}
	nde->td_namelen = len;
	memcpy(nde->td_name, name, len);
	nde->td_seq = TMPFS_DIRSEQ_NONE;

	*de = nde;
	return 0;
}

/*
 * tmpfs_free_dirent: free a directory entry.
 */
void
tmpfs_free_dirent(tmpfs_mount_t *tmp, tmpfs_dirent_t *de)
{
	KASSERT(de->td_node == NULL);
	KASSERT(de->td_seq == TMPFS_DIRSEQ_NONE);
	tmpfs_strname_free(tmp, de->td_name, de->td_namelen);
	tmpfs_dirent_put(tmp, de);
}

/*
 * tmpfs_dir_attach: associate directory entry with a specified inode,
 * and attach the entry into the directory, specified by vnode.
 *
 * => Increases link count on the associated node.
 * => Increases link count on directory node if our node is VDIR.
 * => It is caller's responsibility to check for the LINK_MAX limit.
 * => Triggers kqueue events here.
 */
void
tmpfs_dir_attach(tmpfs_node_t *dnode, tmpfs_dirent_t *de, tmpfs_node_t *node)
{
	vnode_t *dvp = dnode->tn_vnode;
	int events = NOTE_WRITE;

	KASSERT(dvp != NULL);
	KASSERT(VOP_ISLOCKED(dvp));

	/* Get a new sequence number. */
	KASSERT(de->td_seq == TMPFS_DIRSEQ_NONE);
	de->td_seq = tmpfs_dir_getseq(dnode, de);

	/* Associate directory entry and the inode. */
	de->td_node = node;
	if (node != TMPFS_NODE_WHITEOUT) {
		KASSERT(node->tn_links < LINK_MAX);
		node->tn_links++;

		/* Save the hint (might overwrite). */
		node->tn_dirent_hint = de;
	} else if ((dnode->tn_gen & TMPFS_WHITEOUT_BIT) == 0) {
		/* Flag that there are whiteout entries. */
		atomic_or_32(&dnode->tn_gen, TMPFS_WHITEOUT_BIT);
	}

	/* Insert the entry to the directory (parent of inode). */
	TAILQ_INSERT_TAIL(&dnode->tn_spec.tn_dir.tn_dir, de, td_entries);
	dnode->tn_size += sizeof(tmpfs_dirent_t);
	uvm_vnp_setsize(dvp, dnode->tn_size);

	if (node != TMPFS_NODE_WHITEOUT && node->tn_type == VDIR) {
		/* Set parent. */
		KASSERT(node->tn_spec.tn_dir.tn_parent == NULL);
		node->tn_spec.tn_dir.tn_parent = dnode;

		/* Increase the link count of parent. */
		KASSERT(dnode->tn_links < LINK_MAX);
		dnode->tn_links++;
		events |= NOTE_LINK;

		TMPFS_VALIDATE_DIR(node);
	}
	VN_KNOTE(dvp, events);
}

/*
 * tmpfs_dir_detach: disassociate directory entry and its inode,
 * and detach the entry from the directory, specified by vnode.
 *
 * => Decreases link count on the associated node.
 * => Decreases the link count on directory node, if our node is VDIR.
 * => Triggers kqueue events here.
 *
 * => Note: dvp and vp may be NULL only if called by tmpfs_unmount().
 */
void
tmpfs_dir_detach(tmpfs_node_t *dnode, tmpfs_dirent_t *de)
{
	tmpfs_node_t *node = de->td_node;
	vnode_t *vp, *dvp = dnode->tn_vnode;
	int events = NOTE_WRITE;

	KASSERT(dvp == NULL || VOP_ISLOCKED(dvp));

	if (__predict_true(node != TMPFS_NODE_WHITEOUT)) {
		/* Deassociate the inode and entry. */
		node->tn_dirent_hint = NULL;

		KASSERT(node->tn_links > 0);
		node->tn_links--;

		if ((vp = node->tn_vnode) != NULL) {
			KASSERT(VOP_ISLOCKED(vp));
			VN_KNOTE(vp, node->tn_links ? NOTE_LINK : NOTE_DELETE);
		}

		/* If directory - decrease the link count of parent. */
		if (node->tn_type == VDIR) {
			KASSERT(node->tn_spec.tn_dir.tn_parent == dnode);
			node->tn_spec.tn_dir.tn_parent = NULL;

			KASSERT(dnode->tn_links > 0);
			dnode->tn_links--;
			events |= NOTE_LINK;
		}
	}
	de->td_node = NULL;

	/* Remove the entry from the directory. */
	if (dnode->tn_spec.tn_dir.tn_readdir_lastp == de) {
		dnode->tn_spec.tn_dir.tn_readdir_lastp = NULL;
	}
	TAILQ_REMOVE(&dnode->tn_spec.tn_dir.tn_dir, de, td_entries);
	dnode->tn_size -= sizeof(tmpfs_dirent_t);
	tmpfs_dir_putseq(dnode, de);

	if (dvp) {
		uvm_vnp_setsize(dvp, dnode->tn_size);
		VN_KNOTE(dvp, events);
	}
}

/*
 * tmpfs_dir_lookup: find a directory entry in the specified inode.
 *
 * Note that the . and .. components are not allowed as they do not
 * physically exist within directories.
 */
tmpfs_dirent_t *
tmpfs_dir_lookup(tmpfs_node_t *node, struct componentname *cnp)
{
	const char *name = cnp->cn_nameptr;
	const uint16_t nlen = cnp->cn_namelen;
	tmpfs_dirent_t *de;

	KASSERT(VOP_ISLOCKED(node->tn_vnode));
	KASSERT(nlen != 1 || !(name[0] == '.'));
	KASSERT(nlen != 2 || !(name[0] == '.' && name[1] == '.'));
	TMPFS_VALIDATE_DIR(node);

	TAILQ_FOREACH(de, &node->tn_spec.tn_dir.tn_dir, td_entries) {
		if (de->td_namelen != nlen)
			continue;
		if (memcmp(de->td_name, name, nlen) != 0)
			continue;
		break;
	}
	return de;
}

/*
 * tmpfs_dir_cached: get a cached directory entry if it is valid.  Used to
 * avoid unnecessary tmpfs_dir_lookup().
 *
 * => The vnode must be locked.
 */
tmpfs_dirent_t *
tmpfs_dir_cached(tmpfs_node_t *node)
{
	tmpfs_dirent_t *de = node->tn_dirent_hint;

	KASSERT(VOP_ISLOCKED(node->tn_vnode));

	if (de == NULL) {
		return NULL;
	}
	KASSERT(de->td_node == node);

	/*
	 * Directories always have a valid hint.  For files, check if there
	 * are any hard links.  If there are - hint might be invalid.
	 */
	return (node->tn_type != VDIR && node->tn_links > 1) ? NULL : de;
}

/*
 * tmpfs_dir_getseq: get a per-directory sequence number for the entry.
 *
 * => Shall not be larger than 2^31 for linux32 compatibility.
 */
uint32_t
tmpfs_dir_getseq(tmpfs_node_t *dnode, tmpfs_dirent_t *de)
{
	uint32_t seq = de->td_seq;
	vmem_t *seq_arena;
	vmem_addr_t off;
	int error __diagused;

	TMPFS_VALIDATE_DIR(dnode);

	if (__predict_true(seq != TMPFS_DIRSEQ_NONE)) {
		/* Already set. */
		KASSERT(seq >= TMPFS_DIRSEQ_START);
		return seq;
	}

	/*
	 * The "." and ".." and the end-of-directory have reserved numbers.
	 * The other sequence numbers are allocated as following:
	 *
	 * - The first half of the 2^31 is assigned incrementally.
	 *
	 * - If that range is exceeded, then the second half of 2^31
	 * is used, but managed by vmem(9).
	 */

	seq = dnode->tn_spec.tn_dir.tn_next_seq;
	KASSERT(seq >= TMPFS_DIRSEQ_START);

	if (__predict_true(seq < TMPFS_DIRSEQ_END)) {
		/* First half: just increment and return. */
		dnode->tn_spec.tn_dir.tn_next_seq++;
		return seq;
	}

	/*
	 * First half exceeded, use the second half.  May need to create
	 * vmem(9) arena for the directory first.
	 */
	if ((seq_arena = dnode->tn_spec.tn_dir.tn_seq_arena) == NULL) {
		seq_arena = vmem_create("tmpfscoo", 0,
		    TMPFS_DIRSEQ_END - 1, 1, NULL, NULL, NULL, 0,
		    VM_SLEEP, IPL_NONE);
		dnode->tn_spec.tn_dir.tn_seq_arena = seq_arena;
		KASSERT(seq_arena != NULL);
	}
	error = vmem_alloc(seq_arena, 1, VM_SLEEP | VM_BESTFIT, &off);
	KASSERT(error == 0);

	KASSERT(off < TMPFS_DIRSEQ_END);
	seq = off | TMPFS_DIRSEQ_END;
	return seq;
}

static void
tmpfs_dir_putseq(tmpfs_node_t *dnode, tmpfs_dirent_t *de)
{
	vmem_t *seq_arena = dnode->tn_spec.tn_dir.tn_seq_arena;
	uint32_t seq = de->td_seq;

	TMPFS_VALIDATE_DIR(dnode);

	if (seq == TMPFS_DIRSEQ_NONE || seq < TMPFS_DIRSEQ_END) {
		/* First half (or no sequence number set yet). */
		KASSERT(de->td_seq >= TMPFS_DIRSEQ_START);
	} else {
		/* Second half. */
		KASSERT(seq_arena != NULL);
		KASSERT(seq >= TMPFS_DIRSEQ_END);
		seq &= ~TMPFS_DIRSEQ_END;
		vmem_free(seq_arena, seq, 1);
	}
	de->td_seq = TMPFS_DIRSEQ_NONE;

	/* Empty?  We can reset. */
	if (seq_arena && dnode->tn_size == 0) {
		dnode->tn_spec.tn_dir.tn_seq_arena = NULL;
		dnode->tn_spec.tn_dir.tn_next_seq = TMPFS_DIRSEQ_START;
		vmem_destroy(seq_arena);
	}
}

/*
 * tmpfs_dir_lookupbyseq: lookup a directory entry by the sequence number.
 */
tmpfs_dirent_t *
tmpfs_dir_lookupbyseq(tmpfs_node_t *node, off_t seq)
{
	tmpfs_dirent_t *de = node->tn_spec.tn_dir.tn_readdir_lastp;

	TMPFS_VALIDATE_DIR(node);

	/*
	 * First, check the cache.  If does not match - perform a lookup.
	 */
	if (de && de->td_seq == seq) {
		KASSERT(de->td_seq >= TMPFS_DIRSEQ_START);
		KASSERT(de->td_seq != TMPFS_DIRSEQ_NONE);
		return de;
	}
	TAILQ_FOREACH(de, &node->tn_spec.tn_dir.tn_dir, td_entries) {
		KASSERT(de->td_seq >= TMPFS_DIRSEQ_START);
		KASSERT(de->td_seq != TMPFS_DIRSEQ_NONE);
		if (de->td_seq == seq)
			return de;
	}
	return NULL;
}

/*
 * tmpfs_dir_getdotents: helper function for tmpfs_readdir() to get the
 * dot meta entries, that is, "." or "..".  Copy it to the UIO space.
 */
static int
tmpfs_dir_getdotents(tmpfs_node_t *node, struct dirent *dp, struct uio *uio)
{
	tmpfs_dirent_t *de;
	off_t next = 0;
	int error;

	switch (uio->uio_offset) {
	case TMPFS_DIRSEQ_DOT:
		dp->d_fileno = node->tn_id;
		strlcpy(dp->d_name, ".", sizeof(dp->d_name));
		next = TMPFS_DIRSEQ_DOTDOT;
		break;
	case TMPFS_DIRSEQ_DOTDOT:
		dp->d_fileno = node->tn_spec.tn_dir.tn_parent->tn_id;
		strlcpy(dp->d_name, "..", sizeof(dp->d_name));
		de = TAILQ_FIRST(&node->tn_spec.tn_dir.tn_dir);
		next = de ? tmpfs_dir_getseq(node, de) : TMPFS_DIRSEQ_EOF;
		break;
	default:
		KASSERT(false);
	}
	dp->d_type = DT_DIR;
	dp->d_namlen = strlen(dp->d_name);
	dp->d_reclen = _DIRENT_SIZE(dp);

	if (dp->d_reclen > uio->uio_resid) {
		return EJUSTRETURN;
	}
	if ((error = uiomove(dp, dp->d_reclen, uio)) != 0) {
		return error;
	}

	uio->uio_offset = next;
	return error;
}

/*
 * tmpfs_dir_getdents: helper function for tmpfs_readdir.
 *
 * => Returns as much directory entries as can fit in the uio space.
 * => The read starts at uio->uio_offset.
 */
int
tmpfs_dir_getdents(tmpfs_node_t *node, struct uio *uio, off_t *cntp)
{
	tmpfs_dirent_t *de;
	struct dirent dent;
	int error = 0;

	KASSERT(VOP_ISLOCKED(node->tn_vnode));
	TMPFS_VALIDATE_DIR(node);

	/*
	 * First check for the "." and ".." cases.
	 * Note: tmpfs_dir_getdotents() will "seek" for us.
	 */
	memset(&dent, 0, sizeof(dent));

	if (uio->uio_offset == TMPFS_DIRSEQ_DOT) {
		if ((error = tmpfs_dir_getdotents(node, &dent, uio)) != 0) {
			goto done;
		}
		(*cntp)++;
	}
	if (uio->uio_offset == TMPFS_DIRSEQ_DOTDOT) {
		if ((error = tmpfs_dir_getdotents(node, &dent, uio)) != 0) {
			goto done;
		}
		(*cntp)++;
	}

	/* Done if we reached the end. */
	if (uio->uio_offset == TMPFS_DIRSEQ_EOF) {
		goto done;
	}

	/* Locate the directory entry given by the given sequence number. */
	de = tmpfs_dir_lookupbyseq(node, uio->uio_offset);
	if (de == NULL) {
		error = EINVAL;
		goto done;
	}

	/*
	 * Read as many entries as possible; i.e., until we reach the end
	 * of the directory or we exhaust UIO space.
	 */
	do {
		if (de->td_node == TMPFS_NODE_WHITEOUT) {
			dent.d_fileno = 1;
			dent.d_type = DT_WHT;
		} else {
			dent.d_fileno = de->td_node->tn_id;
			dent.d_type = vtype2dt(de->td_node->tn_type);
		}
		dent.d_namlen = de->td_namelen;
		KASSERT(de->td_namelen < sizeof(dent.d_name));
		memcpy(dent.d_name, de->td_name, de->td_namelen);
		dent.d_name[de->td_namelen] = '\0';
		dent.d_reclen = _DIRENT_SIZE(&dent);

		if (dent.d_reclen > uio->uio_resid) {
			/* Exhausted UIO space. */
			error = EJUSTRETURN;
			break;
		}

		/* Copy out the directory entry and continue. */
		error = uiomove(&dent, dent.d_reclen, uio);
		if (error) {
			break;
		}
		(*cntp)++;
		de = TAILQ_NEXT(de, td_entries);

	} while (uio->uio_resid > 0 && de);

	/* Cache the last entry or clear and mark EOF. */
	uio->uio_offset = de ? tmpfs_dir_getseq(node, de) : TMPFS_DIRSEQ_EOF;
	node->tn_spec.tn_dir.tn_readdir_lastp = de;
done:
	tmpfs_update(node->tn_vnode, TMPFS_UPDATE_ATIME);

	if (error == EJUSTRETURN) {
		/* Exhausted UIO space - just return. */
		error = 0;
	}
	KASSERT(error >= 0);
	return error;
}

/*
 * tmpfs_reg_resize: resize the underlying UVM object associated with the 
 * specified regular file.
 */
int
tmpfs_reg_resize(struct vnode *vp, off_t newsize)
{
	tmpfs_mount_t *tmp = VFS_TO_TMPFS(vp->v_mount);
	tmpfs_node_t *node = VP_TO_TMPFS_NODE(vp);
	struct uvm_object *uobj = node->tn_spec.tn_reg.tn_aobj;
	size_t newpages, oldpages;
	off_t oldsize;

	KASSERT(vp->v_type == VREG);
	KASSERT(newsize >= 0);

	oldsize = node->tn_size;
	oldpages = round_page(oldsize) >> PAGE_SHIFT;
	newpages = round_page(newsize) >> PAGE_SHIFT;
	KASSERT(oldpages == node->tn_spec.tn_reg.tn_aobj_pages);

	if (newpages > oldpages) {
		/* Increase the used-memory counter if getting extra pages. */
		if (!tmpfs_mem_incr(tmp, (newpages - oldpages) << PAGE_SHIFT)) {
			return ENOSPC;
		}
	} else if (newsize < oldsize) {
		size_t zerolen;

		zerolen = MIN(round_page(newsize), node->tn_size) - newsize;
		ubc_zerorange(uobj, newsize, zerolen, UBC_UNMAP_FLAG(vp));
	}

	node->tn_spec.tn_reg.tn_aobj_pages = newpages;
	node->tn_size = newsize;
	uvm_vnp_setsize(vp, newsize);

	/*
	 * Free "backing store".
	 */
	if (newpages < oldpages) {
		KASSERT(uobj->vmobjlock == vp->v_interlock);

		mutex_enter(uobj->vmobjlock);
		uao_dropswap_range(uobj, newpages, oldpages);
		mutex_exit(uobj->vmobjlock);

		/* Decrease the used-memory counter. */
		tmpfs_mem_decr(tmp, (oldpages - newpages) << PAGE_SHIFT);
	}
	if (newsize > oldsize) {
		VN_KNOTE(vp, NOTE_EXTEND);
	}
	return 0;
}

/*
 * tmpfs_chflags: change flags of the given vnode.
 */
int
tmpfs_chflags(vnode_t *vp, int flags, kauth_cred_t cred, lwp_t *l)
{
	tmpfs_node_t *node = VP_TO_TMPFS_NODE(vp);
	kauth_action_t action = KAUTH_VNODE_WRITE_FLAGS;
	int error;
	bool changing_sysflags = false;

	KASSERT(VOP_ISLOCKED(vp));

	/* Disallow this operation if the file system is mounted read-only. */
	if (vp->v_mount->mnt_flag & MNT_RDONLY)
		return EROFS;

	/*
	 * If the new flags have non-user flags that are different than
	 * those on the node, we need special permission to change them.
	 */
	if ((flags & SF_SETTABLE) != (node->tn_flags & SF_SETTABLE)) {
		action |= KAUTH_VNODE_WRITE_SYSFLAGS;
		changing_sysflags = true;
	}

	/*
	 * Indicate that this node's flags have system attributes in them if
	 * that's the case.
	 */
	if (node->tn_flags & (SF_IMMUTABLE | SF_APPEND)) {
		action |= KAUTH_VNODE_HAS_SYSFLAGS;
	}

	error = kauth_authorize_vnode(cred, action, vp, NULL,
	    genfs_can_chflags(cred, vp->v_type, node->tn_uid,
	    changing_sysflags));
	if (error)
		return error;

	/*
	 * Set the flags. If we're not setting non-user flags, be careful not
	 * to overwrite them.
	 *
	 * XXX: Can't we always assign here? if the system flags are different,
	 *      the code above should catch attempts to change them without
	 *      proper permissions, and if we're here it means it's okay to
	 *      change them...
	 */
	if (!changing_sysflags) {
		/* Clear all user-settable flags and re-set them. */
		node->tn_flags &= SF_SETTABLE;
		node->tn_flags |= (flags & UF_SETTABLE);
	} else {
		node->tn_flags = flags;
	}
	tmpfs_update(vp, TMPFS_UPDATE_CTIME);
	VN_KNOTE(vp, NOTE_ATTRIB);
	return 0;
}

/*
 * tmpfs_chmod: change access mode on the given vnode.
 */
int
tmpfs_chmod(vnode_t *vp, mode_t mode, kauth_cred_t cred, lwp_t *l)
{
	tmpfs_node_t *node = VP_TO_TMPFS_NODE(vp);
	int error;

	KASSERT(VOP_ISLOCKED(vp));

	/* Disallow this operation if the file system is mounted read-only. */
	if (vp->v_mount->mnt_flag & MNT_RDONLY)
		return EROFS;

	/* Immutable or append-only files cannot be modified, either. */
	if (node->tn_flags & (IMMUTABLE | APPEND))
		return EPERM;

	error = kauth_authorize_vnode(cred, KAUTH_VNODE_WRITE_SECURITY, vp,
	    NULL, genfs_can_chmod(vp->v_type, cred, node->tn_uid, node->tn_gid, mode));
	if (error) {
		return error;
	}
	node->tn_mode = (mode & ALLPERMS);
	tmpfs_update(vp, TMPFS_UPDATE_CTIME);
	VN_KNOTE(vp, NOTE_ATTRIB);
	return 0;
}

/*
 * tmpfs_chown: change ownership of the given vnode.
 *
 * => At least one of uid or gid must be different than VNOVAL.
 * => Attribute is unchanged for VNOVAL case.
 */
int
tmpfs_chown(vnode_t *vp, uid_t uid, gid_t gid, kauth_cred_t cred, lwp_t *l)
{
	tmpfs_node_t *node = VP_TO_TMPFS_NODE(vp);
	int error;

	KASSERT(VOP_ISLOCKED(vp));

	/* Assign default values if they are unknown. */
	KASSERT(uid != VNOVAL || gid != VNOVAL);
	if (uid == VNOVAL) {
		uid = node->tn_uid;
	}
	if (gid == VNOVAL) {
		gid = node->tn_gid;
	}

	/* Disallow this operation if the file system is mounted read-only. */
	if (vp->v_mount->mnt_flag & MNT_RDONLY)
		return EROFS;

	/* Immutable or append-only files cannot be modified, either. */
	if (node->tn_flags & (IMMUTABLE | APPEND))
		return EPERM;

	error = kauth_authorize_vnode(cred, KAUTH_VNODE_CHANGE_OWNERSHIP, vp,
	    NULL, genfs_can_chown(cred, node->tn_uid, node->tn_gid, uid,
	    gid));
	if (error) {
		return error;
	}
	node->tn_uid = uid;
	node->tn_gid = gid;
	tmpfs_update(vp, TMPFS_UPDATE_CTIME);
	VN_KNOTE(vp, NOTE_ATTRIB);
	return 0;
}

/*
 * tmpfs_chsize: change size of the given vnode.
 */
int
tmpfs_chsize(vnode_t *vp, u_quad_t size, kauth_cred_t cred, lwp_t *l)
{
	tmpfs_node_t *node = VP_TO_TMPFS_NODE(vp);
	const off_t length = size;
	int error;

	KASSERT(VOP_ISLOCKED(vp));

	/* Decide whether this is a valid operation based on the file type. */
	switch (vp->v_type) {
	case VDIR:
		return EISDIR;
	case VREG:
		if (vp->v_mount->mnt_flag & MNT_RDONLY) {
			return EROFS;
		}
		break;
	case VBLK:
	case VCHR:
	case VFIFO:
		/*
		 * Allow modifications of special files even if in the file
		 * system is mounted read-only (we are not modifying the
		 * files themselves, but the objects they represent).
		 */
		return 0;
	default:
		return EOPNOTSUPP;
	}

	/* Immutable or append-only files cannot be modified, either. */
	if (node->tn_flags & (IMMUTABLE | APPEND)) {
		return EPERM;
	}

	if (length < 0) {
		return EINVAL;
	}
	if (node->tn_size == length) {
		return 0;
	}

	/* Note: tmpfs_reg_resize() will raise NOTE_EXTEND and NOTE_ATTRIB. */
	if ((error = tmpfs_reg_resize(vp, length)) != 0) {
		return error;
	}
	tmpfs_update(vp, TMPFS_UPDATE_CTIME | TMPFS_UPDATE_MTIME);
	return 0;
}

/*
 * tmpfs_chtimes: change access and modification times for vnode.
 */
int
tmpfs_chtimes(vnode_t *vp, const struct timespec *atime,
    const struct timespec *mtime, const struct timespec *btime,
    int vaflags, kauth_cred_t cred, lwp_t *l)
{
	tmpfs_node_t *node = VP_TO_TMPFS_NODE(vp);
	int error;

	KASSERT(VOP_ISLOCKED(vp));

	/* Disallow this operation if the file system is mounted read-only. */
	if (vp->v_mount->mnt_flag & MNT_RDONLY)
		return EROFS;

	/* Immutable or append-only files cannot be modified, either. */
	if (node->tn_flags & (IMMUTABLE | APPEND))
		return EPERM;

	error = kauth_authorize_vnode(cred, KAUTH_VNODE_WRITE_TIMES, vp, NULL,
	    genfs_can_chtimes(vp, vaflags, node->tn_uid, cred));
	if (error)
		return error;

	if (atime->tv_sec != VNOVAL) {
		node->tn_atime = *atime;
	}
	if (mtime->tv_sec != VNOVAL) {
		node->tn_mtime = *mtime;
	}
	if (btime->tv_sec != VNOVAL) {
		node->tn_birthtime = *btime;
	}
	VN_KNOTE(vp, NOTE_ATTRIB);
	return 0;
}

/*
 * tmpfs_update: update the timestamps as indicated by the flags.
 */
void
tmpfs_update(vnode_t *vp, unsigned tflags)
{
	tmpfs_node_t *node = VP_TO_TMPFS_NODE(vp);
	struct timespec nowtm;

	if (tflags == 0) {
		return;
	}
	vfs_timestamp(&nowtm);

	if (tflags & TMPFS_UPDATE_ATIME) {
		node->tn_atime = nowtm;
	}
	if (tflags & TMPFS_UPDATE_MTIME) {
		node->tn_mtime = nowtm;
	}
	if (tflags & TMPFS_UPDATE_CTIME) {
		node->tn_ctime = nowtm;
	}
}
