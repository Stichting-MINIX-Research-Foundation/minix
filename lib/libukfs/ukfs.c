/*	$NetBSD: ukfs.c,v 1.58 2015/06/17 00:15:26 christos Exp $	*/

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

/*
 * This library enables access to files systems directly without
 * involving system calls.
 */

#ifdef __linux__
#define _XOPEN_SOURCE 500
#define _BSD_SOURCE
#define _FILE_OFFSET_BITS 64
#endif

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/mount.h>

#include <assert.h>
#include <dirent.h>
#include <dlfcn.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#if defined(__minix)
#define _MTHREADIFY_PTHREADS
#include <minix/mthread.h>
#else
#include <pthread.h>
#endif /* defined(__minix) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>

#include <rump/ukfs.h>

#include <rump/rump.h>
#include <rump/rump_syscalls.h>

#include "ukfs_int_disklabel.h"

#define UKFS_MODE_DEFAULT 0555

struct ukfs {
	pthread_spinlock_t ukfs_spin;

	struct mount *ukfs_mp;
	struct lwp *ukfs_lwp;
	void *ukfs_specific;

	int ukfs_devfd;

	char *ukfs_devpath;
	char *ukfs_mountpath;
	char *ukfs_cwd;

	struct ukfs_part *ukfs_part;
};

static int builddirs(const char *, mode_t,
    int (*mkdirfn)(struct ukfs *, const char *, mode_t), struct ukfs *);

struct mount *
ukfs_getmp(struct ukfs *ukfs)
{

	return ukfs->ukfs_mp;
}

void
ukfs_setspecific(struct ukfs *ukfs, void *priv)
{

	ukfs->ukfs_specific = priv;
}

void *
ukfs_getspecific(struct ukfs *ukfs)
{

	return ukfs->ukfs_specific;
}

#ifdef DONT_WANT_PTHREAD_LINKAGE
#define pthread_spin_lock(a)
#define pthread_spin_unlock(a)
#define pthread_spin_init(a,b)
#define pthread_spin_destroy(a)
#endif

static int
precall(struct ukfs *ukfs, struct lwp **curlwp)
{

	/* save previous.  ensure start from pristine context */
	*curlwp = rump_pub_lwproc_curlwp();
	if (*curlwp)
		rump_pub_lwproc_switch(ukfs->ukfs_lwp);
	rump_pub_lwproc_rfork(RUMP_RFCFDG);

	if (rump_sys_chroot(ukfs->ukfs_mountpath) == -1)
		return errno;
	if (rump_sys_chdir(ukfs->ukfs_cwd) == -1)
		return errno;

	return 0;
}

static void
postcall(struct lwp *curlwp)
{

	rump_pub_lwproc_releaselwp();
	if (curlwp)
		rump_pub_lwproc_switch(curlwp);
}

#define PRECALL()							\
struct lwp *ukfs_curlwp;						\
do {									\
	int ukfs_rv;							\
	if ((ukfs_rv = precall(ukfs, &ukfs_curlwp)) != 0) {		\
		errno = ukfs_rv;					\
		return -1;						\
	}								\
} while (/*CONSTCOND*/0)

#define POSTCALL() postcall(ukfs_curlwp);

struct ukfs_part {
	pthread_spinlock_t part_lck;
	int part_refcount;

	int part_type;
	char part_labelchar;
	off_t part_devoff;
	off_t part_devsize;
};

enum ukfs_parttype { UKFS_PART_NONE, UKFS_PART_DISKLABEL, UKFS_PART_OFFSET };

static struct ukfs_part ukfs__part_none = {
	.part_type = UKFS_PART_NONE,
	.part_devoff = 0,
	.part_devsize = RUMP_ETFS_SIZE_ENDOFF,
};
static struct ukfs_part ukfs__part_na;
struct ukfs_part *ukfs_part_none = &ukfs__part_none;
struct ukfs_part *ukfs_part_na = &ukfs__part_na;

#define PART2LOCKSIZE(len) ((len) == RUMP_ETFS_SIZE_ENDOFF ? 0 : (len))

int
_ukfs_init(int version)
{
	int rv;

	if (version != UKFS_VERSION) {
		errno = EPROGMISMATCH;
		warn("incompatible ukfs version, %d vs. %d",
		    version, UKFS_VERSION);
		return -1;
	}

	if ((rv = rump_init()) != 0) {
		errno = rv;
		return -1;
	}

	return 0;
}

/*ARGSUSED*/
static int
rumpmkdir(struct ukfs *dummy, const char *path, mode_t mode)
{

	return rump_sys_mkdir(path, mode);
}

