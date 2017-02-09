/*	$NetBSD: rump_lfs.c,v 1.18 2015/08/02 18:11:57 dholland Exp $	*/

/*
 * Copyright (c) 2008 Antti Kantee.  All Rights Reserved.
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

#include <sys/types.h>
#include <sys/mount.h>

#include <ufs/ufs/ufsmount.h>

#include <err.h>
#include <pthread.h>
#include <puffs.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <rump/rump.h>
#include <rump/p2k.h>
#include <rump/ukfs.h>

#include "mount_lfs.h"

static void *
cleaner(void *arg)
{
	const char *the_argv[7];

	the_argv[0] = "megamaid";
	the_argv[1] = "-D"; /* don't fork() & detach */
	the_argv[2] = arg;

	lfs_cleaner_main(3, __UNCONST(the_argv));

	return NULL;
}

int
main(int argc, char *argv[])
{
	struct ulfs_args args;
	char canon_dev[UKFS_DEVICE_MAXPATHLEN], canon_dir[MAXPATHLEN];
	char rawdev[MAXPATHLEN];
	struct p2k_mount *p2m;
	pthread_t cleanerthread;
	struct ukfs_part *part;
	int mntflags;
	int rv;

	setprogname(argv[0]);
	puffs_unmountonsignal(SIGINT, true);
	puffs_unmountonsignal(SIGTERM, true);

	if (argc >= 3) {
		UKFS_DEVICE_ARGVPROBE(&part);
		if (part != ukfs_part_none) {
			errx(1, "lfs does not currently support "
			    "embedded partitions");
		}
	}
	mount_lfs_parseargs(argc, argv, &args, &mntflags, canon_dev, canon_dir);

	/* Reset getopt for lfs_cleaner_main.  */
	optreset = 1;
	optind = 1;

	p2m = p2k_init(0);
	if (!p2m)
		err(1, "init p2k");
	/*
	 * XXX: this particular piece inspired by the cleaner code.
	 * obviously FIXXXME along with the cleaner.
	 */
	sprintf(rawdev, "/dev/r%s", canon_dev+5);
	rump_pub_etfs_register(rawdev, canon_dev, RUMP_ETFS_CHR);

	/*
	 * We basically have two choices:
	 *  1) run the cleaner in another process and do rump ipc syscalls
	 *  2) run it off a thread in this process and do rump syscalls
	 *     as function calls.
	 *
	 * opt for "2" for now
	 */
#ifdef CLEANER_TESTING
	ukfs_mount(MOUNT_LFS, canon_dev, canon_dir, mntflags,
	    &args, sizeof(args));
	cleaner(canon_dir);
#endif
	if (p2k_setup_diskfs(p2m, MOUNT_LFS, canon_dev, part, canon_dir,
	    mntflags, &args, sizeof(args)) == -1)
		err(1, "mount");
	ukfs_part_release(part);

#ifndef CLEANER_TESTING
	if ((mntflags & MNT_RDONLY) == 0) {
		if (pthread_create(&cleanerthread, NULL,
		    cleaner, canon_dir) == -1)
			err(1, "cannot start cleaner");
	}
#endif

	rv = p2k_mainloop(p2m);
	if (rv == -1)
		err(1, "fs service");

	return 0;
}
