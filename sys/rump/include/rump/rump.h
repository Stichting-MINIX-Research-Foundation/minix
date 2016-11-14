/*	$NetBSD: rump.h,v 1.63 2014/06/13 15:45:02 pooka Exp $	*/

/*
 * Copyright (c) 2007-2011 Antti Kantee.  All Rights Reserved.
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

#ifndef _RUMP_RUMP_H_
#define _RUMP_RUMP_H_

/*
 * NOTE: do not #include anything from <sys> here.  Otherwise this
 * has no chance of working on non-NetBSD platforms.
 */

struct mount;
struct vnode;
struct vattr;
struct componentname;
struct vfsops;
struct fid;
struct statvfs;
struct stat;
struct kauth_cred;
struct lwp;
struct modinfo;
struct uio;

/* yetch */
#if defined(__NetBSD__)
#include <prop/proplib.h>
#else
#ifndef HAVE_PROP_DICTIONARY_T
#define HAVE_PROP_DICTIONARY_T
struct prop_dictionary;
typedef struct prop_dictionary *prop_dictionary_t;
#endif
#endif /* __NetBSD__ */

#if (!defined(_KERNEL)) && (defined(__sun__) || defined(__ANDROID__)) && !defined(RUMP_REGISTER_T)
#define RUMP_REGISTER_T long
typedef RUMP_REGISTER_T register_t;
#endif

#include <rump/rumpvnode_if.h>
#include <rump/rumpdefs.h>

/* rumpkern */
enum rump_uiorw { RUMPUIO_READ, RUMPUIO_WRITE };

enum rump_sigmodel {
	RUMP_SIGMODEL_PANIC,
	RUMP_SIGMODEL_IGNORE,
	RUMP_SIGMODEL__HOST_NOTANYMORE,
	RUMP_SIGMODEL_RAISE,
	RUMP_SIGMODEL_RECORD
};

/* flags to rump_lwproc_rfork */
#define RUMP_RFFDG	0x01
#define RUMP_RFCFDG	0x02
/* slightly-easier-to-parse aliases for the above */
#define RUMP_RFFD_SHARE 0x00 /* lossage */
#define RUMP_RFFD_COPY	RUMP_RFFDG
#define RUMP_RFFD_CLEAR	RUMP_RFCFDG

/* rumpvfs */
#define RUMPCN_FREECRED  0x02
#define RUMP_ETFS_SIZE_ENDOFF ((uint64_t)-1)
enum rump_etfs_type {
	RUMP_ETFS_REG,
	RUMP_ETFS_BLK,
	RUMP_ETFS_CHR,
	RUMP_ETFS_DIR,		/* only the registered directory */
	RUMP_ETFS_DIR_SUBDIRS	/* dir + subdirectories (recursive) */
};

/* um, what's the point ?-) */
#ifdef _BEGIN_DECLS
_BEGIN_DECLS
#endif

int	rump_getversion(void);
int	rump_pub_getversion(void); /* compat */
int	rump_nativeabi_p(void);

int	rump_boot_gethowto(void);
void	rump_boot_sethowto(int);
void	rump_boot_setsigmodel(enum rump_sigmodel);

struct rump_boot_etfs {
	/* client initializes */
	const char *eb_key;
	const char *eb_hostpath;
	enum rump_etfs_type eb_type;
	uint64_t eb_begin;
	uint64_t eb_size;

	/* rump kernel initializes */
	struct rump_boot_etfs *_eb_next;
	int eb_status;
};
void	rump_boot_etfs_register(struct rump_boot_etfs *);

void	rump_schedule(void);
void	rump_unschedule(void);

void	rump_printevcnts(void);

int	rump_daemonize_begin(void);
int	rump_init(void);
int	rump_init_server(const char *);
int	rump_daemonize_done(int);
#define RUMP_DAEMONIZE_SUCCESS 0

#ifndef _KERNEL
#include <rump/rumpkern_if_pub.h>
#include <rump/rumpvfs_if_pub.h>
#include <rump/rumpnet_if_pub.h>
#endif

#ifdef _END_DECLS
_END_DECLS
#endif

/*
 * Begin rump syscall conditionals.  Yes, something a little better
 * is required here.
 */
