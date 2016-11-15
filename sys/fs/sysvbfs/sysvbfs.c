/*	$NetBSD: sysvbfs.c,v 1.15 2014/12/26 15:23:21 hannken Exp $	*/

/*-
 * Copyright (c) 2004 The NetBSD Foundation, Inc.
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
__KERNEL_RCSID(0, "$NetBSD: sysvbfs.c,v 1.15 2014/12/26 15:23:21 hannken Exp $");

#include <sys/resource.h>
#include <sys/param.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/module.h>
#include <miscfs/genfs/genfs.h>
#include <miscfs/genfs/genfs_node.h>
#include <fs/sysvbfs/sysvbfs.h>

MODULE(MODULE_CLASS_VFS, sysvbfs, NULL);

/* External interfaces */

int (**sysvbfs_vnodeop_p)(void *);	/* filled by getnewvnode (vnode.h) */

const struct vnodeopv_entry_desc sysvbfs_vnodeop_entries[] = {
	{ &vop_default_desc, vn_default_error },
	{ &vop_lookup_desc, sysvbfs_lookup },		/* lookup */
	{ &vop_create_desc, sysvbfs_create },		/* create */
	{ &vop_mknod_desc, genfs_eopnotsupp },		/* mknod */
	{ &vop_open_desc, sysvbfs_open },		/* open */
	{ &vop_close_desc, sysvbfs_close },		/* close */
	{ &vop_access_desc, sysvbfs_access },		/* access */
	{ &vop_getattr_desc, sysvbfs_getattr },		/* getattr */
	{ &vop_setattr_desc, sysvbfs_setattr },		/* setattr */
	{ &vop_read_desc, sysvbfs_read },		/* read */
	{ &vop_write_desc, sysvbfs_write },		/* write */
	{ &vop_fallocate_desc, genfs_eopnotsupp },	/* fallocate */
	{ &vop_fdiscard_desc, genfs_eopnotsupp },	/* fdiscard */
	{ &vop_fcntl_desc, genfs_fcntl },		/* fcntl */
	{ &vop_ioctl_desc, genfs_enoioctl },		/* ioctl */
	{ &vop_poll_desc, genfs_poll },			/* poll */
	{ &vop_kqfilter_desc, genfs_kqfilter },		/* kqfilter */
	{ &vop_revoke_desc, genfs_revoke },		/* revoke */
	{ &vop_mmap_desc, genfs_mmap },			/* mmap */
	{ &vop_fsync_desc, sysvbfs_fsync },		/* fsync */
	{ &vop_seek_desc, genfs_seek },			/* seek */
	{ &vop_remove_desc, sysvbfs_remove },		/* remove */
	{ &vop_link_desc, genfs_eopnotsupp },		/* link */
	{ &vop_rename_desc, sysvbfs_rename },		/* rename */
	{ &vop_mkdir_desc, genfs_eopnotsupp },		/* mkdir */
	{ &vop_rmdir_desc, genfs_eopnotsupp },		/* rmdir */
	{ &vop_symlink_desc, genfs_eopnotsupp },	/* symlink */
	{ &vop_readdir_desc, sysvbfs_readdir },		/* readdir */
	{ &vop_readlink_desc, genfs_eopnotsupp },	/* readlink */
	{ &vop_abortop_desc, genfs_abortop },		/* abortop */
	{ &vop_inactive_desc, sysvbfs_inactive },	/* inactive */
	{ &vop_reclaim_desc, sysvbfs_reclaim },		/* reclaim */
	{ &vop_lock_desc, genfs_lock },			/* lock */
	{ &vop_unlock_desc, genfs_unlock },		/* unlock */
	{ &vop_bmap_desc, sysvbfs_bmap },		/* bmap */
	{ &vop_strategy_desc, sysvbfs_strategy },	/* strategy */
	{ &vop_print_desc, sysvbfs_print },		/* print */
	{ &vop_islocked_desc, genfs_islocked },		/* islocked */
	{ &vop_pathconf_desc, sysvbfs_pathconf },	/* pathconf */
	{ &vop_advlock_desc, sysvbfs_advlock },		/* advlock */
	{ &vop_bwrite_desc, vn_bwrite },		/* bwrite */
	{ &vop_getpages_desc, genfs_getpages },		/* getpages */
	{ &vop_putpages_desc, genfs_putpages },		/* putpages */
	{ NULL, NULL }
};

const struct vnodeopv_desc sysvbfs_vnodeop_opv_desc = {
	&sysvbfs_vnodeop_p,
	sysvbfs_vnodeop_entries
};

const struct vnodeopv_desc *sysvbfs_vnodeopv_descs[] = {
	&sysvbfs_vnodeop_opv_desc,
	NULL,
};

const struct genfs_ops sysvbfs_genfsops = {
	.gop_size = genfs_size,
	.gop_alloc = sysvbfs_gop_alloc,
	.gop_write = genfs_gop_write,
};

struct vfsops sysvbfs_vfsops = {
	.vfs_name = MOUNT_SYSVBFS,
	.vfs_min_mount_data = sizeof (struct sysvbfs_args),
	.vfs_mount = sysvbfs_mount,
	.vfs_start = sysvbfs_start,
	.vfs_unmount = sysvbfs_unmount,
	.vfs_root = sysvbfs_root,
	.vfs_quotactl = (void *)eopnotsupp,
	.vfs_statvfs = sysvbfs_statvfs,
	.vfs_sync = sysvbfs_sync,
	.vfs_vget = sysvbfs_vget,
	.vfs_loadvnode = sysvbfs_loadvnode,
	.vfs_fhtovp = sysvbfs_fhtovp,
	.vfs_vptofh = sysvbfs_vptofh,
	.vfs_init = sysvbfs_init,
	.vfs_reinit = sysvbfs_reinit,
	.vfs_done = sysvbfs_done,
	.vfs_snapshot = (void *)eopnotsupp,
	.vfs_extattrctl = vfs_stdextattrctl,
	.vfs_suspendctl = (void *)eopnotsupp,
	.vfs_renamelock_enter = genfs_renamelock_enter,
	.vfs_renamelock_exit = genfs_renamelock_exit,
	.vfs_fsync = (void *)eopnotsupp,
	.vfs_opv_descs = sysvbfs_vnodeopv_descs
};

static int
sysvbfs_modcmd(modcmd_t cmd, void *arg)
{

	switch (cmd) {
	case MODULE_CMD_INIT:
		return vfs_attach(&sysvbfs_vfsops);
	case MODULE_CMD_FINI:
		return vfs_detach(&sysvbfs_vfsops);
	default:
		return ENOTTY;
	}
}
