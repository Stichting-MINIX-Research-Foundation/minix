/*	$NetBSD: tmpfs_vfsops.c,v 1.65 2015/07/06 10:07:12 hannken Exp $	*/

/*
 * Copyright (c) 2005, 2006, 2007 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Julio M. Merino Vidal, developed as part of Google's Summer of Code
 * 2005 program.
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
 * Efficient memory file system.
 *
 * tmpfs is a file system that uses NetBSD's virtual memory sub-system
 * (the well-known UVM) to store file data and metadata in an efficient
 * way.  This means that it does not follow the structure of an on-disk
 * file system because it simply does not need to.  Instead, it uses
 * memory-specific data structures and algorithms to automatically
 * allocate and release resources.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: tmpfs_vfsops.c,v 1.65 2015/07/06 10:07:12 hannken Exp $");

#include <sys/param.h>
#include <sys/atomic.h>
#include <sys/types.h>
#include <sys/kmem.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/systm.h>
#include <sys/vnode.h>
#include <sys/kauth.h>
#include <sys/module.h>

#include <miscfs/genfs/genfs.h>
#include <fs/tmpfs/tmpfs.h>
#include <fs/tmpfs/tmpfs_args.h>

MODULE(MODULE_CLASS_VFS, tmpfs, NULL);

struct pool	tmpfs_dirent_pool;
struct pool	tmpfs_node_pool;

void
tmpfs_init(void)
{

	pool_init(&tmpfs_dirent_pool, sizeof(tmpfs_dirent_t), 0, 0, 0,
	    "tmpfs_dirent", &pool_allocator_nointr, IPL_NONE);
	pool_init(&tmpfs_node_pool, sizeof(tmpfs_node_t), 0, 0, 0,
	    "tmpfs_node", &pool_allocator_nointr, IPL_NONE);
}

void
tmpfs_done(void)
{

	pool_destroy(&tmpfs_dirent_pool);
	pool_destroy(&tmpfs_node_pool);
}

int
tmpfs_mount(struct mount *mp, const char *path, void *data, size_t *data_len)
{
	struct tmpfs_args *args = data;
	tmpfs_mount_t *tmp;
	tmpfs_node_t *root;
	struct vattr va;
	struct vnode *vp;
	uint64_t memlimit;
	ino_t nodes;
	int error;

	if (args == NULL)
		return EINVAL;

	/* Validate the version. */
	if (*data_len < sizeof(*args) ||
	    args->ta_version != TMPFS_ARGS_VERSION)
		return EINVAL;

	/* Handle retrieval of mount point arguments. */
	if (mp->mnt_flag & MNT_GETARGS) {
		if (mp->mnt_data == NULL)
			return EIO;
		tmp = VFS_TO_TMPFS(mp);

		args->ta_version = TMPFS_ARGS_VERSION;
		args->ta_nodes_max = tmp->tm_nodes_max;
		args->ta_size_max = tmp->tm_mem_limit;

		root = tmp->tm_root;
		args->ta_root_uid = root->tn_uid;
		args->ta_root_gid = root->tn_gid;
		args->ta_root_mode = root->tn_mode;

		*data_len = sizeof(*args);
		return 0;
	}


	/* Prohibit mounts if there is not enough memory. */
	if (tmpfs_mem_info(true) < uvmexp.freetarg)
		return EINVAL;

	/* Check for invalid uid and gid arguments */
	if (args->ta_root_uid == VNOVAL || args->ta_root_gid == VNOVAL)
		return EINVAL;

	/* This can never happen? */
	if ((args->ta_root_mode & ALLPERMS) == VNOVAL)
		return EINVAL;

	/* Get the memory usage limit for this file-system. */
	if (args->ta_size_max < PAGE_SIZE) {
		memlimit = UINT64_MAX;
	} else {
		memlimit = args->ta_size_max;
	}
	KASSERT(memlimit > 0);

	if (args->ta_nodes_max <= 3) {
		nodes = 3 + (memlimit / 1024);
	} else {
		nodes = args->ta_nodes_max;
	}
	nodes = MIN(nodes, INT_MAX);
	KASSERT(nodes >= 3);

	if (mp->mnt_flag & MNT_UPDATE) {
		tmp = VFS_TO_TMPFS(mp);
		if (nodes < tmp->tm_nodes_cnt)
			return EBUSY;
		if ((error = tmpfs_mntmem_set(tmp, memlimit)) != 0)
			return error;
		tmp->tm_nodes_max = nodes;
		root = tmp->tm_root;
		root->tn_uid = args->ta_root_uid;
		root->tn_gid = args->ta_root_gid;
		root->tn_mode = args->ta_root_mode;
		return 0;
	}

	/* Allocate the tmpfs mount structure and fill it. */
	tmp = kmem_zalloc(sizeof(tmpfs_mount_t), KM_SLEEP);
	if (tmp == NULL)
		return ENOMEM;

	tmp->tm_nodes_max = nodes;
	tmp->tm_nodes_cnt = 0;
	LIST_INIT(&tmp->tm_nodes);

	mutex_init(&tmp->tm_lock, MUTEX_DEFAULT, IPL_NONE);
	tmpfs_mntmem_init(tmp, memlimit);
	mp->mnt_data = tmp;

	/* Allocate the root node. */
	vattr_null(&va);
	va.va_type = VDIR;
	va.va_mode = args->ta_root_mode & ALLPERMS;
	va.va_uid = args->ta_root_uid;
	va.va_gid = args->ta_root_gid;
	error = vcache_new(mp, NULL, &va, NOCRED, &vp);
	root = VP_TO_TMPFS_NODE(vp);
	KASSERT(error == 0 && root != NULL);

	/*
	 * Parent of the root inode is itself.  Also, root inode has no
	 * directory entry (i.e. is never attached), thus hold an extra
	 * reference (link) for it.
	 */
	root->tn_links++;
	root->tn_spec.tn_dir.tn_parent = root;
	tmp->tm_root = root;
	vrele(vp);

	mp->mnt_flag |= MNT_LOCAL;
	mp->mnt_stat.f_namemax = TMPFS_MAXNAMLEN;
	mp->mnt_fs_bshift = PAGE_SHIFT;
	mp->mnt_dev_bshift = DEV_BSHIFT;
	mp->mnt_iflag |= IMNT_MPSAFE;
	vfs_getnewfsid(mp);

	error = set_statvfs_info(path, UIO_USERSPACE, "tmpfs", UIO_SYSSPACE,
	    mp->mnt_op->vfs_name, mp, curlwp);
	if (error) {
		(void)tmpfs_unmount(mp, MNT_FORCE);
	}
	return error;
}

