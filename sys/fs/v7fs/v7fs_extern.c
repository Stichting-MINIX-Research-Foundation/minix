/*	$NetBSD: v7fs_extern.c,v 1.4 2014/12/29 15:29:38 hannken Exp $	*/

/*-
 * Copyright (c) 2004, 2011 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by UCHIYAMA Yasushi.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: v7fs_extern.c,v 1.4 2014/12/29 15:29:38 hannken Exp $");

#if defined _KERNEL_OPT
#include "opt_v7fs.h"
#endif
#include <sys/resource.h>
#include <sys/param.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/module.h>

#include <miscfs/fifofs/fifo.h>
#include <miscfs/genfs/genfs.h>
#include <miscfs/genfs/genfs_node.h>

#include <fs/v7fs/v7fs_extern.h>

MODULE(MODULE_CLASS_VFS, v7fs, NULL);

/* External interfaces */

int (**v7fs_vnodeop_p)(void *);	/* filled by getnewvnode (vnode.h) */

const struct vnodeopv_entry_desc v7fs_vnodeop_entries[] = {
	{ &vop_default_desc, vn_default_error },
	{ &vop_lookup_desc, v7fs_lookup },		/* lookup */
	{ &vop_create_desc, v7fs_create },		/* create */
	{ &vop_mknod_desc, v7fs_mknod },		/* mknod */
	{ &vop_open_desc, v7fs_open },			/* open */
	{ &vop_close_desc, v7fs_close },		/* close */
	{ &vop_access_desc, v7fs_access },		/* access */
	{ &vop_getattr_desc, v7fs_getattr },		/* getattr */
	{ &vop_setattr_desc, v7fs_setattr },		/* setattr */
	{ &vop_read_desc, v7fs_read },			/* read */
	{ &vop_write_desc, v7fs_write },		/* write */
	{ &vop_fallocate_desc, genfs_eopnotsupp },	/* fallocate */
	{ &vop_fdiscard_desc, genfs_eopnotsupp },	/* fdiscard */
	{ &vop_fcntl_desc, genfs_fcntl },		/* fcntl */
	{ &vop_ioctl_desc, genfs_enoioctl },		/* ioctl */
	{ &vop_poll_desc, genfs_poll },			/* poll */
	{ &vop_kqfilter_desc, genfs_kqfilter },		/* kqfilter */
	{ &vop_revoke_desc, genfs_revoke },		/* revoke */
	{ &vop_mmap_desc, genfs_mmap },			/* mmap */
	{ &vop_fsync_desc, v7fs_fsync },		/* fsync */
	{ &vop_seek_desc, genfs_seek },			/* seek */
	{ &vop_remove_desc, v7fs_remove },		/* remove */
	{ &vop_link_desc, v7fs_link },			/* link */
	{ &vop_rename_desc, v7fs_rename },		/* rename */
	{ &vop_mkdir_desc, v7fs_mkdir },		/* mkdir */
	{ &vop_rmdir_desc, v7fs_rmdir },		/* rmdir */
	{ &vop_symlink_desc, v7fs_symlink },		/* symlink */
	{ &vop_readdir_desc, v7fs_readdir },		/* readdir */
	{ &vop_readlink_desc, v7fs_readlink },		/* readlink */
	{ &vop_abortop_desc, genfs_abortop },		/* abortop */
	{ &vop_inactive_desc, v7fs_inactive },		/* inactive */
	{ &vop_reclaim_desc, v7fs_reclaim },		/* reclaim */
	{ &vop_lock_desc, genfs_lock },			/* lock */
	{ &vop_unlock_desc, genfs_unlock },		/* unlock */
	{ &vop_bmap_desc, v7fs_bmap },			/* bmap */
	{ &vop_strategy_desc, v7fs_strategy },		/* strategy */
	{ &vop_print_desc, v7fs_print },		/* print */
	{ &vop_islocked_desc, genfs_islocked },		/* islocked */
	{ &vop_pathconf_desc, v7fs_pathconf },		/* pathconf */
	{ &vop_advlock_desc, v7fs_advlock },		/* advlock */
	{ &vop_bwrite_desc, vn_bwrite },		/* bwrite */
	{ &vop_getpages_desc, genfs_getpages },		/* getpages */
	{ &vop_putpages_desc, genfs_putpages },		/* putpages */
	{ NULL, NULL }
};


