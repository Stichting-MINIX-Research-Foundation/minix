/*	$NetBSD: rumpuser_file.c,v 1.4 2014/11/04 21:08:12 pooka Exp $	*/

/*
 * Copyright (c) 2007-2010 Antti Kantee.  All Rights Reserved.
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

/* NOTE this code will move to a new driver in the next hypercall revision */

#include "rumpuser_port.h"

#if !defined(lint)
__RCSID("$NetBSD: rumpuser_file.c,v 1.4 2014/11/04 21:08:12 pooka Exp $");
#endif /* !lint */

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

#if defined(HAVE_SYS_DISK_H)
#include <sys/disk.h>
#endif
#if defined(HAVE_SYS_DISKLABEL_H)
#include <sys/disklabel.h>
#endif
#if defined(HAVE_SYS_DKIO_H)
#include <sys/dkio.h>
#endif

#if defined(HAVE_SYS_SYSCTL_H)
#include <sys/sysctl.h>
#endif

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <rump/rumpuser.h>

#include "rumpuser_int.h"

int
rumpuser_getfileinfo(const char *path, uint64_t *sizep, int *ftp)
{
	struct stat sb;
	uint64_t size = 0;
	int needsdev = 0, rv = 0, ft = 0;
	int fd = -1;

	if (stat(path, &sb) == -1) {
		rv = errno;
		goto out;
	}

	switch (sb.st_mode & S_IFMT) {
	case S_IFDIR:
		ft = RUMPUSER_FT_DIR;
		break;
	case S_IFREG:
		ft = RUMPUSER_FT_REG;
		break;
	case S_IFBLK:
		ft = RUMPUSER_FT_BLK;
		needsdev = 1;
		break;
	case S_IFCHR:
		ft = RUMPUSER_FT_CHR;
		needsdev = 1;
		break;
	default:
		ft = RUMPUSER_FT_OTHER;
		break;
	}

	if (!needsdev) {
		size = sb.st_size;
	} else if (sizep) {
		/*
		 * Welcome to the jungle.  Of course querying the kernel
		 * for a device partition size is supposed to be far from
		 * trivial.  On NetBSD we use ioctl.  On $other platform
		 * we have a problem.  We try "the lseek trick" and just
		 * fail if that fails.  Platform specific code can later
		 * be written here if appropriate.
		 *
		 * On NetBSD we hope and pray that for block devices nobody
		 * else is holding them open, because otherwise the kernel
		 * will not permit us to open it.  Thankfully, this is
		 * usually called only in bootstrap and then we can
		 * forget about it.
		 */

		fd = open(path, O_RDONLY);
		if (fd == -1) {
			rv = errno;
			goto out;
		}

#if (!defined(DIOCGDINFO) || !defined(DISKPART)) && !defined(DIOCGWEDGEINFO)
		{
		off_t off = lseek(fd, 0, SEEK_END);
		if (off != 0) {
			size = off;
			goto out;
		}
		fprintf(stderr, "error: device size query not implemented on "
		    "this platform\n");
		rv = EOPNOTSUPP;
		goto out;
		}
#else

#if defined(DIOCGDINFO) && defined(DISKPART)
		{
		struct disklabel lab;
		struct partition *parta;
		if (ioctl(fd, DIOCGDINFO, &lab) == 0) {
			parta = &lab.d_partitions[DISKPART(sb.st_rdev)];
			size = (uint64_t)lab.d_secsize * parta->p_size;
			goto out;
		}
		}
#endif

#if defined(DIOCGWEDGEINFO)
		{
		struct dkwedge_info dkw;
		if (ioctl(fd, DIOCGWEDGEINFO, &dkw) == 0) {
			/*
			 * XXX: should use DIOCGDISKINFO to query
			 * sector size, but that requires proplib,
			 * so just don't bother for now.  it's nice
			 * that something as difficult as figuring out
			 * a partition's size has been made so easy.
			 */
			size = dkw.dkw_size << DEV_BSHIFT;
			goto out;
		}
		}
#endif
		rv = errno;
#endif
	}

 out:
	if (rv == 0 && sizep)
		*sizep = size;
	if (rv == 0 && ftp)
		*ftp = ft;
	if (fd != -1)
		close(fd);

	ET(rv);
}

