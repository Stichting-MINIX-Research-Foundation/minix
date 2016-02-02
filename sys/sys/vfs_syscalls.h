/*     $NetBSD: vfs_syscalls.h,v 1.22 2014/01/25 02:28:31 christos Exp $        */

/*
 * Copyright (c) 2007, 2008, 2009 The NetBSD Foundation, Inc.
 * All rights reserved.
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

#ifndef _SYS_VFS_SYSCALLS_H_
#define _SYS_VFS_SYSCALLS_H_

#include <sys/types.h>
#include <sys/fstypes.h>

struct stat;
struct statvfs;
struct quotactl_args;

/*
 * syscall helpers for compat code.
 */

/* Status functions to kernel 'struct stat' buffers */
int do_sys_stat(const char *, unsigned int, struct stat *);
int do_sys_statat(struct lwp *, int, const char *, unsigned int, struct stat *);
int do_fhstat(struct lwp *, const void *, size_t, struct stat *);
int do_fhstatvfs(struct lwp *, const void *, size_t, struct statvfs *, int);

/* VFS status functions to kernel buffers */
int do_sys_pstatvfs(struct lwp *, const char *, int, struct statvfs *);
int do_sys_fstatvfs(struct lwp *, int, int, struct statvfs *);
/* VFS status - call copyfn() for each entry */
int do_sys_getvfsstat(struct lwp *, void *, size_t, int, int (*)(const void *, void *, size_t), size_t, register_t *);

int do_sys_utimes(struct lwp *, struct vnode *, const char *, int,
    const struct timeval *, enum uio_seg);
int do_sys_utimens(struct lwp *, struct vnode *, const char *, int,
    const struct timespec *, enum uio_seg);
int do_sys_utimensat(struct lwp *, int, struct vnode *, const char *, int,
    const struct timespec *, enum uio_seg);

struct pathbuf;
int do_open(struct lwp *, struct vnode *, struct pathbuf *, int, int, int *);

int	vfs_copyinfh_alloc(const void *, size_t, fhandle_t **);
void	vfs_copyinfh_free(fhandle_t *);

int dofhopen(struct lwp *, const void *, size_t, int, register_t *);

int	do_sys_linkat(struct lwp *, int, const char *, int, const char *, int, register_t *);
int	do_sys_unlink(const char *, enum uio_seg);
int	do_sys_rename(const char *, const char *, enum uio_seg, int);
int	do_sys_mknod(struct lwp *, const char *, mode_t, dev_t, register_t *, enum uio_seg);
int	do_sys_mknodat(struct lwp *, int, const char *, mode_t, dev_t, register_t *, enum uio_seg);
int	do_sys_chmodat(struct lwp *, int, const char *, int, int);
int	do_sys_chownat(struct lwp *, int, const char *, uid_t, gid_t, int);
int	do_sys_accessat(struct lwp *, int, const char *, int ,int);
int	do_sys_mkdir(const char *, mode_t, enum uio_seg);
int	do_sys_symlink(const char *, const char *, enum uio_seg);
int	do_sys_quotactl(const char *, const struct quotactl_args *);
void	do_sys_sync(struct lwp *);

int	chdir_lookup(const char *, int, struct vnode **, struct lwp *);
void	change_root(struct cwdinfo *, struct vnode *, struct lwp *);

#endif /* _SYS_VFS_SYSCALLS_H_ */
