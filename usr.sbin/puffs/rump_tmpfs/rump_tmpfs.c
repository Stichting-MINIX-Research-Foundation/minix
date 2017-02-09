/*	$NetBSD: rump_tmpfs.c,v 1.4 2009/12/17 14:06:38 pooka Exp $	*/

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

#include <fs/tmpfs/tmpfs_args.h>

#include <err.h>
#include <puffs.h>
#include <stdlib.h>
#include <unistd.h>

#include <rump/p2k.h>

#include "mount_tmpfs.h"

/*
 * tmpfs has a slight "issue" with file content storage.  Since it
 * stores data in memory, we will end up using 2x the RAM for
 * storage if we cache in puffs vfs.  If we don't, we need to take
 * the slow path from the kernel to the file server every time the
 * file is accessed.  It's either suck or suck.  However, rump_tmpfs
 * will not be used in real life, so this is hardly a release-stopping
 * issue of catastrophic proportions.  For now we opt for un-double
 * storage (PUFFS_KFLAG_NOCACHE_PAGE).
 */

int
main(int argc, char *argv[])
{
	struct tmpfs_args args;
	char canon_dev[MAXPATHLEN], canon_dir[MAXPATHLEN];
	int mntflags;
	int rv;

	setprogname(argv[0]);

	mount_tmpfs_parseargs(argc, argv, &args, &mntflags,
	    canon_dev, canon_dir);
	rv = p2k_run_fs(MOUNT_TMPFS, canon_dev, canon_dir, mntflags,
		&args, sizeof(args), PUFFS_KFLAG_NOCACHE_PAGE);
	if (rv)
		err(1, "mount");

	return 0;
}
