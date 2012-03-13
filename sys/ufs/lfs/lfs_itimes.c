/*	$NetBSD: lfs_itimes.c,v 1.12 2008/04/28 20:24:11 martin Exp $	*/

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
__KERNEL_RCSID(0, "$NetBSD: lfs_itimes.c,v 1.12 2008/04/28 20:24:11 martin Exp $");

#include <sys/param.h>
#include <sys/time.h>
#include <sys/mount.h>
#include <sys/buf.h>

#include <ufs/ufs/inode.h>

#ifndef _KERNEL
#include "bufcache.h"
#include "vnode.h"
#include "lfs_user.h"
#define vnode uvnode
#define buf ubuf
#define panic call_panic
#else
#include <ufs/lfs/lfs_extern.h>
#include <sys/kauth.h>
#endif

#include <ufs/lfs/lfs.h>

void
lfs_itimes(struct inode *ip, const struct timespec *acc,
    const struct timespec *mod, const struct timespec *cre)
{
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
		ip->i_ffs1_atime = acc->tv_sec;
		ip->i_ffs1_atimensec = acc->tv_nsec;
		if (ip->i_lfs->lfs_version > 1) {
			struct lfs *fs = ip->i_lfs;
			struct buf *ibp;
			IFILE *ifp;

			LFS_IENTRY(ifp, ip->i_lfs, ip->i_number, ibp);
			ifp->if_atime_sec = acc->tv_sec;
			ifp->if_atime_nsec = acc->tv_nsec;
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
			ip->i_ffs1_mtime = mod->tv_sec;
			ip->i_ffs1_mtimensec = mod->tv_nsec;
			ip->i_modrev++;
		}
		if (ip->i_flag & (IN_CHANGE | IN_MODIFY)) {
#ifdef _KERNEL
			if (cre == NULL)
				cre = &now;
#endif
			ip->i_ffs1_ctime = cre->tv_sec;
			ip->i_ffs1_ctimensec = cre->tv_nsec;
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