int
ukfs_part_probe(char *devpath, struct ukfs_part **partp)
{
	struct ukfs_part *part;
	char *p;
	int error = 0;
	int devfd = -1;

	if ((p = strstr(devpath, UKFS_PARTITION_SCANMAGIC)) != NULL) {
		warnx("ukfs: %%PART is deprecated.  use "
		    "%%DISKLABEL instead");
		errno = ENODEV;
		return -1;
	}

	part = malloc(sizeof(*part));
	if (part == NULL) {
		errno = ENOMEM;
		return -1;
	}
	if (pthread_spin_init(&part->part_lck, PTHREAD_PROCESS_PRIVATE) == -1) {
		error = errno;
		free(part);
		errno = error;
		return -1;
	}
	part->part_type = UKFS_PART_NONE;
	part->part_refcount = 1;

	/*
	 * Check for magic in pathname:
	 *   disklabel: /regularpath%DISKLABEL:labelchar%\0
	 *     offsets: /regularpath%OFFSET:start,end%\0
	 */
#define MAGICADJ_DISKLABEL(p, n) (p+sizeof(UKFS_DISKLABEL_SCANMAGIC)-1+n)
	if ((p = strstr(devpath, UKFS_DISKLABEL_SCANMAGIC)) != NULL
	    && strlen(p) == UKFS_DISKLABEL_MAGICLEN
	    && *(MAGICADJ_DISKLABEL(p,1)) == '%') {
		if (*(MAGICADJ_DISKLABEL(p,0)) >= 'a' &&
		    *(MAGICADJ_DISKLABEL(p,0)) < 'a' + UKFS_MAXPARTITIONS) {
			struct ukfs__disklabel dl;
			struct ukfs__partition *pp;
			int imswapped;
			char buf[65536];
			char labelchar = *(MAGICADJ_DISKLABEL(p,0));
			int partition = labelchar - 'a';
			uint32_t poffset, psize;

			*p = '\0';
			devfd = open(devpath, O_RDONLY);
			if (devfd == -1) {
				error = errno;
				goto out;
			}

			/* Locate the disklabel and find the partition. */
			if (pread(devfd, buf, sizeof(buf), 0) == -1) {
				error = errno;
				goto out;
			}

			if (ukfs__disklabel_scan(&dl, &imswapped,
			    buf, sizeof(buf)) != 0) {
				error = ENOENT;
				goto out;
			}

			if (dl.d_npartitions < partition) {
				error = ENOENT;
				goto out;
			}

			pp = &dl.d_partitions[partition];
			part->part_type = UKFS_PART_DISKLABEL;
			part->part_labelchar = labelchar;
			if (imswapped) {
				poffset = bswap32(pp->p_offset);
				psize = bswap32(pp->p_size);
			} else {
				poffset = pp->p_offset;
				psize = pp->p_size;
			}
			part->part_devoff = poffset << DEV_BSHIFT;
			part->part_devsize = psize << DEV_BSHIFT;
		} else {
			error = EINVAL;
		}
#define MAGICADJ_OFFSET(p, n) (p+sizeof(UKFS_OFFSET_SCANMAGIC)-1+n)
	} else if (((p = strstr(devpath, UKFS_OFFSET_SCANMAGIC)) != NULL)
	    && (strlen(p) >= UKFS_OFFSET_MINLEN)) {
		char *comma, *pers, *ep, *nptr;
		u_quad_t val;

		comma = strchr(p, ',');
		if (comma == NULL) {
			error = EINVAL;
			goto out;
		}
		pers = strchr(comma, '%');
		if (pers == NULL) {
			error = EINVAL;
			goto out;
		}
		*comma = '\0';
		*pers = '\0';
		*p = '\0';

		nptr = MAGICADJ_OFFSET(p,0);
		/* check if string is negative */
		if (*nptr == '-') {
			error = ERANGE;
			goto out;
		}
		val = strtouq(nptr, &ep, 10);
		if (val == UQUAD_MAX) {
			error = ERANGE;
			goto out;
		}
		if (*ep != '\0') {
			error = EADDRNOTAVAIL; /* creative ;) */
			goto out;
		}
		part->part_devoff = val;

		/* omstart */

		nptr = comma+1;
		/* check if string is negative */
		if (*nptr == '-') {
			error = ERANGE;
			goto out;
		}
		val = strtouq(nptr, &ep, 10);
		if (val == UQUAD_MAX) {
			error = ERANGE;
			goto out;
		}
		if (*ep != '\0') {
			error = EADDRNOTAVAIL; /* creative ;) */
			goto out;
		}
		part->part_devsize = val;
		part->part_type = UKFS_PART_OFFSET;
	} else {
		ukfs_part_release(part);
		part = ukfs_part_none;
	}

 out:
	if (devfd != -1)
		close(devfd);
	if (error) {
		free(part);
		errno = error;
	} else {
		*partp = part;
	}

	return error ? -1 : 0;
}

