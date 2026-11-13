/*-
 * Copyright (c) 2015 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by John Nemeth
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
__RCSID("$NetBSD: header.c,v 1.10 2019/03/14 10:26:10 mlelstv Exp $");
#endif

#include <sys/types.h>

#include <err.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "map.h"
#include "gpt.h"
#include "gpt_private.h"

static int cmd_header(gpt_t, int, char *[]);

static const char *headerhelp[] = {
	"",
};

struct gpt_cmd c_header = {
	"header",
	cmd_header,
	headerhelp, __arraycount(headerhelp),
	GPT_READONLY,
};

#define usage() gpt_usage(NULL, &c_header)

static int
header(gpt_t gpt)
{
	unsigned int revision;
	map_t map;
	struct gpt_hdr *hdr;
	char buf[128];

	gpt_show_num("Media Size", (uintmax_t)gpt->mediasz);
	printf("Sector Size: %u\n", gpt->secsz);

	gpt_show_num("Number of Sectors",
	    (uintmax_t)(gpt->mediasz / gpt->secsz));

	printf("\nHeader Information:\n");

	map = map_find(gpt, MAP_TYPE_PRI_GPT_HDR);
	if (map == NULL) {
		printf("- GPT Header not found\n");
		return -1;
	}

	hdr = map->map_data;
	revision = le32toh(hdr->hdr_revision);
	printf("- GPT Header Revision: %u.%u\n", revision >> 16,
	     revision & 0xffff);
	gpt_show_num("- First Data Sector", le64toh(hdr->hdr_lba_start));
	gpt_show_num("- Last Data Sector", le64toh(hdr->hdr_lba_end));
	gpt_uuid_snprintf(buf, sizeof(buf), "%d", hdr->hdr_guid);
	printf("- Media GUID: %s\n", buf);
	printf("- Number of GPT Entries: %u\n", le32toh(hdr->hdr_entries));
	return 0;
}

static int
cmd_header(gpt_t gpt, int argc, char *argv[])
{
	if (argc != optind)
		return usage();

	return header(gpt);
}
