/*	$NetBSD: tmpfs_fifoops.h,v 1.8 2011/05/24 20:17:49 rmind Exp $	*/

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

#ifndef _FS_TMPFS_TMPFS_FIFOOPS_H_
#define _FS_TMPFS_TMPFS_FIFOOPS_H_

#if !defined(_KERNEL)
#error not supposed to be exposed to userland.
#endif

#include <miscfs/fifofs/fifo.h>
#include <fs/tmpfs/tmpfs_vnops.h>

/*
 * Declarations for tmpfs_fifoops.c.
 */

extern int (**tmpfs_fifoop_p)(void *);

#define	tmpfs_fifo_lookup	vn_fifo_bypass
#define	tmpfs_fifo_create	vn_fifo_bypass
#define	tmpfs_fifo_mknod	vn_fifo_bypass
#define	tmpfs_fifo_open		vn_fifo_bypass
int	tmpfs_fifo_close	(void *);
#define	tmpfs_fifo_access	tmpfs_access
#define	tmpfs_fifo_getattr	tmpfs_getattr
#define	tmpfs_fifo_setattr	tmpfs_setattr
int	tmpfs_fifo_read		(void *);
int	tmpfs_fifo_write	(void *);
#define	tmpfs_fifo_fcntl	tmpfs_fcntl
#define	tmpfs_fifo_ioctl	vn_fifo_bypass
#define	tmpfs_fifo_poll		vn_fifo_bypass
#define	tmpfs_fifo_kqfilter	vn_fifo_bypass
#define	tmpfs_fifo_revoke	vn_fifo_bypass
#define	tmpfs_fifo_mmap		vn_fifo_bypass
#define	tmpfs_fifo_fsync	vn_fifo_bypass
#define	tmpfs_fifo_seek		vn_fifo_bypass
#define	tmpfs_fifo_remove	vn_fifo_bypass
#define	tmpfs_fifo_link		vn_fifo_bypass
#define	tmpfs_fifo_rename	vn_fifo_bypass
#define	tmpfs_fifo_mkdir	vn_fifo_bypass
#define	tmpfs_fifo_rmdir	vn_fifo_bypass
#define	tmpfs_fifo_symlink	vn_fifo_bypass
#define	tmpfs_fifo_readdir	vn_fifo_bypass
#define	tmpfs_fifo_readlink	vn_fifo_bypass
#define	tmpfs_fifo_abortop	vn_fifo_bypass
#define	tmpfs_fifo_inactive	tmpfs_inactive
#define	tmpfs_fifo_reclaim	tmpfs_reclaim
#define	tmpfs_fifo_lock		tmpfs_lock
#define	tmpfs_fifo_unlock	tmpfs_unlock
#define	tmpfs_fifo_bmap		vn_fifo_bypass
#define	tmpfs_fifo_strategy	vn_fifo_bypass
#define	tmpfs_fifo_print	tmpfs_print
#define	tmpfs_fifo_pathconf	vn_fifo_bypass
#define	tmpfs_fifo_islocked	tmpfs_islocked
#define	tmpfs_fifo_advlock	vn_fifo_bypass
#define	tmpfs_fifo_bwrite	tmpfs_bwrite
#define	tmpfs_fifo_getpages	genfs_badop
#define	tmpfs_fifo_putpages	vn_fifo_bypass

#endif /* _FS_TMPFS_TMPFS_FIFOOPS_H_ */