int
ukfs_part_tostring(struct ukfs_part *part, char *str, size_t strsize)
{
	int rv;

	*str = '\0';
	/* "pseudo" values */
	if (part == ukfs_part_na) {
		errno = EINVAL;
		return -1;
	}
	if (part == ukfs_part_none)
		return 0;

	rv = 0;
	switch (part->part_type) {
	case UKFS_PART_NONE:
		break;

	case UKFS_PART_DISKLABEL:
		snprintf(str, strsize, "%%DISKLABEL:%c%%",part->part_labelchar);
		rv = 1;
		break;

	case UKFS_PART_OFFSET:
		snprintf(str, strsize, "[%llu,%llu]",
		    (unsigned long long)part->part_devoff,
		    (unsigned long long)(part->part_devoff+part->part_devsize));
		rv = 1;
		break;
	}

	return rv;
}

static void
unlockdev(int fd, struct ukfs_part *part)
{
	struct flock flarg;

	if (part == ukfs_part_na)
		return;

	memset(&flarg, 0, sizeof(flarg));
	flarg.l_type = F_UNLCK;
	flarg.l_whence = SEEK_SET;
	flarg.l_start = part->part_devoff;
	flarg.l_len = PART2LOCKSIZE(part->part_devsize);
	if (fcntl(fd, F_SETLK, &flarg) == -1)
		warn("ukfs: cannot unlock device file");
}

/*
 * Open the disk file and flock it.  Also, if we are operation on
 * an embedded partition, find the partition offset and size from
 * the disklabel.
 *
 * We hard-fail only in two cases:
 *  1) we failed to get the partition info out (don't know what offset
 *     to mount from)
 *  2) we failed to flock the source device (i.e. fcntl() fails,
 *     not e.g. open() before it)
 *
 * Otherwise we let the code proceed to mount and let the file system
 * throw the proper error.  The only questionable bit is that if we
 * soft-fail before flock and mount does succeed...
 *
 * Returns: -1 error (errno reports error code)
 *           0 success
 *
 * dfdp: -1  device is not open
 *        n  device is open
 */
static int
process_diskdevice(const char *devpath, struct ukfs_part *part, int rdonly,
	int *dfdp)
{
	struct stat sb;
	int rv = 0, devfd;

	/* defaults */
	*dfdp = -1;

	devfd = open(devpath, rdonly ? O_RDONLY : O_RDWR);
	if (devfd == -1) {
		rv = errno;
		goto out;
	}

	if (fstat(devfd, &sb) == -1) {
		rv = errno;
		goto out;
	}

	/*
	 * We do this only for non-block device since the
	 * (NetBSD) kernel allows block device open only once.
	 * We also need to close the device for fairly obvious reasons.
	 */
	if (!S_ISBLK(sb.st_mode)) {
		struct flock flarg;

		memset(&flarg, 0, sizeof(flarg));
		flarg.l_type = rdonly ? F_RDLCK : F_WRLCK;
		flarg.l_whence = SEEK_SET;
		flarg.l_start = part->part_devoff;
		flarg.l_len = PART2LOCKSIZE(part->part_devsize);
		if (fcntl(devfd, F_SETLK, &flarg) == -1) {
			pid_t holder;
			int sverrno;

			sverrno = errno;
			if (fcntl(devfd, F_GETLK, &flarg) != 1)
				holder = flarg.l_pid;
			else
				holder = -1;
			warnx("ukfs_mount: cannot lock device.  held by pid %d",
			    holder);
			rv = sverrno;
			goto out;
		}
	} else {
		close(devfd);
		devfd = -1;
	}
	*dfdp = devfd;

 out:
	if (rv) {
		if (devfd != -1)
			close(devfd);
	}

	return rv;
}

struct mountinfo {
	const char *mi_vfsname;
	const char *mi_mountpath;
	int mi_mntflags;
	void *mi_arg;
	size_t mi_alen;
	int *mi_error;
};
static void *
mfs_mounter(void *arg)
{
	struct mountinfo *mi = arg;
	int rv;

	rv = rump_sys_mount(mi->mi_vfsname, mi->mi_mountpath, mi->mi_mntflags,
	    mi->mi_arg, mi->mi_alen);
	if (rv) {
		warn("mfs mount failed.  fix me.");
		abort(); /* XXX */
	}

	return NULL;
}

