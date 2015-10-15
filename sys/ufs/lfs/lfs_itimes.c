/*	$NetBSD: lfs_itimes.c,v 1.19 2015/09/01 06:08:37 dholland Exp $	*/

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
#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: lfs_itimes.c,v 1.19 2015/09/01 06:08:37 dholland Exp $");

#include <sys/param.h>
#include <sys/time.h>
#include <sys/mount.h>
#include <sys/buf.h>

#ifndef _KERNEL
#include "bufcache.h"
#include "vnode.h"
#include "lfs_user.h"
#define vnode uvnode
#define buf ubuf
#define panic call_panic
#else
#include <ufs/lfs/ulfs_inode.h>
#include <ufs/lfs/lfs_extern.h>
#include <sys/kauth.h>
#endif

#include <ufs/lfs/lfs.h>
#include <ufs/lfs/lfs_accessors.h>
#include <ufs/lfs/lfs_inode.h>

void
lfs_itimes(struct inode *ip, const struct timespec *acc,
    const struct timespec *mod, const struct timespec *cre)
{
	struct lfs *fs = ip->i_lfs;
#ifdef _KERNEL
	struct timespec now;

	KASSERT(ip->i_flag & (IN_ACCESS | IN_CHANGE | IN_UPDATE | IN_MODIFY));

	vfs_timestamp(&now);
#endif

	if (ip->i_flag & IN_ACCESS) {
#ifdef _KERNEL
		if (acc == NULL)
			acc = &now;
#endif
		lfs_dino_setatime(fs, ip->i_din, acc->tv_sec);
		lfs_dino_setatimensec(fs, ip->i_din, acc->tv_nsec);
		if (fs->lfs_is64 || lfs_sb_getversion(fs) > 1) {
			struct buf *ibp;
			IFILE *ifp;

			LFS_IENTRY(ifp, fs, ip->i_number, ibp);
			lfs_if_setatime_sec(fs, ifp, acc->tv_sec);
			lfs_if_setatime_nsec(fs, ifp, acc->tv_nsec);
			LFS_BWRITE_LOG(ibp);
			mutex_enter(&lfs_lock);
			fs->lfs_flags |= LFS_IFDIRTY;
			mutex_exit(&lfs_lock);
		} else {
			mutex_enter(&lfs_lock);
			LFS_SET_UINO(ip, IN_ACCESSED);
			mutex_exit(&lfs_lock);
		}
	}
	if (ip->i_flag & (IN_CHANGE | IN_UPDATE | IN_MODIFY)) {
		if (ip->i_flag & (IN_UPDATE | IN_MODIFY)) {
#ifdef _KERNEL
			if (mod == NULL)
				mod = &now;
#endif
			lfs_dino_setmtime(fs, ip->i_din, mod->tv_sec);
			lfs_dino_setmtimensec(fs, ip->i_din, mod->tv_nsec);
			ip->i_modrev++;
		}
		if (ip->i_flag & (IN_CHANGE | IN_MODIFY)) {
#ifdef _KERNEL
			if (cre == NULL)
				cre = &now;
#endif
			lfs_dino_setctime(fs, ip->i_din, cre->tv_sec);
			lfs_dino_setctimensec(fs, ip->i_din, cre->tv_nsec);
		}
		mutex_enter(&lfs_lock);
		if (ip->i_flag & (IN_CHANGE | IN_UPDATE))
			LFS_SET_UINO(ip, IN_MODIFIED);
		if (ip->i_flag & IN_MODIFY)
			LFS_SET_UINO(ip, IN_ACCESSED);
		mutex_exit(&lfs_lock);
	}
	ip->i_flag &= ~(IN_ACCESS | IN_CHANGE | IN_UPDATE | IN_MODIFY);
}