#ifdef RUMP_SYS_NETWORKING
#define socket(a,b,c) rump_sys_socket(a,b,c)
#define accept(a,b,c) rump_sys_accept(a,b,c)
#define bind(a,b,c) rump_sys_bind(a,b,c)
#define connect(a,b,c) rump_sys_connect(a,b,c)
#define getpeername(a,b,c) rump_sys_getpeername(a,b,c)
#define getsockname(a,b,c) rump_sys_getsockname(a,b,c)
#define listen(a,b) rump_sys_listen(a,b)
#define recvfrom(a,b,c,d,e,f) rump_sys_recvfrom(a,b,c,d,e,f)
#define recvmsg(a,b,c) rump_sys_recvmsg(a,b,c)
#define sendto(a,b,c,d,e,f) rump_sys_sendto(a,b,c,d,e,f)
#define sendmsg(a,b,c) rump_sys_sendmsg(a,b,c)
#define getsockopt(a,b,c,d,e) rump_sys_getsockopt(a,b,c,d,e)
#define setsockopt(a,b,c,d,e) rump_sys_setsockopt(a,b,c,d,e)
#define shutdown(a,b) rump_sys_shutdown(a,b)
#endif /* RUMP_SYS_NETWORKING */

#ifdef RUMP_SYS_IOCTL
#define ioctl(...) rump_sys_ioctl(__VA_ARGS__)
#define fnctl(...) rump_sys_fcntl(__VA_ARGS__)
#endif /* RUMP_SYS_IOCTL */

#ifdef RUMP_SYS_CLOSE
#define close(a) rump_sys_close(a)
#endif /* RUMP_SYS_CLOSE */

#ifdef RUMP_SYS_OPEN
#define open(...) rump_sys_open(__VA_ARGS__)
#endif /* RUMP_SYS_OPEN */

#ifdef RUMP_SYS_READWRITE
#define read(a,b,c) rump_sys_read(a,b,c)
#define readv(a,b,c) rump_sys_readv(a,b,c)
#define pread(a,b,c,d) rump_sys_pread(a,b,c,d)
#define preadv(a,b,c,d) rump_sys_preadv(a,b,c,d)
#define write(a,b,c) rump_sys_write(a,b,c)
#define writev(a,b,c) rump_sys_writev(a,b,c)
#define pwrite(a,b,c,d) rump_sys_pwrite(a,b,c,d)
#define pwritev(a,b,c,d) rump_sys_pwritev(a,b,c,d)
#endif /* RUMP_SYS_READWRITE */

#ifdef RUMP_SYS_FILEOPS
#define mkdir(a,b) rump_sys_mkdir(a,b)
#define rmdir(a) rump_sys_rmdir(a)
#define link(a,b) rump_sys_link(a,b)
#define symlink(a,b) rump_sys_symlink(a,b)
#define unlink(a) rump_sys_unlink(a)
#define readlink(a,b,c) rump_sys_readlink(a,b,c)
#define chdir(a) rump_sys_chdir(a)
#define fsync(a) rump_sys_fsync(a)
#define sync() rump_sys_sync()
#define chown(a,b,c) rump_sys_chown(a,b,c)
#define fchown(a,b,c) rump_sys_fchown(a,b,c)
#define lchown(a,b,c) rump_sys_lchown(a,b,c)
#define lseek(a,b,c) rump_sys_lseek(a,b,c)
#define mknod(a,b,c) rump_sys_mknod(a,b,c)
#define rename(a,b) rump_sys_rename(a,b)
#define truncate(a,b) rump_sys_truncate(a,b)
#define ftruncate(a,b) rump_sys_ftruncate(a,b)
#define umask(a) rump_sys_umask(a)
#define getdents(a,b,c) rump_sys_getdents(a,b,c)
#endif /* RUMP_SYS_FILEOPS */

#ifdef RUMP_SYS_STAT
#define stat(a,b) rump_sys_stat(a,b)
#define fstat(a,b) rump_sys_fstat(a,b)
#define lstat(a,b) rump_sys_lstat(a,b)
#endif /* RUMP_SYS_STAT */

#ifdef RUMP_SYS_PROCOPS
#define getpid() rump_sys_getpid()
#endif /* RUMP_SYS_PROCOPS */

#endif /* _RUMP_RUMP_H_ */
