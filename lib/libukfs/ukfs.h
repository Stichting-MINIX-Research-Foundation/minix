/*	$NetBSD: ukfs.h,v 1.14 2012/07/19 06:33:03 joerg Exp $	*/

/*
 * Copyright (c) 2007, 2008, 2009  Antti Kantee.  All Rights Reserved.
 *
 * Development of this software was supported by the
 * Finnish Cultural Foundation.
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

#ifndef _RUMP_UKFS_H_
#define _RUMP_UKFS_H_

#include <sys/types.h>

#include <stdint.h>

/* don't include NetBSD <sys/header.h> for portability */
struct vnode;
struct stat;
struct timeval;

struct ukfs;
struct ukfs_dircookie;
struct ukfs_part;

#define UKFS_DEFAULTMP "/ukfs"

#define UKFS_RELFLAG_NOUNMOUNT	0x01
#define UKFS_RELFLAG_FORCE	0x02

#define UKFS_VERSION	002 /* secret ukfs 002 */
#define	ukfs_init()	_ukfs_init(UKFS_VERSION)

__BEGIN_DECLS

int		_ukfs_init(int);
struct ukfs	*ukfs_mount(const char *, const char *, const char *,
			    int, void *, size_t);
struct ukfs	*ukfs_mount_disk(const char *, const char *, struct ukfs_part *,
				 const char *, int, void *, size_t);
int		ukfs_release(struct ukfs *, int);

int		ukfs_opendir(struct ukfs *, const char *,
			     struct ukfs_dircookie **);
int		ukfs_getdents(struct ukfs *, const char *, off_t *,
			      uint8_t *, size_t);
int		ukfs_getdents_cookie(struct ukfs *, struct ukfs_dircookie *,
				     off_t *, uint8_t *, size_t);
int		ukfs_closedir(struct ukfs *, struct ukfs_dircookie *);

int		ukfs_open(struct ukfs *, const char *, int);
ssize_t		ukfs_read(struct ukfs *, const char *, off_t,
			      uint8_t *, size_t);
ssize_t		ukfs_read_fd(struct ukfs *, int, off_t, uint8_t *, size_t);
ssize_t		ukfs_write(struct ukfs *, const char *, off_t,
			       uint8_t *, size_t);
ssize_t		ukfs_write_fd(struct ukfs *, int, off_t, uint8_t *, size_t,int);
int		ukfs_close(struct ukfs *, int);

ssize_t		ukfs_readlink(struct ukfs *, const char *, char *, size_t);

int		ukfs_create(struct ukfs *, const char *, mode_t);
int		ukfs_mkdir(struct ukfs *, const char *, mode_t);
int		ukfs_mknod(struct ukfs *, const char *, mode_t, dev_t);
int		ukfs_mkfifo(struct ukfs *, const char *, mode_t);
int		ukfs_symlink(struct ukfs *, const char *, const char *);

int		ukfs_remove(struct ukfs *, const char *);
int		ukfs_rmdir(struct ukfs *, const char *);

int		ukfs_link(struct ukfs *, const char *, const char *);
int		ukfs_rename(struct ukfs *, const char *, const char *);

int		ukfs_chdir(struct ukfs *, const char *);

int		ukfs_stat(struct ukfs *, const char *, struct stat *);
int		ukfs_lstat(struct ukfs *, const char *, struct stat *);

int		ukfs_chmod(struct ukfs *, const char *, mode_t);
int		ukfs_lchmod(struct ukfs *, const char *, mode_t);
int		ukfs_chown(struct ukfs *, const char *, uid_t, gid_t);
int		ukfs_lchown(struct ukfs *, const char *, uid_t, gid_t);
int		ukfs_chflags(struct ukfs *, const char *, u_long);
int		ukfs_lchflags(struct ukfs *, const char *, u_long);

int		ukfs_utimes(struct ukfs *, const char *, 
			    const struct timeval *);
int		ukfs_lutimes(struct ukfs *, const char *, 
			     const struct timeval *);

struct mount	*ukfs_getmp(struct ukfs *);
struct vnode	*ukfs_getrvp(struct ukfs *);
void		ukfs_setspecific(struct ukfs *, void *);
void *		ukfs_getspecific(struct ukfs *);

/* partition magic in device names */
extern struct ukfs_part *ukfs_part_none;
extern struct ukfs_part *ukfs_part_na;
#define UKFS_PARTITION_SCANMAGIC "%PART:" /* deprecated */

#define UKFS_DISKLABEL_SCANMAGIC "%DISKLABEL:"
#define UKFS_DISKLABEL_MAGICLEN (sizeof(UKFS_DISKLABEL_SCANMAGIC "a%")-1)

#define UKFS_OFFSET_SCANMAGIC "%OFFSET:"
#define UKFS_OFFSET_MINLEN (sizeof(UKFS_OFFSET_SCANMAGIC "512,512%")-1)

#define UKFS_DEVICE_MAXSTR 128 /* unexact science ... */
#define UKFS_DEVICE_MAXPATHLEN (MAXPATHLEN+UKFS_DEVICE_MAXSTR)

#define UKFS_DEVICE_ARGVPROBE(part)					\
do {									\
	if (argc < 3)							\
		*part = NULL;						\
	else if (ukfs_part_probe(argv[argc-2], part) == -1)		\
			err(1, "ukfs_part_probe");			\
} while (/*CONSTCOND*/0)

int		ukfs_part_probe(char *, struct ukfs_part **);
void		ukfs_part_release(struct ukfs_part *);
int		ukfs_part_tostring(struct ukfs_part *, char *, size_t);

/* dynamic loading of library modules */
int		ukfs_modload(const char *);
int		ukfs_modload_dir(const char *);
ssize_t		ukfs_vfstypes(char *, size_t);

/* Utilities */
int		ukfs_util_builddirs(struct ukfs *, const char *, mode_t);

__END_DECLS

#endif /* _RUMP_UKFS_H_ */
