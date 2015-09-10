/*	$NetBSD: v7fs.c,v 1.8 2013/01/29 15:52:25 christos Exp $	*/

/*-
 * Copyright (c) 2011 The NetBSD Foundation, Inc.
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

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <sys/cdefs.h>
#if defined(__RCSID) && !defined(__lint)
__RCSID("$NetBSD: v7fs.c,v 1.8 2013/01/29 15:52:25 christos Exp $");
#endif	/* !__lint */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <util.h>

#include "makefs.h"
#include "v7fs.h"
#include "v7fs_impl.h"
#include "v7fs_makefs.h"
#include "newfs_v7fs.h"


#ifndef HAVE_NBTOOL_CONFIG_H
#include "progress.h"
static bool progress_bar_enable;
#endif
int v7fs_newfs_verbose;

void
v7fs_prep_opts(fsinfo_t *fsopts)
{
	v7fs_opt_t *v7fs_opts = ecalloc(1, sizeof(*v7fs_opts));
	const option_t v7fs_options[] = {
		{ 'p', "pdp", &v7fs_opts->pdp_endian, OPT_INT32, false, true,
		    "PDP endian" },
		{ 'P', "progress", &v7fs_opts->progress, OPT_INT32, false, true,
		  "Progress bar" },
		{ .name = NULL }
	};

	fsopts->fs_specific = v7fs_opts;
	fsopts->fs_options = copy_opts(v7fs_options);
}

void
v7fs_cleanup_opts(fsinfo_t *fsopts)
{
	free(fsopts->fs_specific);
	free(fsopts->fs_options);
}

int
v7fs_parse_opts(const char *option, fsinfo_t *fsopts)
{

	return set_option_var(fsopts->fs_options, option, "1", NULL, 0) != -1;
}

void
v7fs_makefs(const char *image, const char *dir, fsnode *root, fsinfo_t *fsopts)
{
	struct v7fs_mount_device v7fs_mount;
	int fd, endian, error = 1;
	v7fs_opt_t *v7fs_opts = fsopts->fs_specific;

	v7fs_newfs_verbose = debug;
#ifndef HAVE_NBTOOL_CONFIG_H
	if ((progress_bar_enable = v7fs_opts->progress)) {
		progress_switch(progress_bar_enable);
		progress_init();
		progress(&(struct progress_arg){ .cdev = image });
	}
#endif

	/* Determine filesystem image size */
	v7fs_estimate(dir, root, fsopts);
	printf("Calculated size of `%s': %lld bytes, %ld inodes\n",
	    image, (long long)fsopts->size, (long)fsopts->inodes);

	if ((fd = open(image, O_RDWR | O_CREAT | O_TRUNC, 0666)) == -1) {
		err(EXIT_FAILURE, "%s", image);
	}
	if (lseek(fd, fsopts->size - 1, SEEK_SET) == -1) {
		goto err_exit;
	}
	if (write(fd, &fd, 1) != 1) {
		goto err_exit;
	}
	if (lseek(fd, 0, SEEK_SET) == -1) {
		goto err_exit;
	}
	fsopts->fd = fd;
	v7fs_mount.device.fd = fd;

#if !defined BYTE_ORDER
#error
#endif
#if BYTE_ORDER == LITTLE_ENDIAN
	if (fsopts->needswap)
		endian = BIG_ENDIAN;
	else
		endian = LITTLE_ENDIAN;
#else
	if (fsopts->needswap)
		endian = LITTLE_ENDIAN;
	else
		endian = BIG_ENDIAN;
#endif
	if (v7fs_opts->pdp_endian) {
		endian = PDP_ENDIAN;
	}

	v7fs_mount.endian = endian;
	v7fs_mount.sectors = fsopts->size >> V7FS_BSHIFT;
	if (v7fs_newfs(&v7fs_mount, fsopts->inodes) != 0) {
		goto err_exit;
	}

	if (v7fs_populate(dir, root, fsopts, &v7fs_mount) != 0) {
		error = 2;	/* some files couldn't add */
		goto err_exit;
	}

	close(fd);
	return;

 err_exit:
	close(fd);
	err(error, "%s", image);
}

void
progress(const struct progress_arg *p)
{
#ifndef HAVE_NBTOOL_CONFIG_H
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
#endif
}
