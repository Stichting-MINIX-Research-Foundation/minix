/*	$NetBSD: tmpfs_specops.h,v 1.8 2011/05/24 20:17:49 rmind Exp $	*/

/*
 * Copyright (c) 2005 The NetBSD Foundation, Inc.
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

#ifndef _FS_TMPFS_TMPFS_SPECOPS_H_
#define _FS_TMPFS_TMPFS_SPECOPS_H_

#if !defined(_KERNEL)
#error not supposed to be exposed to userland.
#endif

#include <miscfs/specfs/specdev.h>
#include <fs/tmpfs/tmpfs_vnops.h>

/*
 * Declarations for tmpfs_specops.c.
 */

extern int (**tmpfs_specop_p)(void *);

#define	tmpfs_spec_lookup	spec_lookup
#define	tmpfs_spec_create	spec_create
#define	tmpfs_spec_mknod	spec_mknod
#define	tmpfs_spec_open		spec_open
int	tmpfs_spec_close	(void *);
#define	tmpfs_spec_access	tmpfs_access
#define	tmpfs_spec_getattr	tmpfs_getattr
#define	tmpfs_spec_setattr	tmpfs_setattr
int	tmpfs_spec_read		(void *);
int	tmpfs_spec_write	(void *);
#define	tmpfs_spec_fcntl	tmpfs_fcntl
#define	tmpfs_spec_ioctl	spec_ioctl
#define	tmpfs_spec_poll		spec_poll
#define	tmpfs_spec_kqfilter	spec_kqfilter
#define	tmpfs_spec_revoke	spec_revoke
#define	tmpfs_spec_mmap		spec_mmap
#define	tmpfs_spec_fsync	spec_fsync
#define	tmpfs_spec_seek		spec_seek
#define	tmpfs_spec_remove	spec_remove
#define	tmpfs_spec_link		spec_link
#define	tmpfs_spec_rename	spec_rename
#define	tmpfs_spec_mkdir	spec_mkdir
#define	tmpfs_spec_rmdir	spec_rmdir
#define	tmpfs_spec_symlink	spec_symlink
#define	tmpfs_spec_readdir	spec_readdir
#define	tmpfs_spec_readlink	spec_readlink
#define	tmpfs_spec_abortop	spec_abortop
#define	tmpfs_spec_inactive	tmpfs_inactive
#define	tmpfs_spec_reclaim	tmpfs_reclaim
#define	tmpfs_spec_lock		tmpfs_lock
#define	tmpfs_spec_unlock	tmpfs_unlock
#define	tmpfs_spec_bmap		spec_bmap
#define	tmpfs_spec_strategy	spec_strategy
#define	tmpfs_spec_print	tmpfs_print
#define	tmpfs_spec_pathconf	spec_pathconf
#define	tmpfs_spec_islocked	tmpfs_islocked
#define	tmpfs_spec_advlock	spec_advlock
#define	tmpfs_spec_bwrite	vn_bwrite
#define	tmpfs_spec_getpages	spec_getpages
#define	tmpfs_spec_putpages	spec_putpages

#endif /* _FS_TMPFS_TMPFS_SPECOPS_H_ */
