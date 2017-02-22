/*	$NetBSD: vnconfig.c,v 1.42 2014/05/23 20:50:16 dholland Exp $	*/

/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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

/*
 * Copyright (c) 1993 University of Utah.
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 * from: Utah $Hdr: vnconfig.c 1.1 93/12/15$
 *
 *	@(#)vnconfig.c	8.1 (Berkeley) 12/15/93
 */

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#if !defined(__minix)
#include <sys/buf.h>
#include <sys/disklabel.h>
#include <sys/disk.h>
#include <sys/bitops.h>
#else
#include <minix/paths.h>
#include <sys/wait.h>
#endif /* !defined(__minix) */

#include <dev/vndvar.h>

#include <disktab.h>
#include <err.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <util.h>
#include <paths.h>

#define VND_CONFIG	1
#define VND_UNCONFIG	2
#define VND_GET		3

static int	verbose = 0;
static int	readonly = 0;
static int	force = 0;
static int	compressed = 0;
static char	*tabname;
#if defined(__minix)
static int	service = 1;
#endif /* defined(__minix) */

#if !defined(__minix)
static void	show(int, int);
#else
static void	show(const char *, int);
#endif /* !defined(__minix) */
static int	config(char *, char *, char *, int);
static int	getgeom(struct vndgeom *, char *);
__dead static void	usage(void);

#if defined(__minix)
/*
 * Start a driver instance for the given vnd name.  The return value indicates
 * whether the instance has been started successfully.
 */
static int
start_service(char *dev)
{
	char *p, *endp, cmd[PATH_MAX];
	int n, status;

	p = strrchr(dev, '/');
	if (p == NULL) p = dev;
	else p++;

	/*
	 * There are two alternatives to get the instance number for the
	 * driver: either we scan the given device name, or we obtain its major
	 * number.  We choose to scan the name, because major numbers are more
	 * likely to change in the future.
	 */
	if (strncmp(p, "vnd", 3) != 0)
		return 0;
	n = strtoul(p + 3, &endp, 10);
	if (endp[0])
		return 0;

	if (verbose)
		printf("%s: starting driver\n", dev);

	snprintf(cmd, sizeof(cmd),
	    "%s up %s/vnd -label vnd%u -args instance=%u -dev %s",
	    _PATH_MINIX_SERVICE, _PATH_DRIVERS, n, n, dev);

	status = system(cmd);

	if (!WIFEXITED(status))
		return 0;
	return !WEXITSTATUS(status);
}

/*
 * Stop the driver instance responsible for the given file descriptor.
 * The file descriptor is closed upon return.
 */
static void
stop_service(int fd, char *dev)
{
	char cmd[PATH_MAX];
	struct vnd_user vnu;
	int openct, stop = 0;

	/* Only shut down the driver if the device is opened once, by us. */
	if (ioctl(fd, DIOCOPENCT, &openct) == 0 && openct == 1) {
		/* We let the driver tell us what instance number it has. */
		if (ioctl(fd, VNDIOCGET, &vnu) == 0)
			stop = 1;
	}

	/* Close the file descriptor before shutting down the driver! */
	(void) close(fd);

	if (stop) {
		if (verbose)
			printf("%s: stopping driver\n", dev);

		snprintf(cmd, sizeof(cmd), "%s down vnd%u",
		    _PATH_MINIX_SERVICE, vnu.vnu_unit);

		system(cmd);
	}
}
#endif /* defined(__minix) */

