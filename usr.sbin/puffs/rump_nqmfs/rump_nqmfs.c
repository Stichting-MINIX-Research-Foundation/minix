/*	$NetBSD: rump_nqmfs.c,v 1.3 2010/03/31 14:54:07 pooka Exp $	*/

/*
 * Copyright (c) 2010 Antti Kantee.  All Rights Reserved.
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
 * Not Quite MFS.  Instead of allocating memory and creating a FFS file system
 * there, mmap a file (which should contain ffs) and use that as the backend.
 * You can also give a newly created image as the file ...
 */

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/stat.h>

#include <ufs/ufs/ufsmount.h>

#include <err.h>
#include <mntopts.h>
#include <puffs.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <rump/p2k.h>
#include <rump/ukfs.h>

const struct mntopt mopts[] = {
	MOPT_STDOPTS,
	MOPT_NULL,
};

static void
usage(void)
{

	fprintf(stderr, "%s: [-o args] fspec mountpoint\n", getprogname());
	exit(1);
}

static struct mfs_args args;
static char *mntpath, *mntfile;
static int mntflags, altflags;

int
main(int argc, char *argv[])
{
	struct stat sb;
	mntoptparse_t mp;
	void *membase;
	int ch, fd, rdonly, shared;

	setprogname(argv[0]);
	puffs_unmountonsignal(SIGINT, true);
	puffs_unmountonsignal(SIGTERM, true);

	shared = mntflags = altflags = 0;

	memset(&args, 0, sizeof(args));
	while ((ch = getopt(argc, argv, "o:s")) != -1) {
		switch (ch) {
		case 'o':
			mp = getmntopts(optarg, mopts, &mntflags, &altflags);
			if (mp == NULL)
				err(1, "getmntopts");
			freemntopts(mp);
			break;
		case 's':
			shared = 1;
			break;
		default:
			usage();
			break;
		}
	}
	argc -= optind;
	argv += optind;
	
	if (argc != 2)
		usage();
	mntfile = argv[0];
	mntpath = argv[1];
	rdonly = mntflags & MNT_RDONLY;

	if (stat(mntfile, &sb) == -1)
		err(1, "stat fspec");
	if (!S_ISREG(sb.st_mode))
		errx(1, "fspec must be a regular file");

	fd = open(mntfile, rdonly ? O_RDONLY: O_RDWR);
	if (fd == -1)
		err(1, "open fspec");

	membase = mmap(NULL, sb.st_size, PROT_READ | (rdonly ? 0 : PROT_WRITE),
	    MAP_FILE | (shared ? MAP_SHARED : MAP_PRIVATE), fd, 0);
	if (membase == MAP_FAILED)
		err(1, "cannot mmap fspec");

	args.fspec = mntfile;
	args.base = membase;
	args.size = sb.st_size;

	if (p2k_run_fs(MOUNT_MFS, "/swabbie", mntpath,
	    0, &args, sizeof(args), 0) == -1)
		err(1, "p2k");

	return 0;
}
