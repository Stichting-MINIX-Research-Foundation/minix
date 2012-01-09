/*	$NetBSD: devopen.c,v 1.8 2010/12/24 20:40:42 jakllsch Exp $	 */

/*-
 * Copyright (c) 2005 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Bang Jun-Young.
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
 * Copyright (c) 1996, 1997
 *	Matthias Drochner.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include <sys/types.h>

#include <lib/libsa/stand.h>
#include <lib/libkern/libkern.h>

#include <libi386.h>
#include <biosdisk.h>
#include "devopen.h"
#ifdef _STANDALONE
#include <bootinfo.h>
#endif
#ifdef SUPPORT_PS2
#include <biosmca.h>
#endif

static int dev2bios(char *, int, int *);

static int
dev2bios(char *devname, int unit, int *biosdev)
{

	if (strcmp(devname, "hd") == 0)
		*biosdev = 0x80 + unit;
	else if (strcmp(devname, "fd") == 0)
		*biosdev = 0x00 + unit;
	else if (strcmp(devname, "cd") == 0)
		*biosdev = boot_biosdev;
	else
		return ENXIO;

	return 0;
}

void
bios2dev(int biosdev, daddr_t sector, char **devname, int *unit, int *partition)
{

	/* set default */
	*unit = biosdev & 0x7f;

	if (biosdev & 0x80) {
		/*
		 * There seems to be no standard way of numbering BIOS
		 * CD-ROM drives. The following method is a little tricky
		 * but works nicely.
		 */
		if (biosdev >= 0x80 + get_harddrives()) {
			*devname = "cd";
			*unit = 0;		/* override default */
		} else
			*devname = "hd";
	} else
		*devname = "fd";

	*partition = biosdisk_findpartition(biosdev, sector);
}

#ifdef _STANDALONE
struct btinfo_bootpath bibp;
extern bool kernel_loaded;
#endif

/*
 * Open the BIOS disk device
 */
int
devopen(struct open_file *f, const char *fname, char **file)
{
	char *fsname, *devname;
	int unit, partition;
	int biosdev;
	int error;

	if ((error = parsebootfile(fname, &fsname, &devname,
				   &unit, &partition, (const char **) file))
	    || (error = dev2bios(devname, unit, &biosdev)))
		return error;

	f->f_dev = &devsw[0];		/* must be biosdisk */

#ifdef _STANDALONE
	if (!kernel_loaded) {
		strncpy(bibp.bootpath, *file, sizeof(bibp.bootpath));
		BI_ADD(&bibp, BTINFO_BOOTPATH, sizeof(bibp));
	}
#endif

	return biosdisk_open(f, biosdev, partition);
}