int (**v7fs_specop_p)(void *);
const struct vnodeopv_entry_desc v7fs_specop_entries[] = {
	{ &vop_default_desc, vn_default_error },
	{ &vop_lookup_desc, spec_lookup },		/* lookup */
	{ &vop_create_desc, spec_create },		/* create xxx*/
	{ &vop_mknod_desc, spec_mknod },		/* mknod xxx*/
	{ &vop_open_desc, spec_open },			/* open */
	{ &vop_close_desc, spec_close },		/* close */
	{ &vop_access_desc, v7fs_access },		/* access */
	{ &vop_getattr_desc, v7fs_getattr },		/* getattr */
	{ &vop_setattr_desc, v7fs_setattr },		/* setattr */
	{ &vop_read_desc, spec_read },			/* read */
	{ &vop_write_desc, spec_write },		/* write */
	{ &vop_fallocate_desc, spec_fallocate },	/* fallocate */
	{ &vop_fdiscard_desc, spec_fdiscard },		/* fdiscard */
	{ &vop_ioctl_desc, spec_ioctl },		/* ioctl */
	{ &vop_fcntl_desc, genfs_fcntl },		/* fcntl */
	{ &vop_poll_desc, spec_poll },			/* poll */
	{ &vop_kqfilter_desc, spec_kqfilter },		/* kqfilter */
	{ &vop_revoke_desc, spec_revoke },		/* revoke */
	{ &vop_mmap_desc, spec_mmap },			/* mmap */
	{ &vop_fsync_desc, spec_fsync },		/* fsync */
	{ &vop_seek_desc, spec_seek },			/* seek */
	{ &vop_remove_desc, spec_remove },		/* remove */
	{ &vop_link_desc, spec_link },			/* link */
	{ &vop_rename_desc, spec_rename },		/* rename */
	{ &vop_mkdir_desc, spec_mkdir },		/* mkdir */
	{ &vop_rmdir_desc, spec_rmdir },		/* rmdir */
	{ &vop_symlink_desc, spec_symlink },		/* symlink */
	{ &vop_readdir_desc, spec_readdir },		/* readdir */
	{ &vop_readlink_desc, spec_readlink },		/* readlink */
	{ &vop_abortop_desc, spec_abortop },		/* abortop */
	{ &vop_inactive_desc, v7fs_inactive },		/* inactive */
	{ &vop_reclaim_desc, v7fs_reclaim },		/* reclaim */
	{ &vop_lock_desc, genfs_lock },			/* lock */
	{ &vop_unlock_desc, genfs_unlock },		/* unlock */
	{ &vop_bmap_desc, spec_bmap },			/* bmap */
	{ &vop_strategy_desc, spec_strategy },		/* strategy */
	{ &vop_print_desc, spec_print },		/* print */
	{ &vop_islocked_desc, genfs_islocked },		/* islocked */
	{ &vop_pathconf_desc, spec_pathconf },		/* pathconf */
	{ &vop_advlock_desc, spec_advlock },		/* advlock */
	{ &vop_bwrite_desc, vn_bwrite },		/* bwrite */
	{ &vop_getpages_desc, spec_getpages },		/* getpages */
	{ &vop_putpages_desc, spec_putpages },		/* putpages */
	{ NULL, NULL }
};

