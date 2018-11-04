/*	$NetBSD: newfs_v7fs.c,v 1.5 2017/01/10 20:53:09 christos Exp $ */

/*-
 * Copyright (c) 2004, 2011 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by UCHIYAMA Yasushi.
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
#ifndef lint
__RCSID("$NetBSD: newfs_v7fs.c,v 1.5 2017/01/10 20:53:09 christos Exp $");
#endif /* not lint */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/disklabel.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

#include <err.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include <fs/v7fs/v7fs.h>
#include "v7fs_impl.h"
#include "progress.h"
#include "newfs_v7fs.h"

static void usage(void) __dead;
static bool progress_bar_enable = false;
int v7fs_newfs_verbose = 3;	/* newfs compatible */

int
main(int argc, char **argv)
{
	const char *device;
	struct disklabel d;
	struct partition *p;
	struct stat st;
	uint32_t partsize;
	int Fflag, Zflag;
	int part;
	int fd, ch;
	int endian = _BYTE_ORDER;
	int32_t maxfile = 0;

	if (argc < 2)
		usage();

	Fflag = Zflag = partsize = 0;
	while ((ch = getopt(argc, argv, "Fs:Zs:n:B:V:")) != -1) {
		switch (ch) {
		case 'V':
			v7fs_newfs_verbose = atoi(optarg);
			break;
		case 'F':
			Fflag = 1;
			break;
		case 's':
			partsize = atoi(optarg);
			break;
		case 'n':
			maxfile = atoi(optarg);
			break;
		case 'Z':
			Zflag = 1;
			break;
		case 'B':
			switch (optarg[0]) {
			case 'l':
				endian = _LITTLE_ENDIAN;
				break;
			case 'b':
				endian = _BIG_ENDIAN;
				break;
			case 'p':
				endian = _PDP_ENDIAN;
				break;
			}
			break;
		default:
			usage();
			/*NOTREACHED*/
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 1)
		usage();
	device = argv[0];

	progress_bar_enable = v7fs_newfs_verbose > 1;


	if (progress_bar_enable) {
		progress_switch(progress_bar_enable);
		progress_init();
		progress(&(struct progress_arg){ .cdev = device });
	}

	if (!Fflag) {
		if ((fd = open(device, O_RDWR)) == -1) {
			err(EXIT_FAILURE, "%s", device);
		}
		if (fstat(fd, &st) != 0) {
			goto err_exit;
		}
		if (!S_ISCHR(st.st_mode)) {
			warnx("not a raw device");
		}

		part = DISKPART(st.st_rdev);

		if (ioctl(fd, DIOCGDINFO, &d) == -1) {
			goto err_exit;
		}
		p = &d.d_partitions[part];
		if (v7fs_newfs_verbose) {
			printf("partition=%d size=%d offset=%d fstype=%d"
			    " secsize=%d\n", part, p->p_size, p->p_offset,
			    p->p_fstype, d.d_secsize);
		}
		if (p->p_fstype != FS_V7) {
			warnx("not a Version 7 partition");
			goto err_exit;
		}
		partsize = p->p_size;
	} else {
		off_t filesize;
		uint8_t zbuf[8192] = {0, };

		if (partsize == 0) {
			errx(EXIT_FAILURE, "-F requires -s");
		}

		filesize = partsize << V7FS_BSHIFT;

		fd = open(device, O_RDWR|O_CREAT|O_TRUNC, 0666);
		if (fd == -1) {
			err(EXIT_FAILURE, "%s", device);
		}

		if (Zflag) {
			while (filesize > 0) {
				size_t writenow = MIN(filesize,
				    (off_t)sizeof(zbuf));

				if ((size_t)write(fd, zbuf, writenow) !=
				    writenow) {
					err(EXIT_FAILURE, NULL);
				}
				filesize -= writenow;
			}
		} else {
			if (lseek(fd, filesize - 1, SEEK_SET) == -1) {
				goto err_exit;
			}
			if (write(fd, zbuf, 1) != 1) {
				goto err_exit;
			}
			if (lseek(fd, 0, SEEK_SET) == -1) {
				goto err_exit;
			}
		}
	}

	if (v7fs_newfs(&(struct v7fs_mount_device)
		{ .device.fd = fd, .endian = endian, .sectors = partsize },
		maxfile) != 0)
		goto err_exit;

	close(fd);

	return EXIT_SUCCESS;
 err_exit:
	close(fd);
	err(EXIT_FAILURE, NULL);
}

void
progress(const struct progress_arg *p)
{
	static struct progress_arg Progress;
	static char cdev[32];
	static char label[32];

	if (!progress_bar_enable)
		return;

	if (p) {
		Progress = *p;
		if (p->cdev)
			strcpy(cdev, p->cdev);
		if (p->label)
			strcpy(label, p->label);
	}

	if (!Progress.tick)
		return;
	if (++Progress.cnt > Progress.tick) {
		Progress.cnt = 0;
		Progress.total++;
		progress_bar(cdev, label, Progress.total, PROGRESS_BAR_GRANULE);
	}
}

static void
usage(void)
{

	(void)fprintf(stderr, "usage: \n%s [-FZ] [-B byte-order]"
	    " [-n inodes] [-s sectors] [-V verbose] special\n", getprogname());

	exit(EXIT_FAILURE);
}