int
tmpfs_start(struct mount *mp, int flags)
{

	return 0;
}

int
tmpfs_unmount(struct mount *mp, int mntflags)
{
	tmpfs_mount_t *tmp = VFS_TO_TMPFS(mp);
	tmpfs_node_t *node, *cnode;
	int error, flags = 0;

	/* Handle forced unmounts. */
	if (mntflags & MNT_FORCE)
		flags |= FORCECLOSE;

	/* Finalize all pending I/O. */
	error = vflush(mp, NULL, flags);
	if (error != 0)
		return error;

	/*
	 * First round, detach and destroy all directory entries.
	 * Also, clear the pointers to the vnodes - they are gone.
	 */
	LIST_FOREACH(node, &tmp->tm_nodes, tn_entries) {
		tmpfs_dirent_t *de;

		node->tn_vnode = NULL;
		if (node->tn_type != VDIR) {
			continue;
		}
		while ((de = TAILQ_FIRST(&node->tn_spec.tn_dir.tn_dir)) != NULL) {
			cnode = de->td_node;
			if (cnode && cnode != TMPFS_NODE_WHITEOUT) {
				cnode->tn_vnode = NULL;
			}
			tmpfs_dir_detach(node, de);
			tmpfs_free_dirent(tmp, de);
		}
		/* Extra virtual entry (itself for the root). */
		node->tn_links--;
	}

	/* Release the reference on root (diagnostic). */
	node = tmp->tm_root;
	node->tn_links--;

	/* Second round, destroy all inodes. */
	while ((node = LIST_FIRST(&tmp->tm_nodes)) != NULL) {
		tmpfs_free_node(tmp, node);
	}

	/* Throw away the tmpfs_mount structure. */
	tmpfs_mntmem_destroy(tmp);
	mutex_destroy(&tmp->tm_lock);
	kmem_free(tmp, sizeof(*tmp));
	mp->mnt_data = NULL;

	return 0;
}

int
tmpfs_root(struct mount *mp, vnode_t **vpp)
{
	tmpfs_node_t *node = VFS_TO_TMPFS(mp)->tm_root;
	int error;

	error = vcache_get(mp, &node, sizeof(node), vpp);
	if (error)
		return error;
	error = vn_lock(*vpp, LK_EXCLUSIVE);
	if (error) {
		vrele(*vpp);
		*vpp = NULL;
		return error;
	}

	return 0;
}

int
tmpfs_vget(struct mount *mp, ino_t ino, vnode_t **vpp)
{

	return EOPNOTSUPP;
}

int
tmpfs_fhtovp(struct mount *mp, struct fid *fhp, vnode_t **vpp)
{
	tmpfs_mount_t *tmp = VFS_TO_TMPFS(mp);
	tmpfs_node_t *node;
	tmpfs_fid_t tfh;
	int error;

	if (fhp->fid_len != sizeof(tmpfs_fid_t)) {
		return EINVAL;
	}
	memcpy(&tfh, fhp, sizeof(tmpfs_fid_t));

	mutex_enter(&tmp->tm_lock);
	LIST_FOREACH(node, &tmp->tm_nodes, tn_entries) {
		if (node->tn_id == tfh.tf_id) {
			/* Prevent this node from disappearing. */
			atomic_inc_32(&node->tn_holdcount);
			break;
		}
	}
	mutex_exit(&tmp->tm_lock);
	if (node == NULL)
		return ESTALE;

	error = vcache_get(mp, &node, sizeof(node), vpp);
	/* If this node has been reclaimed free it now. */
	if (atomic_dec_32_nv(&node->tn_holdcount) == TMPFS_NODE_RECLAIMED) {
		KASSERT(error != 0);
		tmpfs_free_node(tmp, node);
	}
	if (error)
		return (error == ENOENT ? ESTALE : error);
	error = vn_lock(*vpp, LK_EXCLUSIVE);
	if (error) {
		vrele(*vpp);
		*vpp = NULL;
		return error;
	}
	if (TMPFS_NODE_GEN(node) != tfh.tf_gen) {
		vput(*vpp);
		*vpp = NULL;
		return ESTALE;
	}

	return 0;
}

