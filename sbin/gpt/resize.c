/*-
 * Copyright (c) 2002 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
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

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <sys/cdefs.h>
#ifdef __FBSDID
__FBSDID("$FreeBSD: src/sbin/gpt/add.c,v 1.14 2006/06/22 22:05:28 marcel Exp $");
#endif
#ifdef __RCSID
__RCSID("$NetBSD: resize.c,v 1.25 2020/05/24 14:42:44 jmcneill Exp $");
#endif

#include <sys/types.h>

#include <err.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "map.h"
#include "gpt.h"
#include "gpt_private.h"

static int cmd_resize(gpt_t, int, char *[]);

static const char *resizehelp[] = {
	"[-i index | -b blocknr] [-a alignment] [-s size] [-q]",
};

struct gpt_cmd c_resize = {
	"resize",
	cmd_resize,
	resizehelp, __arraycount(resizehelp),
	GPT_SYNC,
};

#define usage() gpt_usage(NULL, &c_resize)

static int
resize(gpt_t gpt, u_int entry, off_t alignment, off_t sectors, off_t size, bool quiet)
{
	map_t map;
	struct gpt_hdr *hdr;
	struct gpt_ent *ent;
	unsigned int i;
	off_t alignsecs, newsize, oldsize;
	uint64_t end;
	

	if ((hdr = gpt_hdr(gpt)) == NULL)
		return -1;

	i = entry - 1;
	ent = gpt_ent_primary(gpt, i);
	if (gpt_uuid_is_nil(ent->ent_type)) {
		gpt_warnx(gpt, "Entry at index %u is unused", entry);
		return -1;
	}

	alignsecs = alignment / gpt->secsz;

	for (map = map_first(gpt); map != NULL; map = map->map_next) {
		if (entry == map->map_index)
			break;
	}
	if (map == NULL) {
		gpt_warnx(gpt, "Could not find map entry corresponding "
		    "to index");
		return -1;
	}

	if (sectors > 0 && sectors == map->map_size)
		if (alignment == 0 ||
		    (alignment > 0 && sectors % alignsecs == 0)) {
			/* nothing to do */
			if (!quiet)
				gpt_warnx(gpt,
				    "partition does not need resizing");
			return 0;
		}

	oldsize = map->map_size;
	newsize = map_resize(gpt, map, sectors, alignsecs);
	if (newsize == -1)
		return -1;

	if (oldsize == newsize) {
		/* Nothing to do */
		if (!quiet)
			gpt_warnx(gpt,
			    "partition does not need resizing");
		return 0;
	}

	end = htole64((uint64_t)(map->map_start + newsize - 1LL));
	ent->ent_lba_end = end;

	if (gpt_write_primary(gpt) == -1)
		return -1;

	ent = gpt_ent(gpt->gpt, gpt->lbt, i);
	ent->ent_lba_end = end;

	if (gpt_write_backup(gpt) == -1)
		return -1;

	gpt_msg(gpt, "Partition %d resized: %" PRIu64 " %" PRIu64, entry,
	    map->map_start, newsize);

	return 0;
}

static int
cmd_resize(gpt_t gpt, int argc, char *argv[])
{
	int ch;
	off_t alignment = 0, sectors, start = 0, size = 0;
	unsigned int entry = 0;
	map_t m;
	bool quiet = false;

	while ((ch = getopt(argc, argv, GPT_AIS "b:q")) != -1) {
		if (ch == 'b')
			gpt_human_get(gpt, &start);
		else if (ch == 'q')
			quiet = true;
		else if (gpt_add_ais(gpt, &alignment, &entry, &size, ch) == -1)
			return usage();
	}

	if (argc != optind)
		return usage();

	if (start > 0) {
		for (m = map_first(gpt); m != NULL; m = m->map_next) {
			if (m->map_type != MAP_TYPE_GPT_PART ||
			    m->map_index < 1)
				continue;
			if (start != m->map_start)
				continue;
			entry = m->map_index;
			break;
		}
	}

	if ((sectors = gpt_check_ais(gpt, alignment, entry, size)) == -1)
		return -1;

	return resize(gpt, entry, alignment, sectors, size, quiet);
}