int
main(int argc, char *argv[])
{
	int ch, rv, action = VND_CONFIG;

#if !defined(__minix)
	while ((ch = getopt(argc, argv, "Fcf:lrt:uvz")) != -1) {
#else
	/* MINIX3: added -S; no support for -f, -t, -z at this time. */
	while ((ch = getopt(argc, argv, "SFclruv")) != -1) {
#endif /* !defined(__minix) */
		switch (ch) {
#if defined(__minix)
		case 'S':
			service = 0;
			break;
#endif /* defined(__minix) */
		case 'F':
			force = 1;
			break;
		case 'c':
			action = VND_CONFIG;
			break;
		case 'f':
#if !defined(__minix)
			if (setdisktab(optarg) == -1)
				usage();
#endif /* !defined(__minix) */
			break;
		case 'l':
			action = VND_GET;
			break;
		case 'r':
			readonly = 1;
			break;
		case 't':
			tabname = optarg;
			break;
		case 'u':
			action = VND_UNCONFIG;
			break;
		case 'v':
			verbose = 1;
			break;
		case 'z':
			compressed = 1;
			readonly = 1;
			break;
		default:
		case '?':
			usage();
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	if (action == VND_CONFIG) {
		if ((argc < 2 || argc > 3) ||
		    (argc == 3 && tabname != NULL))
			usage();
		rv = config(argv[0], argv[1], (argc == 3) ? argv[2] : NULL,
		    action);
	} else if (action == VND_UNCONFIG) {
		if (argc != 1 || tabname != NULL)
			usage();
		rv = config(argv[0], NULL, NULL, action);
	} else { /* VND_GET */
#if !defined(__minix)
		int n, v;
		const char *vn;
		char path[64];
#else
		int n;
#endif /* !defined(__minix) */

		if (argc != 0 && argc != 1)
			usage();

#if !defined(__minix)
		vn = argc ? argv[0] : "vnd0";

		v = opendisk(vn, O_RDONLY, path, sizeof(path), 0);
		if (v == -1)
			err(1, "open: %s", vn);
#endif /* !defined(__minix) */

		if (argc)
#if !defined(__minix)
			show(v, -1);
#else
			show(argv[0], -1);
#endif /* !defined(__minix) */
		else {
			DIR *dirp;
			struct dirent *dp;
#if !defined(__minix)
			__BITMAP_TYPE(, uint32_t, 65536) bm;

			__BITMAP_ZERO(&bm);
#else
			char *endp;
#endif /* !defined(__minix) */

			if ((dirp = opendir(_PATH_DEV)) == NULL)
				err(1, "opendir: %s", _PATH_DEV);

			while ((dp = readdir(dirp)) != NULL) {
#if !defined(__minix)
				if (strncmp(dp->d_name, "rvnd", 4) != 0)
					continue;
				n = atoi(dp->d_name + 4);
				if (__BITMAP_ISSET(n, &bm))
					continue;
				__BITMAP_SET(n, &bm);
				show(v, n);
#else
				if (strncmp(dp->d_name, "vnd", 3) != 0)
					continue;
				n = strtoul(dp->d_name + 3, &endp, 10);
				if (endp[0])
					continue;
				show(dp->d_name, n);
#endif /* !defined(__minix) */
			}

			closedir(dirp);
		}
#if !defined(__minix)
		close(v);
#endif /* !defined(__minix) */
		rv = 0;
	}
	return rv;
}

static void
#if !defined(__minix)
show(int v, int n)
#else
show(const char *vn, int n)
#endif /* !defined(__minix) */
{
	struct vnd_user vnu;
	char *dev;
	struct statvfs *mnt;
	int i, nmount;
#if defined(__minix)
	int v;
	char path[PATH_MAX];

	v = opendisk(vn, O_RDONLY, path, sizeof(path), 0);
	if (v == -1) {
		if (n == -1)
			err(1, "open: %s", vn);
		else
			printf("vnd%d: not in use\n", n);
		return;
	}
#endif /* defined(__minix) */

	vnu.vnu_unit = n;
	if (ioctl(v, VNDIOCGET, &vnu) == -1)
		err(1, "VNDIOCGET");

#if defined(__minix)
	close(v);
#endif /* defined(__minix) */

	if (vnu.vnu_ino == 0) {
		printf("vnd%d: not in use\n", vnu.vnu_unit);
		return;
	}

	printf("vnd%d: ", vnu.vnu_unit);

	dev = devname(vnu.vnu_dev, S_IFBLK);
	if (dev != NULL)
		nmount = getmntinfo(&mnt, MNT_NOWAIT);
	else {
		mnt = NULL;
		nmount = 0;
	}

	if (mnt != NULL) {
		for (i = 0; i < nmount; i++) {
			if (strncmp(mnt[i].f_mntfromname, "/dev/", 5) == 0 &&
			    strcmp(mnt[i].f_mntfromname + 5, dev) == 0)
				break;
		}
		if (i < nmount)
			printf("%s (%s) ", mnt[i].f_mntonname,
			    mnt[i].f_mntfromname);
		else
			printf("%s ", dev);
	}
	else if (dev != NULL)
		printf("%s ", dev);
	else
		printf("dev %llu,%llu ",
		    (unsigned long long)major(vnu.vnu_dev),
		    (unsigned long long)minor(vnu.vnu_dev));

	printf("inode %llu\n", (unsigned long long)vnu.vnu_ino);
}

static int
config(char *dev, char *file, char *geom, int action)
{
	struct vnd_ioctl vndio;
#if !defined(__minix)
	struct disklabel *lp;
#else
	int stop = 0;
#endif /* !defined(__minix) */
	char rdev[MAXPATHLEN + 1];
	int fd, rv;

#if defined(__minix)
	/*
	 * MINIX does not have the concept of raw devices.  As such, the access
	 * checks that apply to opening block devices, automatically apply here
	 * as well.  Therefore, we must open the device as read-only, or we
	 * would be unable to un-configure a device that was configured as
	 * read-only: opening such a device as read-write would fail.
	 */
	fd = opendisk(dev, O_RDONLY, rdev, sizeof(rdev), 0);

	if (fd < 0 && errno == ENXIO && action == VND_CONFIG && service) {
		stop = start_service(rdev);

		fd = opendisk(dev, O_RDONLY, rdev, sizeof(rdev), 0);
	}
#else
	fd = opendisk(dev, O_RDWR, rdev, sizeof(rdev), 0);
#endif /* defined(__minix) */
	if (fd < 0) {
		warn("%s: opendisk", rdev);
		return (1);
	}

	memset(&vndio, 0, sizeof(vndio));
#ifdef __GNUC__
	rv = 0;			/* XXX */
#endif

#if !defined(__minix)
	vndio.vnd_file = file;
#endif /* !defined(__minix) */
	if (geom != NULL) {
		rv = getgeom(&vndio.vnd_geom, geom);
#if defined(__minix)
		if (rv && stop)
			stop_service(fd, rdev);
#endif /* defined(__minix) */
		if (rv != 0)
			errx(1, "invalid geometry: %s", geom);
		vndio.vnd_flags = VNDIOF_HASGEOM;
#if !defined(__minix)
	} else if (tabname != NULL) {
		lp = getdiskbyname(tabname);
		if (lp == NULL)
			errx(1, "unknown disk type: %s", tabname);
		vndio.vnd_geom.vng_secsize = lp->d_secsize;
		vndio.vnd_geom.vng_nsectors = lp->d_nsectors;
		vndio.vnd_geom.vng_ntracks = lp->d_ntracks;
		vndio.vnd_geom.vng_ncylinders = lp->d_ncylinders;
		vndio.vnd_flags = VNDIOF_HASGEOM;
#endif /* !defined(__minix) */
	}

	if (readonly)
		vndio.vnd_flags |= VNDIOF_READONLY;

#if !defined(__minix)
	if (compressed)
		vndio.vnd_flags |= VNF_COMP;
#endif /* !defined(__minix) */

	/*
	 * Clear (un-configure) the device
	 */
	if (action == VND_UNCONFIG) {
		if (force)
			vndio.vnd_flags |= VNDIOF_FORCE;
		rv = ioctl(fd, VNDIOCCLR, &vndio);
#ifdef VNDIOOCCLR
		if (rv && errno == ENOTTY)
			rv = ioctl(fd, VNDIOOCCLR, &vndio);
#endif
		if (rv)
			warn("%s: VNDIOCCLR", rdev);
		else if (verbose)
			printf("%s: cleared\n", rdev);
#if defined(__minix)
		if (!rv && service)
			stop = 2;
#endif /* defined(__minix) */
	}
	/*
	 * Configure the device
	 */
	if (action == VND_CONFIG) {
		int	ffd;

		ffd = open(file, readonly ? O_RDONLY : O_RDWR);
		if (ffd < 0) {
			warn("%s", file);
			rv = -1;
		} else {
#if !defined(__minix)
			(void) close(ffd);
#else
			vndio.vnd_fildes = ffd;
#endif /* !defined(__minix) */

			rv = ioctl(fd, VNDIOCSET, &vndio);
#ifdef VNDIOOCSET
			if (rv && errno == ENOTTY) {
				rv = ioctl(fd, VNDIOOCSET, &vndio);
				vndio.vnd_size = vndio.vnd_osize;
			}
#endif
#if defined(__minix)
			(void) close(ffd);
#endif /* defined(__minix) */
			if (rv)
				warn("%s: VNDIOCSET", rdev);
			else if (verbose) {
				printf("%s: %" PRIu64 " bytes on %s", rdev,
				    vndio.vnd_size, file);
				if (vndio.vnd_flags & VNDIOF_HASGEOM)
					printf(" using geometry %d/%d/%d/%d",
					    vndio.vnd_geom.vng_secsize,
					    vndio.vnd_geom.vng_nsectors,
					    vndio.vnd_geom.vng_ntracks,
				    vndio.vnd_geom.vng_ncylinders);
				printf("\n");
			}
		}
#if defined(__minix)
		if ((ffd < 0 || rv) && service)
			stop++;
#endif /* defined(__minix) */
	}

#if defined(__minix)
	if (stop >= 2)
		stop_service(fd, rdev);
	else
#endif /* defined(__minix) */
	(void) close(fd);
	fflush(stdout);
	return (rv < 0);
}

static int
getgeom(struct vndgeom *vng, char *cp)
{
	char *secsize, *nsectors, *ntracks, *ncylinders;

#define	GETARG(arg) \
	do { \
		if (cp == NULL || *cp == '\0') \
			return (1); \
		arg = strsep(&cp, "/"); \
		if (arg == NULL) \
			return (1); \
	} while (0)

	GETARG(secsize);
	GETARG(nsectors);
	GETARG(ntracks);
	GETARG(ncylinders);

#undef GETARG

	/* Too many? */
	if (cp != NULL)
		return (1);

#define	CVTARG(str, num) \
	do { \
		num = strtol(str, &cp, 10); \
		if (*cp != '\0') \
			return (1); \
	} while (0)

	CVTARG(secsize, vng->vng_secsize);
	CVTARG(nsectors, vng->vng_nsectors);
	CVTARG(ntracks, vng->vng_ntracks);
	CVTARG(ncylinders, vng->vng_ncylinders);

#undef CVTARG

	return (0);
}

static void
usage(void)
{

	(void)fprintf(stderr, "%s%s",
#if !defined(__minix)
	    "usage: vnconfig [-crvz] [-f disktab] [-t typename] vnode_disk"
		" regular-file [geomspec]\n",
	    "       vnconfig -u [-Fv] vnode_disk\n"
#else
	    "usage: vnconfig [-Scrv] vnode_disk regular-file [geomspec]\n",
	    "       vnconfig -u [-SFv] vnode_disk\n"
#endif /* !defined(__minix) */
	    "       vnconfig -l [vnode_disk]\n");
	exit(1);
}