int
rumpuser_open(const char *path, int ruflags, int *fdp)
{
	int fd, flags, rv;

	switch (ruflags & RUMPUSER_OPEN_ACCMODE) {
	case RUMPUSER_OPEN_RDONLY:
		flags = O_RDONLY;
		break;
	case RUMPUSER_OPEN_WRONLY:
		flags = O_WRONLY;
		break;
	case RUMPUSER_OPEN_RDWR:
		flags = O_RDWR;
		break;
	default:
		rv = EINVAL;
		goto out;
	}

#define TESTSET(_ru_, _h_) if (ruflags & _ru_) flags |= _h_;
	TESTSET(RUMPUSER_OPEN_CREATE, O_CREAT);
	TESTSET(RUMPUSER_OPEN_EXCL, O_EXCL);
#undef TESTSET

	KLOCK_WRAP(fd = open(path, flags, 0644));
	if (fd == -1) {
		rv = errno;
	} else {
		*fdp = fd;
		rv = 0;
	}

 out:
	ET(rv);
}

int
rumpuser_close(int fd)
{
	int nlocks;

	rumpkern_unsched(&nlocks, NULL);
	fsync(fd);
	close(fd);
	rumpkern_sched(nlocks, NULL);

	ET(0);
}

/*
 * Assume "struct rumpuser_iovec" and "struct iovec" are the same.
 * If you encounter POSIX platforms where they aren't, add some
 * translation for iovlen > 1.
 */
int
rumpuser_iovread(int fd, struct rumpuser_iovec *ruiov, size_t iovlen,
	int64_t roff, size_t *retp)
{
	struct iovec *iov = (struct iovec *)ruiov;
	off_t off = (off_t)roff;
	ssize_t nn;
	int rv;

	if (off == RUMPUSER_IOV_NOSEEK) {
		KLOCK_WRAP(nn = readv(fd, iov, iovlen));
	} else {
		int nlocks;

		rumpkern_unsched(&nlocks, NULL);
		if (lseek(fd, off, SEEK_SET) == off) {
			nn = readv(fd, iov, iovlen);
		} else {
			nn = -1;
		}
		rumpkern_sched(nlocks, NULL);
	}

	if (nn == -1) {
		rv = errno;
	} else {
		*retp = (size_t)nn;
		rv = 0;
	}

	ET(rv);
}

int
rumpuser_iovwrite(int fd, const struct rumpuser_iovec *ruiov, size_t iovlen,
	int64_t roff, size_t *retp)
{
	const struct iovec *iov = (const struct iovec *)ruiov;
	off_t off = (off_t)roff;
	ssize_t nn;
	int rv;

	if (off == RUMPUSER_IOV_NOSEEK) {
		KLOCK_WRAP(nn = writev(fd, iov, iovlen));
	} else {
		int nlocks;

		rumpkern_unsched(&nlocks, NULL);
		if (lseek(fd, off, SEEK_SET) == off) {
			nn = writev(fd, iov, iovlen);
		} else {
			nn = -1;
		}
		rumpkern_sched(nlocks, NULL);
	}

	if (nn == -1) {
		rv = errno;
	} else {
		*retp = (size_t)nn;
		rv = 0;
	}

	ET(rv);
}

int
rumpuser_syncfd(int fd, int flags, uint64_t start, uint64_t len)
{
	int rv = 0;
	
	/*
	 * For now, assume fd is regular file and does not care
	 * about read syncing
	 */
	if ((flags & RUMPUSER_SYNCFD_BOTH) == 0) {
		rv = EINVAL;
		goto out;
	}
	if ((flags & RUMPUSER_SYNCFD_WRITE) == 0) {
		rv = 0;
		goto out;
	}

#if defined(HAVE_FSYNC_RANGE)
	{
	int fsflags = FDATASYNC;

	if (fsflags & RUMPUSER_SYNCFD_SYNC)
		fsflags |= FDISKSYNC;
	if (fsync_range(fd, fsflags, start, len) == -1)
		rv = errno;
	}
#else
	/* el-simplo */
	if (fsync(fd) == -1)
		rv = errno;
#endif

 out:
	ET(rv);
}