static struct ukfs *
doukfsmount(const char *vfsname, const char *devpath, struct ukfs_part *part,
	const char *mountpath, int mntflags, void *arg, size_t alen)
{
	struct ukfs *fs = NULL;
	struct lwp *curlwp;
	int rv = 0, devfd = -1;
	int mounted = 0;
	int regged = 0;

	pthread_spin_lock(&part->part_lck);
	part->part_refcount++;
	pthread_spin_unlock(&part->part_lck);
	if (part != ukfs_part_na) {
		if ((rv = process_diskdevice(devpath, part,
		    mntflags & MNT_RDONLY, &devfd)) != 0)
			goto out;
	}

	fs = malloc(sizeof(struct ukfs));
	if (fs == NULL) {
		rv = ENOMEM;
		goto out;
	}
	memset(fs, 0, sizeof(struct ukfs));

	/* create our mountpoint.  this is never removed. */
	if (builddirs(mountpath, 0777, rumpmkdir, NULL) == -1) {
		if (errno != EEXIST) {
			rv = errno;
			goto out;
		}
	}

	if (part != ukfs_part_na) {
		/* LINTED */
		rv = rump_pub_etfs_register_withsize(devpath, devpath,
		    RUMP_ETFS_BLK, part->part_devoff, part->part_devsize);
		if (rv) {
			goto out;
		}
		regged = 1;
	}

	/*
	 * MFS is special since mount(2) doesn't return.  Hence, we
	 * create a thread here.  Could fix mfs to return, but there's
	 * too much history for me to bother.
	 */
	if (strcmp(vfsname, MOUNT_MFS) == 0) {
#if !defined(__minix)
		pthread_t pt;
#endif
		struct mountinfo mi;
		int i;

		mi.mi_vfsname = vfsname;
		mi.mi_mountpath = mountpath;
		mi.mi_mntflags = mntflags;
		mi.mi_arg = arg;
		mi.mi_alen = alen;

#if defined(__minix)
		/* No support for threads. */
		rv = ENOTSUP;
		goto out;
#else
		if (pthread_create(&pt, NULL, mfs_mounter, &mi) == -1) {
			rv = errno;
			goto out;
		}
#endif /* defined(__minix) */

		for (i = 0;i < 100000; i++) {
			struct statvfs svfsb;

			rv = rump_sys_statvfs1(mountpath, &svfsb, ST_WAIT);
			if (rv == -1) {
				rv = errno;
				goto out;
			}

			if (strcmp(svfsb.f_mntonname, mountpath) == 0 && 
			    strcmp(svfsb.f_fstypename, MOUNT_MFS) == 0) {
				break;
			}
			usleep(1);
		}
	} else {
		rv = rump_sys_mount(vfsname, mountpath, mntflags, arg, alen);
		if (rv) {
			rv = errno;
			goto out;
		}
	}

	mounted = 1;
	rv = rump_pub_vfs_getmp(mountpath, &fs->ukfs_mp);
	if (rv) {
		goto out;
	}

	if (regged) {
		fs->ukfs_devpath = strdup(devpath);
	}
	fs->ukfs_mountpath = strdup(mountpath);
	pthread_spin_init(&fs->ukfs_spin, PTHREAD_PROCESS_SHARED);
	fs->ukfs_devfd = devfd;
	fs->ukfs_part = part;
	assert(rv == 0);

	curlwp = rump_pub_lwproc_curlwp();
	rump_pub_lwproc_newlwp(0);
	fs->ukfs_lwp = rump_pub_lwproc_curlwp();
	fs->ukfs_cwd = strdup("/");
	rump_pub_lwproc_switch(curlwp);

 out:
	if (rv) {
		if (fs) {
			free(fs);
			fs = NULL;
		}
		if (mounted)
			rump_sys_unmount(mountpath, MNT_FORCE);
		if (regged)
			rump_pub_etfs_remove(devpath);
		if (devfd != -1) {
			unlockdev(devfd, part);
			close(devfd);
		}
		ukfs_part_release(part);
		errno = rv;
	}

	return fs;
}

struct ukfs *
ukfs_mount(const char *vfsname, const char *devpath,
	const char *mountpath, int mntflags, void *arg, size_t alen)
{

	return doukfsmount(vfsname, devpath, ukfs_part_na,
	    mountpath, mntflags, arg, alen);
}

struct ukfs *
ukfs_mount_disk(const char *vfsname, const char *devpath,
	struct ukfs_part *part, const char *mountpath, int mntflags,
	void *arg, size_t alen)
{

	return doukfsmount(vfsname, devpath, part,
	    mountpath, mntflags, arg, alen);
}