int
tmpfs_vptofh(vnode_t *vp, struct fid *fhp, size_t *fh_size)
{
	tmpfs_fid_t tfh;
	tmpfs_node_t *node;

	if (*fh_size < sizeof(tmpfs_fid_t)) {
		*fh_size = sizeof(tmpfs_fid_t);
		return E2BIG;
	}
	*fh_size = sizeof(tmpfs_fid_t);
	node = VP_TO_TMPFS_NODE(vp);

	memset(&tfh, 0, sizeof(tfh));
	tfh.tf_len = sizeof(tmpfs_fid_t);
	tfh.tf_gen = TMPFS_NODE_GEN(node);
	tfh.tf_id = node->tn_id;
	memcpy(fhp, &tfh, sizeof(tfh));

	return 0;
}

int
tmpfs_statvfs(struct mount *mp, struct statvfs *sbp)
{
	tmpfs_mount_t *tmp;
	fsfilcnt_t freenodes;
	size_t avail;

	tmp = VFS_TO_TMPFS(mp);

	sbp->f_iosize = sbp->f_frsize = sbp->f_bsize = PAGE_SIZE;

	mutex_enter(&tmp->tm_acc_lock);
	avail =  tmpfs_pages_avail(tmp);
	sbp->f_blocks = (tmpfs_bytes_max(tmp) >> PAGE_SHIFT);
	sbp->f_bavail = sbp->f_bfree = avail;
	sbp->f_bresvd = 0;

	freenodes = MIN(tmp->tm_nodes_max - tmp->tm_nodes_cnt,
	    avail * PAGE_SIZE / sizeof(tmpfs_node_t));

	sbp->f_files = tmp->tm_nodes_cnt + freenodes;
	sbp->f_favail = sbp->f_ffree = freenodes;
	sbp->f_fresvd = 0;
	mutex_exit(&tmp->tm_acc_lock);

	copy_statvfs_info(sbp, mp);

	return 0;
}

int
tmpfs_sync(struct mount *mp, int waitfor, kauth_cred_t uc)
{

	return 0;
}

int
tmpfs_snapshot(struct mount *mp, vnode_t *vp, struct timespec *ctime)
{

	return EOPNOTSUPP;
}

/*
 * tmpfs vfs operations.
 */

extern const struct vnodeopv_desc tmpfs_fifoop_opv_desc;
extern const struct vnodeopv_desc tmpfs_specop_opv_desc;
extern const struct vnodeopv_desc tmpfs_vnodeop_opv_desc;

const struct vnodeopv_desc * const tmpfs_vnodeopv_descs[] = {
	&tmpfs_fifoop_opv_desc,
	&tmpfs_specop_opv_desc,
	&tmpfs_vnodeop_opv_desc,
	NULL,
};

struct vfsops tmpfs_vfsops = {
	.vfs_name = MOUNT_TMPFS,
	.vfs_min_mount_data = sizeof (struct tmpfs_args),
	.vfs_mount = tmpfs_mount,
	.vfs_start = tmpfs_start,
	.vfs_unmount = tmpfs_unmount,
	.vfs_root = tmpfs_root,
	.vfs_quotactl = (void *)eopnotsupp,
	.vfs_statvfs = tmpfs_statvfs,
	.vfs_sync = tmpfs_sync,
	.vfs_vget = tmpfs_vget,
	.vfs_loadvnode = tmpfs_loadvnode,
	.vfs_newvnode = tmpfs_newvnode,
	.vfs_fhtovp = tmpfs_fhtovp,
	.vfs_vptofh = tmpfs_vptofh,
	.vfs_init = tmpfs_init,
	.vfs_done = tmpfs_done,
	.vfs_snapshot = tmpfs_snapshot,
	.vfs_extattrctl = vfs_stdextattrctl,
	.vfs_suspendctl = (void *)eopnotsupp,
	.vfs_renamelock_enter = genfs_renamelock_enter,
	.vfs_renamelock_exit = genfs_renamelock_exit,
	.vfs_fsync = (void *)eopnotsupp,
	.vfs_opv_descs = tmpfs_vnodeopv_descs
};

static int
tmpfs_modcmd(modcmd_t cmd, void *arg)
{

	switch (cmd) {
	case MODULE_CMD_INIT:
		return vfs_attach(&tmpfs_vfsops);
	case MODULE_CMD_FINI:
		return vfs_detach(&tmpfs_vfsops);
	default:
		return ENOTTY;
	}
}
