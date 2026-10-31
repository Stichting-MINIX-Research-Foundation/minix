/*	$NetBSD: biosboot.c,v 1.32 2019/06/20 10:56:38 martin Exp $ */

/*
 * Copyright (c) 2009 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to the NetBSD Foundation
 * by Mike M. Volokhov. Development of this software was supported by the
 * Google Summer of Code program.
 * The GSoC project was mentored by Allen Briggs and Joerg Sonnenberger.
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
#ifdef __RCSID
__RCSID("$NetBSD: biosboot.c,v 1.32 2019/06/20 10:56:38 martin Exp $");
#endif

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/bootblock.h>

#if defined(DIOCGWEDGEINFO) && !defined(HAVE_NBTOOL_CONFIG_H)
#define USE_WEDGES
#endif
#ifdef USE_WEDGES
#include <sys/disk.h>
#endif

#include <err.h>
#include <fcntl.h>
#include <paths.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "map.h"
#include "gpt.h"
#include "gpt_private.h"

#define DEFAULT_BOOTDIR		"/usr/mdec"
#define DEFAULT_BOOTCODE	"gptmbr.bin"

static int cmd_biosboot(gpt_t, int, char *[]);

static const char *biosboothelp[] = {
	"[-A] [-c bootcode] [-i index] [-L label] [-b startsec]",
#if notyet
	"[-a alignment] [-b blocknr] [-i index] [-l label]",
	"[-s size] [-t type]",
#endif
};

struct gpt_cmd c_biosboot = {
	"biosboot",
	cmd_biosboot,
	biosboothelp, __arraycount(biosboothelp),
	0,
};

#define usage() gpt_usage(NULL, &c_biosboot)

static struct mbr*
read_boot(gpt_t gpt, const char *bootpath)
{
	int bfd, ret = -1;
	struct mbr *buf;
	struct stat st;
	char *bp;

	buf = NULL;
	bfd = -1;

	if (bootpath == NULL)
		bp = strdup(DEFAULT_BOOTDIR "/" DEFAULT_BOOTCODE);
	else if (*bootpath == '/')
		bp = strdup(bootpath);
	else {
		if (asprintf(&bp, "%s/%s", DEFAULT_BOOTDIR, bootpath) < 0)
			bp = NULL;
	}

	if (bp == NULL) {
		gpt_warn(gpt, "Can't allocate memory for bootpath");
		goto fail;
	}

	if ((buf = malloc((size_t)gpt->secsz)) == NULL) {
		gpt_warn(gpt, "Can't allocate memory for sector");
		goto fail;
	}


	if ((bfd = open(bp, O_RDONLY)) < 0 || fstat(bfd, &st) == -1) {
		gpt_warn(gpt, "Can't open `%s'", bp);
		goto fail;
	}

	if (st.st_size != MBR_DSN_OFFSET) {
		gpt_warnx(gpt, "The bootcode in `%s' does not match the"
		    " expected size %u", bp, MBR_DSN_OFFSET);
		goto fail;
	}

	if (read(bfd, buf, (size_t)st.st_size) != (ssize_t)st.st_size) {
		gpt_warn(gpt, "Error reading from `%s'", bp);
		goto fail;
	}

	ret = 0;
fail:
	if (bfd != -1)
		close(bfd);
	if (ret == -1) {
		free(buf);
		buf = NULL;
	}
	free(bp);
	return buf;
}

static int
set_bootable(gpt_t gpt, map_t map, map_t tbl, unsigned int i)
{
	unsigned int j;
	struct gpt_hdr *hdr = map->map_data;
	struct gpt_ent *ent;
	unsigned int ne = le32toh(hdr->hdr_entries);

	for (j = 0; j < ne; j++) {
		ent = gpt_ent(map, tbl, j);
		ent->ent_attr &= ~GPT_ENT_ATTR_LEGACY_BIOS_BOOTABLE;
	}

	ent = gpt_ent(map, tbl, i);
	ent->ent_attr |= GPT_ENT_ATTR_LEGACY_BIOS_BOOTABLE;

	return gpt_write_crc(gpt, map, tbl);
}

static int
biosboot(gpt_t gpt, daddr_t start, uint64_t size, u_int entry, uint8_t *label,
    const char *bootpath, int active)
{
	map_t mbrmap, m;
	struct mbr *mbr, *bootcode;
	unsigned int i;
	struct gpt_ent *ent;
	uint8_t utfbuf[__arraycount(ent->ent_name) * 3 + 1];

	/*
	 * Parse and validate partition maps
	 */
	if (gpt_hdr(gpt) == NULL)
		return -1;

	mbrmap = map_find(gpt, MAP_TYPE_PMBR);
	if (mbrmap == NULL || mbrmap->map_start != 0) {
		gpt_warnx(gpt, "No valid Protective MBR found");
		return -1;
	}

	mbr = mbrmap->map_data;

	/*
	 * Update the boot code
	 */
	if ((bootcode = read_boot(gpt, bootpath)) == NULL) {
		gpt_warnx(gpt, "Error reading bootcode");
		return -1;
	}
	(void)memcpy(&mbr->mbr_code, &bootcode->mbr_code,
		sizeof(mbr->mbr_code));
	free(bootcode);

	for (i = 0; i < __arraycount(mbr->mbr_part); i++)
		if (mbr->mbr_part[i].part_typ == MBR_PTYPE_PMBR)
			mbr->mbr_part[i].part_flag = active ? 0x80 : 0;

	/*
	 * Walk through the GPT and see where we can boot from
	 */
	for (m = map_first(gpt); m != NULL; m = m->map_next) {
		if (m->map_type != MAP_TYPE_GPT_PART || m->map_index < 1)
			continue;

		ent = m->map_data;

		/* first, prefer user selection */
		if (entry > 0 && m->map_index == entry)
			break;

		if (label != NULL) {
			utf16_to_utf8(ent->ent_name,
			    __arraycount(ent->ent_name), utfbuf,
			    __arraycount(utfbuf));
			if (strcmp((char *)label, (char *)utfbuf) == 0)
				break;
		}

		/* next, partition as could be specified by wedge */
		if (entry < 1 && label == NULL && size > 0 &&
		    m->map_start == start && m->map_size == (off_t)size)
			break;
		/* next could be start sector specified by -b option */
		if (entry < 1 && label == NULL && size == 0 &&
		    m->map_start == start)
			break;
	}

	if (m == NULL) {
		gpt_warnx(gpt, "No bootable partition");
		return -1;
	}

	i = m->map_index - 1;


	if (set_bootable(gpt, gpt->gpt, gpt->tbl, i) == -1)
		return -1;

	if (set_bootable(gpt, gpt->tpg, gpt->lbt, i) == -1)
		return -1;

	if (gpt_write(gpt, mbrmap) == -1) {
		gpt_warnx(gpt, "Cannot update Protective MBR");
		return -1;
	}

	gpt_msg(gpt, "Partition %d marked as bootable", i + 1);
	return 0;
}

