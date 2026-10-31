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
__FBSDID("$FreeBSD: src/sbin/gpt/create.c,v 1.11 2005/08/31 01:47:19 marcel Exp $");
#endif
#ifdef __RCSID
__RCSID("$NetBSD: create.c,v 1.23 2016/09/23 19:36:50 christos Exp $");
#endif

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/bootblock.h>

#include <err.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "map.h"
#include "gpt.h"
#include "gpt_private.h"

static int cmd_create(gpt_t, int, char *[]);

static const char *createhelp[] = {
	"[-AfP] [-p partitions]",
};

struct gpt_cmd c_create = {
	"create",
	cmd_create,
	createhelp, __arraycount(createhelp),
	0,
};

#define usage() gpt_usage(NULL, &c_create)


static int
create(gpt_t gpt, u_int parts, int force, int primary_only, int active)
{
	off_t last = gpt_last(gpt);
	map_t map;
	struct mbr *mbr;

	map = map_find(gpt, MAP_TYPE_MBR);
	if (map != NULL) {
		if (!force) {
			gpt_warnx(gpt, "Device contains a MBR");
			return -1;
		}
		/* Nuke the MBR in our internal map. */
		map->map_type = MAP_TYPE_UNUSED;
	}

	/*
	 * Create PMBR.
	 */
	if (map_find(gpt, MAP_TYPE_PMBR) == NULL) {
		if (map_free(gpt, 0LL, 1LL) == 0) {
			gpt_warnx(gpt, "No room for the PMBR");
			return -1;
		}
		mbr = gpt_read(gpt, 0LL, 1);
		if (mbr == NULL) {
			gpt_warnx(gpt, "Error reading MBR");
			return -1;
		}
		memset(mbr, 0, sizeof(*mbr));
		mbr->mbr_sig = htole16(MBR_SIG);
		gpt_create_pmbr_part(mbr->mbr_part, last, active);

		map = map_add(gpt, 0LL, 1LL, MAP_TYPE_PMBR, mbr, 1);
		if (gpt_write(gpt, map) == -1) {
			gpt_warn(gpt, "Can't write PMBR");
			return -1;
		}
	}

	if (gpt_create(gpt, last, parts, primary_only) == -1)
		return -1;

	if (gpt_write_primary(gpt) == -1)
		return -1;

	if (!primary_only && gpt_write_backup(gpt) == -1)
		return -1;

	return 0;
}

static int
cmd_create(gpt_t gpt, int argc, char *argv[])
{
	int ch;
	int active = 0;
	int force = 0;
	int primary_only = 0;
	u_int parts = 0;

	while ((ch = getopt(argc, argv, "AfPp:")) != -1) {
		switch(ch) {
		case 'A':
			active = 1;
			break;
		case 'f':
			force = 1;
			break;
		case 'P':
			primary_only = 1;
			break;
		case 'p':
			if (gpt_uint_get(gpt, &parts) == -1)
				return -1;
			break;
		default:
			return usage();
		}
	}
	if (parts == 0)
		parts = 128;

	if (argc != optind)
		return usage();

	return create(gpt, parts, force, primary_only, active);
}
