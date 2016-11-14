/*	$NetBSD: rumpvfs_compat50.c,v 1.1 2015/04/22 17:00:59 pooka Exp $	*/

/*
 * Copyright (c) 2009 Antti Kantee.  All Rights Reserved.
 *
 * Development of this software was supported by the Nokia Foundation
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: rumpvfs_compat50.c,v 1.1 2015/04/22 17:00:59 pooka Exp $");

#include <sys/param.h>
#include <sys/kmem.h>
#include <sys/sched.h>
#include <sys/syscallargs.h>
#include <sys/vnode.h>

#include <compat/sys/time.h>

#include <rump/rump.h>

#include "rump_vfs_private.h"

/*
 * XXX: these are handwritten for now.  They provide compat for
 * calling post-time_t file systems from a pre-time_t userland.
 * (mknod is missing.  I don't care very much)
 *
 * Doing remote system calls with these does not (obviously) work.
 */
#if     BYTE_ORDER == BIG_ENDIAN
#define SPARG(p,k)      ((p)->k.be.datum)
#else /* LITTLE_ENDIAN, I hope dearly */
#define SPARG(p,k)      ((p)->k.le.datum)
#endif

struct vattr50 {
	enum vtype	va_type;	/* vnode type (for create) */
	mode_t		va_mode;	/* files access mode and type */
	nlink_t		va_nlink;	/* number of references to file */
	uid_t		va_uid;		/* owner user id */
	gid_t		va_gid;		/* owner group id */
	u_long		va_fsid;	/* file system id (dev for now) */
	ino_t		va_fileid;	/* file id */
	u_quad_t	va_size;	/* file size in bytes */
	long		va_blocksize;	/* blocksize preferred for i/o */
	struct timespec50 va_atime;	/* time of last access */
	struct timespec50 va_mtime;	/* time of last modification */
	struct timespec50 va_ctime;	/* time file changed */
	struct timespec50 va_birthtime;	/* time file created */
	u_long		va_gen;		/* generation number of file */
	u_long		va_flags;	/* flags defined for file */
	uint32_t	va_rdev;	/* device the special file represents */
	u_quad_t	va_bytes;	/* bytes of disk space held by file */
	u_quad_t	va_filerev;	/* file modification number */
	u_int		va_vaflags;	/* operations flags, see below */
	long		va_spare;	/* remain quad aligned */
};

/*
 * XXX: types.  But I don't want to start playing compat games in
 * the userspace namespace too
 */
void
rump_vattr50_to_vattr(const struct vattr *_va50, struct vattr *va)
{
	const struct vattr50 *va50 = (const struct vattr50 *)_va50;

	va->va_type = va50->va_type;
	va->va_mode = va50->va_mode;
	va->va_nlink = va50->va_nlink;
	va->va_uid = va50->va_uid;
	va->va_gid = va50->va_gid;
	va->va_fsid = (long)va50->va_fsid;
	va->va_fileid = va50->va_fileid;
	va->va_size = va50->va_size;
	va->va_blocksize = va50->va_blocksize;
	timespec50_to_timespec(&va50->va_atime, &va->va_atime);
	timespec50_to_timespec(&va50->va_ctime, &va->va_ctime);
	timespec50_to_timespec(&va50->va_mtime, &va->va_mtime);
	timespec50_to_timespec(&va50->va_birthtime, &va->va_birthtime);
	va->va_gen = va50->va_gen;
	va->va_flags = va50->va_flags;
	va->va_rdev = (int32_t)va50->va_rdev;
	va->va_bytes = va50->va_bytes;
	va->va_filerev = va50->va_filerev;
	va->va_vaflags = va50->va_flags;
}

void
rump_vattr_to_vattr50(const struct vattr *va, struct vattr *_va50)
{
	struct vattr50 *va50 = (struct vattr50 *)_va50;

	va50->va_type = va->va_type;
	va50->va_mode = va->va_mode;
	va50->va_nlink = va->va_nlink;
	va50->va_uid = va->va_uid;
	va50->va_gid = va->va_gid;
	va50->va_fsid = (u_long)va->va_fsid;
	va50->va_fileid = va->va_fileid;
	va50->va_size = va->va_size;
	va50->va_blocksize = va->va_blocksize;
	timespec_to_timespec50(&va->va_atime, &va50->va_atime);
	timespec_to_timespec50(&va->va_ctime, &va50->va_ctime);
	timespec_to_timespec50(&va->va_mtime, &va50->va_mtime);
	timespec_to_timespec50(&va->va_birthtime, &va50->va_birthtime);
	va50->va_gen = va->va_gen;
	va50->va_flags = va->va_flags;
	va50->va_rdev = (uint32_t)va->va_rdev;
	va50->va_bytes = va->va_bytes;
	va50->va_filerev = va->va_filerev;
	va50->va_vaflags = va->va_flags;
}