int
ukfs_release(struct ukfs *fs, int flags)
{
	struct lwp *curlwp = rump_pub_lwproc_curlwp();

	/* get root lwp */
	rump_pub_lwproc_switch(fs->ukfs_lwp);
	rump_pub_lwproc_rfork(RUMP_RFCFDG);

	if ((flags & UKFS_RELFLAG_NOUNMOUNT) == 0) {
		int rv, mntflag, error;

		mntflag = 0;
		if (flags & UKFS_RELFLAG_FORCE)
			mntflag = MNT_FORCE;

		rv = rump_sys_unmount(fs->ukfs_mountpath, mntflag);
		if (rv == -1) {
			error = errno;
			rump_pub_lwproc_releaselwp();
			if (curlwp)
				rump_pub_lwproc_switch(curlwp);
			errno = error;
			return -1;
		}
	}

	if (fs->ukfs_devpath) {
		rump_pub_etfs_remove(fs->ukfs_devpath);
		free(fs->ukfs_devpath);
	}
	free(fs->ukfs_mountpath);
	free(fs->ukfs_cwd);

	/* release this routine's lwp and ukfs base lwp */
	rump_pub_lwproc_releaselwp();
	rump_pub_lwproc_switch(fs->ukfs_lwp);
	rump_pub_lwproc_releaselwp();

	pthread_spin_destroy(&fs->ukfs_spin);
	if (fs->ukfs_devfd != -1) {
		unlockdev(fs->ukfs_devfd, fs->ukfs_part);
		close(fs->ukfs_devfd);
	}
	ukfs_part_release(fs->ukfs_part);
	free(fs);

	if (curlwp)
		rump_pub_lwproc_switch(curlwp);

	return 0;
}

void
ukfs_part_release(struct ukfs_part *part)
{
	int release;

	if (part != ukfs_part_none && part != ukfs_part_na) {
		pthread_spin_lock(&part->part_lck);
		release = --part->part_refcount == 0;
		pthread_spin_unlock(&part->part_lck);
		if (release) {
			pthread_spin_destroy(&part->part_lck);
			free(part);
		}
	}
}

#define STDCALL(ukfs, thecall)						\
	int rv = 0;							\
									\
	PRECALL();							\
	rv = thecall;							\
	POSTCALL();							\
	return rv;

int
ukfs_opendir(struct ukfs *ukfs, const char *dirname, struct ukfs_dircookie **c)
{
	struct vnode *vp;
	int rv;

	PRECALL();
	rv = rump_pub_namei(RUMP_NAMEI_LOOKUP, RUMP_NAMEI_LOCKLEAF, dirname,
	    NULL, &vp, NULL);
	POSTCALL();

	if (rv == 0) {
		RUMP_VOP_UNLOCK(vp);
	} else {
		errno = rv;
		rv = -1;
	}

	/*LINTED*/
	*c = (struct ukfs_dircookie *)vp;
	return rv;
}

static int
getmydents(struct vnode *vp, off_t *off, uint8_t *buf, size_t bufsize)
{
	struct uio *uio;
	size_t resid;
	int rv, eofflag;
	struct kauth_cred *cred;
	
	uio = rump_pub_uio_setup(buf, bufsize, *off, RUMPUIO_READ);
	cred = rump_pub_cred_create(0, 0, 0, NULL);
	rv = RUMP_VOP_READDIR(vp, uio, cred, &eofflag, NULL, NULL);
	rump_pub_cred_put(cred);
	RUMP_VOP_UNLOCK(vp);
	*off = rump_pub_uio_getoff(uio);
	resid = rump_pub_uio_free(uio);

	if (rv) {
		errno = rv;
		return -1;
	}

	/* LINTED: not totally correct return type, but follows syscall */
	return bufsize - resid;
}

/*ARGSUSED*/
int
ukfs_getdents_cookie(struct ukfs *ukfs, struct ukfs_dircookie *c, off_t *off,
	uint8_t *buf, size_t bufsize)
{
	/*LINTED*/
	struct vnode *vp = (struct vnode *)c;

	RUMP_VOP_LOCK(vp, RUMP_LK_SHARED);
	return getmydents(vp, off, buf, bufsize);
}

