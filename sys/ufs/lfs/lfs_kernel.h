/*	$NetBSD: lfs_kernel.h,v 1.2 2015/08/12 18:24:14 dholland Exp $	*/

/*  from NetBSD: lfs.h,v 1.157 2013/06/28 16:14:06 matt Exp  */

/*-
 * Copyright (c) 1999, 2000, 2001, 2002, 2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Konrad E. Schroder <perseant@hhhh.org>.
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
/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)lfs.h	8.9 (Berkeley) 5/8/95
 */

#ifndef _UFS_LFS_LFS_KERNEL_H_
#define _UFS_LFS_LFS_KERNEL_H_

#include <ufs/lfs/lfs.h>

extern struct lfs_stats lfs_stats;

/* XXX MP */
#define	LFS_SEGLOCK_HELD(fs) \
	((fs)->lfs_seglock != 0 &&					\
	 (fs)->lfs_lockpid == curproc->p_pid &&				\
	 (fs)->lfs_locklwp == curlwp->l_lid)

struct lfs_cluster {
	size_t bufsize;	       /* Size of kept data */
	struct buf **bpp;      /* Array of kept buffers */
	int bufcount;	       /* Number of kept buffers */
#define LFS_CL_MALLOC	0x00000001
#define LFS_CL_SHIFT	0x00000002
#define LFS_CL_SYNC	0x00000004
	u_int32_t flags;       /* Flags */
	struct lfs *fs;	       /* LFS that this belongs to */
	struct segment *seg;   /* Segment structure, for LFS_CL_SYNC */
};

/*
 * Splay tree containing block numbers allocated through lfs_balloc.
 */
struct lbnentry {
	SPLAY_ENTRY(lbnentry) entry;
	daddr_t lbn;
};

/*
 * Compat fcntls.  Defined for kernel only.  Userland always uses
 * "the one true version".
 */
#include <compat/sys/time_types.h>

struct lfs_fcntl_markv_70 {
	BLOCK_INFO_70 *blkiov;	/* blocks to relocate */
	int blkcnt;		/* number of blocks (limited to 65536) */
};

#define LFCNSEGWAITALL_COMPAT	 _FCNW_FSPRIV('L', 0, struct timeval50)
#define LFCNSEGWAIT_COMPAT	 _FCNW_FSPRIV('L', 1, struct timeval50)
#define LFCNIFILEFH_COMPAT	 _FCNW_FSPRIV('L', 5, struct lfs_fhandle)
#define LFCNIFILEFH_COMPAT2	 _FCN_FSPRIV(F_FSOUT, 'L', 11, 32)
#define LFCNWRAPSTOP_COMPAT	 _FCNO_FSPRIV('L', 9)
#define LFCNWRAPGO_COMPAT	 _FCNO_FSPRIV('L', 10)
#define LFCNSEGWAITALL_COMPAT_50 _FCNR_FSPRIV('L', 0, struct timeval50)
#define LFCNSEGWAIT_COMPAT_50	 _FCNR_FSPRIV('L', 1, struct timeval50)
#define LFCNBMAPV_COMPAT_70	_FCNRW_FSPRIV('L', 2, struct lfs_fcntl_markv_70)
#define LFCNMARKV_COMPAT_70	_FCNRW_FSPRIV('L', 3, struct lfs_fcntl_markv_70)


#endif /* _UFS_LFS_LFS_KERNEL_H_ */