static int
cmd_biosboot(gpt_t gpt, int argc, char *argv[])
{
#ifdef USE_WEDGES
	struct dkwedge_info dkw;
#endif
	int ch;
	gpt_t ngpt = gpt;
	off_t start = 0;
	uint64_t size = 0;
	int active = 0;
	unsigned int entry = 0;
	uint8_t *label = NULL;
	char *bootpath = NULL;

	while ((ch = getopt(argc, argv, "Ac:i:L:b:")) != -1) {
		switch(ch) {
		case 'A':
			active = 1;
			break;
		case 'c':
			if (gpt_name_get(gpt, &bootpath) == -1)
				goto usage;
			break;
		case 'i':
			if (gpt_uint_get(gpt, &entry) == -1)
				goto usage;
			break;
		case 'L':
			if (gpt_name_get(gpt, &label) == -1)
				goto usage;
			break;
		case 'b':
			if (gpt_human_get(gpt, &start) == -1)
				goto usage;
			break;
		default:
			goto usage;
		}
	}

	if (argc != optind)
		return usage();

#ifdef USE_WEDGES
	if ((gpt->sb.st_mode & S_IFMT) != S_IFREG &&
	    ioctl(gpt->fd, DIOCGWEDGEINFO, &dkw) != -1) {
		if (entry > 0)
			/* wedges and indexes are mutually exclusive */
			goto usage;
		start = dkw.dkw_offset;
		size = dkw.dkw_size;
		ngpt = gpt_open(dkw.dkw_parent, gpt->flags, gpt->verbose,
		    0, 0, 0);
		if (ngpt == NULL)
			goto cleanup;
	}
#endif
	if (biosboot(ngpt, start, size, entry, label, bootpath, active) == -1)
		goto cleanup;
	if (ngpt != gpt)
		gpt_close(ngpt);
	return 0;
usage:
	usage();
cleanup:
	if (ngpt != gpt)
		gpt_close(ngpt);
	free(bootpath);
	free(label);
	return -1;
}