int
ukfs_getdents(struct ukfs *ukfs, const char *dirname, off_t *off,
	uint8_t *buf, size_t bufsize)
{
	struct vnode *vp;
	int rv;

	PRECALL();
	rv = rump_pub_namei(RUMP_NAMEI_LOOKUP, RUMP_NAMEI_LOCKLEAF, dirname,
	    NULL, &vp, NULL);
	if (rv) {
		POSTCALL();
		errno = rv;
		return -1;
	}

	rv = getmydents(vp, off, buf, bufsize);
	rump_pub_vp_rele(vp);
	POSTCALL();
	return rv;
}

/*ARGSUSED*/
int
ukfs_closedir(struct ukfs *ukfs, struct ukfs_dircookie *c)
{

	/*LINTED*/
	rump_pub_vp_rele((struct vnode *)c);
	return 0;
}

int
ukfs_open(struct ukfs *ukfs, const char *filename, int flags)
{
	int fd;

	PRECALL();
	fd = rump_sys_open(filename, flags, 0);
	POSTCALL();
	if (fd == -1)
		return -1;

	return fd;
}

ssize_t
ukfs_read(struct ukfs *ukfs, const char *filename, off_t off,
	uint8_t *buf, size_t bufsize)
{
	int fd;
	ssize_t xfer = -1; /* XXXgcc */

	PRECALL();
	fd = rump_sys_open(filename, RUMP_O_RDONLY, 0);
	if (fd == -1)
		goto out;

	xfer = rump_sys_pread(fd, buf, bufsize, off);
	rump_sys_close(fd);

 out:
	POSTCALL();
	if (fd == -1) {
		return -1;
	}
	return xfer;
}

/*ARGSUSED*/
ssize_t
ukfs_read_fd(struct ukfs *ukfs, int fd, off_t off, uint8_t *buf, size_t buflen)
{

	return rump_sys_pread(fd, buf, buflen, off);
}

ssize_t
ukfs_write(struct ukfs *ukfs, const char *filename, off_t off,
	uint8_t *buf, size_t bufsize)
{
	int fd;
	ssize_t xfer = -1; /* XXXgcc */

	PRECALL();
	fd = rump_sys_open(filename, RUMP_O_WRONLY, 0);
	if (fd == -1)
		goto out;

	/* write and commit */
	xfer = rump_sys_pwrite(fd, buf, bufsize, off);
	if (xfer > 0)
		rump_sys_fsync(fd);

	rump_sys_close(fd);

 out:
	POSTCALL();
	if (fd == -1) {
		return -1;
	}
	return xfer;
}

/*ARGSUSED*/
ssize_t
ukfs_write_fd(struct ukfs *ukfs, int fd, off_t off, uint8_t *buf, size_t buflen,
	int dosync)
{
	ssize_t xfer;

	xfer = rump_sys_pwrite(fd, buf, buflen, off);
	if (xfer > 0 && dosync)
		rump_sys_fsync(fd);

	return xfer;
}

/*ARGSUSED*/
int
ukfs_close(struct ukfs *ukfs, int fd)
{

	rump_sys_close(fd);
	return 0;
}

int
ukfs_create(struct ukfs *ukfs, const char *filename, mode_t mode)
{
	int fd;

	PRECALL();
	fd = rump_sys_open(filename, RUMP_O_WRONLY | RUMP_O_CREAT, mode);
	if (fd == -1)
		return -1;
	rump_sys_close(fd);

	POSTCALL();
	return 0;
}

int
ukfs_mknod(struct ukfs *ukfs, const char *path, mode_t mode, dev_t dev)
{

	STDCALL(ukfs, rump_sys_mknod(path, mode, dev));
}

int
ukfs_mkfifo(struct ukfs *ukfs, const char *path, mode_t mode)
{

	STDCALL(ukfs, rump_sys_mkfifo(path, mode));
}

int
ukfs_mkdir(struct ukfs *ukfs, const char *filename, mode_t mode)
{

	STDCALL(ukfs, rump_sys_mkdir(filename, mode));
}

int
ukfs_remove(struct ukfs *ukfs, const char *filename)
{

	STDCALL(ukfs, rump_sys_unlink(filename));
}

int
ukfs_rmdir(struct ukfs *ukfs, const char *filename)
{

	STDCALL(ukfs, rump_sys_rmdir(filename));
}

int
ukfs_link(struct ukfs *ukfs, const char *filename, const char *f_create)
{

	STDCALL(ukfs, rump_sys_link(filename, f_create));
}

int
ukfs_symlink(struct ukfs *ukfs, const char *filename, const char *linkname)
{

	STDCALL(ukfs, rump_sys_symlink(filename, linkname));
}

ssize_t
ukfs_readlink(struct ukfs *ukfs, const char *filename,
	char *linkbuf, size_t buflen)
{
	ssize_t rv;

	PRECALL();
	rv = rump_sys_readlink(filename, linkbuf, buflen);
	POSTCALL();
	return rv;
}

