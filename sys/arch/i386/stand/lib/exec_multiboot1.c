/* $NetBSD: exec_multiboot1.c,v 1.1.2.2 2019/09/17 19:32:00 martin Exp $ */

/*
 * Copyright (c) 2019 The NetBSD Foundation, Inc.
 * All rights reserved.
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

#include <sys/param.h>
#include <sys/reboot.h> 
 
#include <i386/multiboot.h>
 
#include <lib/libsa/stand.h> 
#include <lib/libkern/libkern.h> 
 
#include "loadfile.h"
#include "libi386.h"
#include "bootinfo.h"
#include "bootmod.h"
#include "vbe.h"

extern struct btinfo_modulelist *btinfo_modulelist;

static int
exec_multiboot1(struct multiboot_package *mbp)
{
	struct multiboot_info *mbi;
	struct multiboot_module *mbm;
	int i, len;
	char *cmdline;
	struct bi_modulelist_entry *bim;

	mbi = alloc(sizeof(struct multiboot_info));
	mbi->mi_flags = MULTIBOOT_INFO_HAS_MEMORY;

	mbi->mi_mem_upper = mbp->mbp_extmem;
	mbi->mi_mem_lower = mbp->mbp_basemem;

	if (mbp->mbp_args) {
		mbi->mi_flags |= MULTIBOOT_INFO_HAS_CMDLINE;
		len = strlen(mbp->mbp_file) + 1 + strlen(mbp->mbp_args) + 1;
		cmdline = alloc(len);
		snprintf(cmdline, len, "%s %s", mbp->mbp_file, mbp->mbp_args);
		mbi->mi_cmdline = (char *) vtophys(cmdline);
	}

	/* pull in any modules if necessary */
	if (btinfo_modulelist) {
		mbm = alloc(sizeof(struct multiboot_module) *
				   btinfo_modulelist->num);

		bim = (struct bi_modulelist_entry *)
		  (((char *) btinfo_modulelist) +
		   sizeof(struct btinfo_modulelist));
		for (i = 0; i < btinfo_modulelist->num; i++) {
			mbm[i].mmo_start = bim->base;
			mbm[i].mmo_end = bim->base + bim->len;
			mbm[i].mmo_string = (char *)vtophys(bim->path);
			mbm[i].mmo_reserved = 0;
			bim++;
		}
		mbi->mi_flags |= MULTIBOOT_INFO_HAS_MODS;
		mbi->mi_mods_count = btinfo_modulelist->num;
		mbi->mi_mods_addr = vtophys(mbm);
	}

#ifdef DEBUG
	printf("Start @ 0x%lx [%ld=0x%lx-0x%lx]...\n",
	    mbp->mbp_marks[MARK_ENTRY],
	    mbp->mbp_marks[MARK_NSYM], 
	    mbp->mbp_marks[MARK_SYM],
	    mbp->mbp_marks[MARK_END]);
#endif

	/* Does not return */
	multiboot(mbp->mbp_marks[MARK_ENTRY], vtophys(mbi),
	    x86_trunc_page(mbi->mi_mem_lower * 1024), MULTIBOOT_INFO_MAGIC);

	return 0;
}

static void
cleanup_multiboot1(struct multiboot_package *mbp)
{
	dealloc(mbp->mbp_header, sizeof(*mbp->mbp_header));
	dealloc(mbp, sizeof(*mbp));

	return;
}


struct multiboot_package *
probe_multiboot1(const char *path)
{
	int fd = -1;
	size_t i;
	char buf[8192 + sizeof(struct multiboot_header)];
	ssize_t readen;
	struct multiboot_package *mbp = NULL;

	if ((fd = open(path, 0)) == -1)
		goto out;

	readen = read(fd, buf, sizeof(buf));
	if (readen < sizeof(struct multiboot_header))
		goto out;

	for (i = 0; i < readen; i += 8) {
		struct multiboot_header *mbh;

		mbh = (struct multiboot_header *)(buf + i);
		
		if (mbh->mh_magic != MULTIBOOT_HEADER_MAGIC)
			continue;
		
		if (mbh->mh_magic + mbh->mh_flags + mbh->mh_checksum)
			continue;

		mbp = alloc(sizeof(*mbp));
		mbp->mbp_version	= 1;
		mbp->mbp_file		= path;
		mbp->mbp_header		= alloc(sizeof(*mbp->mbp_header));
		mbp->mbp_probe		= *probe_multiboot1;
		mbp->mbp_exec		= *exec_multiboot1;
		mbp->mbp_cleanup	= *cleanup_multiboot1;

		memcpy(mbp->mbp_header, mbh, sizeof(*mbp->mbp_header));

		goto out;

	}
		
out:
	if (fd != -1)
		close(fd);

	return mbp;
}