int (**v7fs_fifoop_p)(void *);
const struct vnodeopv_entry_desc v7fs_fifoop_entries[] = {
	{ &vop_default_desc, vn_default_error },
	{ &vop_lookup_desc, vn_fifo_bypass },		/* lookup */
	{ &vop_create_desc, vn_fifo_bypass },		/* create */
	{ &vop_mknod_desc, vn_fifo_bypass },		/* mknod */
	{ &vop_open_desc, vn_fifo_bypass },		/* open */
	{ &vop_close_desc, vn_fifo_bypass },		/* close */
	{ &vop_access_desc, v7fs_access },		/* access */
	{ &vop_getattr_desc, v7fs_getattr },		/* getattr */
	{ &vop_setattr_desc, v7fs_setattr },		/* setattr */
	{ &vop_read_desc, vn_fifo_bypass },		/* read */
	{ &vop_write_desc, vn_fifo_bypass },		/* write */
	{ &vop_fallocate_desc, vn_fifo_bypass },	/* fallocate */
	{ &vop_fdiscard_desc, vn_fifo_bypass },		/* fdiscard */
	{ &vop_ioctl_desc, vn_fifo_bypass },		/* ioctl */
	{ &vop_fcntl_desc, genfs_fcntl },		/* fcntl */
	{ &vop_poll_desc, vn_fifo_bypass },		/* poll */
	{ &vop_kqfilter_desc, vn_fifo_bypass },		/* kqfilter */
	{ &vop_revoke_desc, vn_fifo_bypass },		/* revoke */
	{ &vop_mmap_desc, vn_fifo_bypass },		/* mmap */
	{ &vop_fsync_desc, vn_fifo_bypass },		/* fsync */
	{ &vop_seek_desc, vn_fifo_bypass },		/* seek */
	{ &vop_remove_desc, vn_fifo_bypass },		/* remove */
	{ &vop_link_desc, vn_fifo_bypass },		/* link */
	{ &vop_rename_desc, vn_fifo_bypass },		/* rename */
	{ &vop_mkdir_desc, vn_fifo_bypass },		/* mkdir */
	{ &vop_rmdir_desc, vn_fifo_bypass },		/* rmdir */
	{ &vop_symlink_desc, vn_fifo_bypass },		/* symlink */
	{ &vop_readdir_desc, vn_fifo_bypass },		/* readdir */
	{ &vop_readlink_desc, vn_fifo_bypass },		/* readlink */
	{ &vop_abortop_desc, vn_fifo_bypass },		/* abortop */
	{ &vop_inactive_desc, v7fs_inactive },		/* inactive */
	{ &vop_reclaim_desc, v7fs_reclaim },		/* reclaim */
	{ &vop_lock_desc, vn_fifo_bypass },		/* lock */
	{ &vop_unlock_desc, vn_fifo_bypass },		/* unlock */
	{ &vop_bmap_desc, vn_fifo_bypass },		/* bmap */
	{ &vop_strategy_desc, vn_fifo_bypass },		/* strategy */
	{ &vop_print_desc, vn_fifo_bypass },		/* print */
	{ &vop_islocked_desc, vn_fifo_bypass },		/* islocked */
	{ &vop_pathconf_desc, vn_fifo_bypass },		/* pathconf */
	{ &vop_advlock_desc, vn_fifo_bypass },		/* advlock */
	{ &vop_bwrite_desc, vn_bwrite },		/* bwrite */
	{ &vop_putpages_desc, vn_fifo_bypass },		/* putpages */
	{ NULL, NULL }
};

const struct vnodeopv_desc v7fs_fifoop_opv_desc = {
	&v7fs_fifoop_p,
	v7fs_fifoop_entries
};

const struct vnodeopv_desc v7fs_vnodeop_opv_desc = {
	&v7fs_vnodeop_p,
	v7fs_vnodeop_entries
};

const struct vnodeopv_desc v7fs_specop_opv_desc = {
	&v7fs_specop_p,
	v7fs_specop_entries
};

const struct vnodeopv_desc *v7fs_vnodeopv_descs[] = {
	&v7fs_vnodeop_opv_desc,
	&v7fs_specop_opv_desc,
	&v7fs_fifoop_opv_desc,
	NULL,
};

const struct genfs_ops v7fs_genfsops = {
	.gop_size = genfs_size,
	.gop_alloc = v7fs_gop_alloc,
	.gop_write = genfs_gop_write,
};

struct vfsops v7fs_vfsops = {
	.vfs_name = MOUNT_V7FS,
	.vfs_min_mount_data = sizeof(struct v7fs_args),
	.vfs_mount = v7fs_mount,
	.vfs_start = v7fs_start,
	.vfs_unmount = v7fs_unmount,
	.vfs_root = v7fs_root,
	.vfs_quotactl = (void *)eopnotsupp,
	.vfs_statvfs = v7fs_statvfs,
	.vfs_sync = v7fs_sync,
	.vfs_vget = v7fs_vget,
	.vfs_loadvnode = v7fs_loadvnode,
	.vfs_fhtovp = v7fs_fhtovp,
	.vfs_vptofh = v7fs_vptofh,
	.vfs_init = v7fs_init,
	.vfs_reinit = v7fs_reinit,
	.vfs_done = v7fs_done,
	.vfs_mountroot = v7fs_mountroot,
	.vfs_snapshot = (void *)eopnotsupp,
	.vfs_extattrctl = vfs_stdextattrctl,
	.vfs_suspendctl = (void *)eopnotsupp,
	.vfs_renamelock_enter = genfs_renamelock_enter,
	.vfs_renamelock_exit = genfs_renamelock_exit,
	.vfs_fsync = (void *)eopnotsupp,
	.vfs_opv_descs = v7fs_vnodeopv_descs
};

static int
v7fs_modcmd(modcmd_t cmd, void *arg)
{

	switch (cmd) {
	case MODULE_CMD_INIT:
		return vfs_attach(&v7fs_vfsops);
	case MODULE_CMD_FINI:
		return vfs_detach(&v7fs_vfsops);
	default:
		return ENOTTY;
	}
}