int
ukfs_rename(struct ukfs *ukfs, const char *from, const char *to)
{

	STDCALL(ukfs, rump_sys_rename(from, to));
}

int
ukfs_chdir(struct ukfs *ukfs, const char *path)
{
	char *newpath, *oldpath;
	int rv;

	PRECALL();
	rv = rump_sys_chdir(path);
	if (rv == -1)
		goto out;

	newpath = malloc(MAXPATHLEN);
	if (rump_sys___getcwd(newpath, MAXPATHLEN) == -1) {
		goto out;
	}

	pthread_spin_lock(&ukfs->ukfs_spin);
	oldpath = ukfs->ukfs_cwd;
	ukfs->ukfs_cwd = newpath;
	pthread_spin_unlock(&ukfs->ukfs_spin);
	free(oldpath);

 out:
	POSTCALL();
	return rv;
}

int
ukfs_stat(struct ukfs *ukfs, const char *filename, struct stat *file_stat)
{
	int rv;

	PRECALL();
	rv = rump_sys_stat(filename, file_stat);
	POSTCALL();

	return rv;
}

int
ukfs_lstat(struct ukfs *ukfs, const char *filename, struct stat *file_stat)
{
	int rv;

	PRECALL();
	rv = rump_sys_lstat(filename, file_stat);
	POSTCALL();

	return rv;
}

int
ukfs_chmod(struct ukfs *ukfs, const char *filename, mode_t mode)
{

	STDCALL(ukfs, rump_sys_chmod(filename, mode));
}

int
ukfs_lchmod(struct ukfs *ukfs, const char *filename, mode_t mode)
{

	STDCALL(ukfs, rump_sys_lchmod(filename, mode));
}

int
ukfs_chown(struct ukfs *ukfs, const char *filename, uid_t uid, gid_t gid)
{

	STDCALL(ukfs, rump_sys_chown(filename, uid, gid));
}

int
ukfs_lchown(struct ukfs *ukfs, const char *filename, uid_t uid, gid_t gid)
{

	STDCALL(ukfs, rump_sys_lchown(filename, uid, gid));
}

int
ukfs_chflags(struct ukfs *ukfs, const char *filename, u_long flags)
{

	STDCALL(ukfs, rump_sys_chflags(filename, flags));
}

int
ukfs_lchflags(struct ukfs *ukfs, const char *filename, u_long flags)
{

	STDCALL(ukfs, rump_sys_lchflags(filename, flags));
}

int
ukfs_utimes(struct ukfs *ukfs, const char *filename, const struct timeval *tptr)
{

	STDCALL(ukfs, rump_sys_utimes(filename, tptr));
}

int
ukfs_lutimes(struct ukfs *ukfs, const char *filename, 
	      const struct timeval *tptr)
{

	STDCALL(ukfs, rump_sys_lutimes(filename, tptr));
}

/*
 * Dynamic module support
 */

/* load one library */

/*
 * XXX: the dlerror stuff isn't really threadsafe, but then again I
 * can't protect against other threads calling dl*() outside of ukfs,
 * so just live with it being flimsy
 */
int
ukfs_modload(const char *fname)
{
	void *handle;
	const struct modinfo *const *mi_start, *const *mi_end;
	int error;

	handle = dlopen(fname, RTLD_LAZY|RTLD_GLOBAL);
	if (handle == NULL) {
		const char *dlmsg = dlerror();
		if (strstr(dlmsg, "Undefined symbol"))
			return 0;
		warnx("dlopen %s failed: %s", fname, dlmsg);
		/* XXXerrno */
		return -1;
	}

	mi_start = dlsym(handle, "__start_link_set_modules");
	mi_end = dlsym(handle, "__stop_link_set_modules");
	if (mi_start && mi_end) {
		error = rump_pub_module_init(mi_start,
		    (size_t)(mi_end-mi_start));
		if (error)
			goto errclose;
		return 1;
	}
	error = EINVAL;

 errclose:
	dlclose(handle);
	errno = error;
	return -1;
}

struct loadfail {
	char *pname;

	LIST_ENTRY(loadfail) entries;
};

#define RUMPFSMOD_PREFIX "librumpfs_"
#define RUMPFSMOD_SUFFIX ".so"

