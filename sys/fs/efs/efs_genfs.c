/*	$NetBSD: efs_genfs.c,v 1.1 2007/06/29 23:30:28 rumble Exp $	*/

/*
 * Copyright (c) 2006 Stephen M. Rumble <rumble@ephemeral.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: efs_genfs.c,v 1.1 2007/06/29 23:30:28 rumble Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/vnode.h>

#include <miscfs/genfs/genfs.h>
#include <miscfs/genfs/genfs_node.h>

#include <fs/efs/efs.h>
#include <fs/efs/efs_sb.h>
#include <fs/efs/efs_dir.h>
#include <fs/efs/efs_genfs.h>
#include <fs/efs/efs_mount.h>
#include <fs/efs/efs_extent.h>
#include <fs/efs/efs_dinode.h>
#include <fs/efs/efs_inode.h>
#include <fs/efs/efs_subr.h>

const struct genfs_ops efs_genfsops = {
	.gop_size  = genfs_size,
	.gop_alloc = efs_gop_alloc,
	.gop_write = genfs_gop_write,
};

int
efs_gop_alloc(struct vnode *vp, off_t off, off_t len, int flags,
    kauth_cred_t cred)
{

	return (0);
}
