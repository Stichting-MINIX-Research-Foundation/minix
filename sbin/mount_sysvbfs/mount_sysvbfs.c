/*	$NetBSD: mount_sysvbfs.c,v 1.8 2011/08/29 14:35:02 joerg Exp $	*/

/*-
 * Copyright (c) 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
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
 */

#include <sys/cdefs.h>
#ifndef lint
__COPYRIGHT("@(#) Copyright (c) 1993, 1994\
 The Regents of the University of California.  All rights reserved.");
#endif /* not lint */

#ifndef lint
__RCSID("$NetBSD: mount_sysvbfs.c,v 1.8 2011/08/29 14:35:02 joerg Exp $");
#endif /* not lint */

#include <sys/param.h>
#include <sys/mount.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <unistd.h>

#include <mntopts.h>

#include "mountprog.h"
#include "mount_sysvbfs.h"

static const struct mntopt mopts[] = {
	MOPT_STDOPTS,
	MOPT_UPDATE,
	MOPT_GETARGS,
	MOPT_NULL,
};

__dead static void
sysvbfs_usage(void)
{

	(void)fprintf(stderr, "usage: %s [-o options] special node\n",
	    getprogname());
	exit(EXIT_FAILURE);
}

#ifndef MOUNT_NOMAIN
int
main(int argc, char **argv)
{

	return mount_sysvbfs(argc, argv);
}
#endif

void
mount_sysvbfs_parseargs(int argc, char **argv,
	struct sysvbfs_args *args, int *mntflags,
	char *canon_dev, char *canon_dir)
{
	int ch;
	mntoptparse_t mp;

	*mntflags = 0;
	optind = optreset = 1;		/* Reset for parse of new argv. */
	while ((ch = getopt(argc, argv, "o:")) != -1)
		switch (ch) {
		case 'o':
			mp = getmntopts(optarg, mopts, mntflags, 0);
			if (mp == NULL)
				err(1, "getmntopts");
			freemntopts(mp);
			break;
		case '?':
		default:
			sysvbfs_usage();
		}
	argc -= optind;
	argv += optind;

	if (argc != 2)
		sysvbfs_usage();

	pathadj(argv[0], canon_dev);
	args->fspec = canon_dev;
	pathadj(argv[1], canon_dir);
}

int
mount_sysvbfs(int argc, char *argv[])
{
	struct sysvbfs_args args;
	char canon_dev[MAXPATHLEN], canon_dir[MAXPATHLEN];
	const char *errcause;
	int mntflags;

	mount_sysvbfs_parseargs(argc, argv, &args, &mntflags,
	    canon_dev, canon_dir);

	if (mount(MOUNT_SYSVBFS, canon_dir, mntflags, &args, sizeof args)==-1) {
		switch (errno) {
		case EMFILE:
			errcause = "mount table full";
			break;
		case EINVAL:
			if (mntflags & MNT_UPDATE)
				errcause =
			    "specified device does not match mounted device";
			else
				errcause = "incorrect super block";
			break;
		default:
			errcause = strerror(errno);
			break;
		}
		errx(EXIT_FAILURE, "%s on %s: %s",
		    canon_dev, canon_dir, errcause);
	}
	exit(EXIT_SUCCESS);
}

#if defined(__minix)
int __real_main(int argc, char *argv[]);

int
__real_main(int argc, char *argv[])
{
	return mount_sysvbfs(argc, argv);
}
#endif