int
ukfs_modload_dir(const char *dir)
{
	char nbuf[MAXPATHLEN+1], *p;
	struct dirent entry, *result;
	DIR *libdir;
	struct loadfail *lf, *nlf;
	int error, nloaded = 0, redo;
	LIST_HEAD(, loadfail) lfs;

	libdir = opendir(dir);
	if (libdir == NULL)
		return -1;

	LIST_INIT(&lfs);
	for (;;) {
		if ((error = readdir_r(libdir, &entry, &result)) != 0)
			break;
		if (!result)
			break;
		if (strncmp(result->d_name, RUMPFSMOD_PREFIX,
		    strlen(RUMPFSMOD_PREFIX)) != 0)
			continue;
		if (((p = strstr(result->d_name, RUMPFSMOD_SUFFIX)) == NULL)
		    || strlen(p) != strlen(RUMPFSMOD_SUFFIX))
			continue;
		strlcpy(nbuf, dir, sizeof(nbuf));
		strlcat(nbuf, "/", sizeof(nbuf));
		strlcat(nbuf, result->d_name, sizeof(nbuf));
		switch (ukfs_modload(nbuf)) {
		case 0:
			lf = malloc(sizeof(*lf));
			if (lf == NULL) {
				error = ENOMEM;
				break;
			}
			lf->pname = strdup(nbuf);
			if (lf->pname == NULL) {
				free(lf);
				error = ENOMEM;
				break;
			}
			LIST_INSERT_HEAD(&lfs, lf, entries);
			break;
		case 1:
			nloaded++;
			break;
		default:
			/* ignore errors */
			break;
		}
	}
	closedir(libdir);
	if (error && nloaded != 0)
		error = 0;

	/*
	 * El-cheapo dependency calculator.  Just try to load the
	 * modules n times in a loop
	 */
	for (redo = 1; redo;) {
		redo = 0;
		nlf = LIST_FIRST(&lfs);
		while ((lf = nlf) != NULL) {
			nlf = LIST_NEXT(lf, entries);
			if (ukfs_modload(lf->pname) == 1) {
				nloaded++;
				redo = 1;
				LIST_REMOVE(lf, entries);
				free(lf->pname);
				free(lf);
			}
		}
	}

	while ((lf = LIST_FIRST(&lfs)) != NULL) {
		LIST_REMOVE(lf, entries);
		free(lf->pname);
		free(lf);
	}

	if (error && nloaded == 0) {
		errno = error;
		return -1;
	}

	return nloaded;
}

/* XXX: this code uses definitions from NetBSD, needs rumpdefs */
ssize_t
ukfs_vfstypes(char *buf, size_t buflen)
{
	int mib[3];
	struct sysctlnode q, ans[128];
	size_t alen;
	int i;

	mib[0] = CTL_VFS;
	mib[1] = VFS_GENERIC;
	mib[2] = CTL_QUERY;
	alen = sizeof(ans);

	memset(&q, 0, sizeof(q));
	q.sysctl_flags = SYSCTL_VERSION;

	if (rump_sys___sysctl(mib, 3, ans, &alen, &q, sizeof(q)) == -1) {
		return -1;
	}

	for (i = 0; i < alen/sizeof(ans[0]); i++)
		if (strcmp("fstypes", ans[i].sysctl_name) == 0)
			break;
	if (i == alen/sizeof(ans[0])) {
		errno = ENXIO;
		return -1;
	}

	mib[0] = CTL_VFS;
	mib[1] = VFS_GENERIC;
	mib[2] = ans[i].sysctl_num;

	if (rump_sys___sysctl(mib, 3, buf, &buflen, NULL, 0) == -1) {
		return -1;
	}

	return buflen;
}

/*
 * Utilities
 */
static int
builddirs(const char *pathname, mode_t mode,
	int (*mkdirfn)(struct ukfs *, const char *, mode_t), struct ukfs *fs)
{
	char *f1, *f2;
	int rv;
	mode_t mask;
	bool end;

	/*ukfs_umask((mask = ukfs_umask(0)));*/
	umask((mask = umask(0)));

	f1 = f2 = strdup(pathname);
	if (f1 == NULL) {
		errno = ENOMEM;
		return -1;
	}

	end = false;
	for (;;) {
		/* find next component */
		f2 += strspn(f2, "/");
		f2 += strcspn(f2, "/");
		if (*f2 == '\0')
			end = true;
		else
			*f2 = '\0';

		rv = mkdirfn(fs, f1, mode & ~mask); 
		if (errno == EEXIST)
			rv = 0;

		if (rv == -1 || *f2 != '\0' || end)
			break;

		*f2 = '/';
	}

	free(f1);

	return rv;
}

int
ukfs_util_builddirs(struct ukfs *ukfs, const char *pathname, mode_t mode)
{

	return builddirs(pathname, mode, ukfs_mkdir, ukfs);
}
