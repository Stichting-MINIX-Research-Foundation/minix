/*	$NetBSD: rump_cd9660.c,v 1.8 2010/05/30 04:32:09 dholland Exp $	*/

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
#include <sys/param.h>
#include <sys/mount.h>

#include <isofs/cd9660/cd9660_mount.h>

#include <rump/p2k.h>
#include <rump/ukfs.h> 

#include <err.h>
#include <stdlib.h>
#include <string.h>

#include "mount_cd9660.h"

int
main(int argc, char *argv[])
{
	struct iso_args args;
	char canon_dev[UKFS_DEVICE_MAXPATHLEN], canon_dir[MAXPATHLEN];
	struct ukfs_part *part;
	int mntflags;
	int rv;

	setprogname(argv[0]);

	UKFS_DEVICE_ARGVPROBE(&part);
	mount_cd9660_parseargs(argc, argv, &args, &mntflags,
	    canon_dev, canon_dir);
	rv = p2k_run_diskfs(MOUNT_CD9660, canon_dev, part, canon_dir, mntflags,
	    &args, sizeof(args), 0);
	ukfs_part_release(part);
	if (rv)
		err(1, "mount");

	return 0;
}
