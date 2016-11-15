/*	$NetBSD: efs_genfs.h,v 1.1 2007/06/29 23:30:28 rumble Exp $	*/

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

#ifndef _FS_EFS_EFS_GENFS_H_
#define _FS_EFS_EFS_GENFS_H_

extern const struct genfs_ops efs_genfsops;

extern int efs_gop_alloc(struct vnode *, off_t, off_t, int, kauth_cred_t);

#endif /* !_FS_EFS_EFS_GENFS_H_*/
