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
__FBSDID("$FreeBSD: src/sbin/gpt/recover.c,v 1.8 2005/08/31 01:47:19 marcel Exp $");
#endif
#ifdef __RCSID
__RCSID("$NetBSD: recover.c,v 1.19 2020/12/13 21:55:25 jnemeth Exp $");
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

static int cmd_recover(gpt_t, int, char *[]);

static const char *recoverhelp[] = {
	"",
};

struct gpt_cmd c_recover = {
	"recover",
	cmd_recover,
	recoverhelp, __arraycount(recoverhelp),
	GPT_SYNC,
};

#define usage() gpt_usage(NULL, &c_recover)

static int
recover_gpt_hdr(gpt_t gpt, int type, off_t last)
{
	const char *name, *origname;
	map_t *dgpt, dtbl, sgpt, stbl __unused;
	struct gpt_hdr *hdr;

	if (gpt_add_hdr(gpt, type, last) == -1)
		return -1;

	switch (type) {
	case MAP_TYPE_PRI_GPT_HDR:
		dgpt = &gpt->gpt;
		dtbl = gpt->tbl;
		sgpt = gpt->tpg;
		stbl = gpt->lbt;
		origname = "secondary";
		name = "primary";
		break;
	case MAP_TYPE_SEC_GPT_HDR:
		dgpt = &gpt->tpg;
		dtbl = gpt->lbt;
		sgpt = gpt->gpt;
		stbl = gpt->tbl;
		origname = "primary";
		name = "secondary";
		break;
	default:
		gpt_warn(gpt, "Bad table type %d", type);
		return -1;
	}

	memcpy((*dgpt)->map_data, sgpt->map_data, gpt->secsz);
	hdr = (*dgpt)->map_data;
	hdr->hdr_lba_self = htole64((uint64_t)(*dgpt)->map_start);
	hdr->hdr_lba_alt = htole64((uint64_t)sgpt->map_start);
	hdr->hdr_lba_table = htole64((uint64_t)dtbl->map_start);
	hdr->hdr_crc_self = 0;
	hdr->hdr_crc_self = htole32(crc32(hdr, le32toh(hdr->hdr_size)));
	if (gpt_write(gpt, *dgpt) == -1) {
		gpt_warnx(gpt, "Writing %s GPT header failed", name);
		return -1;
	}
	gpt_msg(gpt, "Recovered %s GPT header from %s", name, origname);
	return 0;
}

static int
recover_gpt_tbl(gpt_t gpt, int type, off_t start)
{
	const char *name, *origname;
	map_t *dtbl, stbl;

	switch (type) {
	case MAP_TYPE_PRI_GPT_TBL:
		dtbl = &gpt->tbl;
		stbl = gpt->lbt;
		origname = "secondary";
		name = "primary";
		break;
	case MAP_TYPE_SEC_GPT_TBL:
		dtbl = &gpt->lbt;
		stbl = gpt->tbl;
		origname = "primary";
		name = "secondary";
		break;
	default:
		gpt_warn(gpt, "Bad table type %d", type);
		return -1;
	}

	*dtbl = map_add(gpt, start, stbl->map_size, type, stbl->map_data, 0);
	if (*dtbl == NULL) {
		gpt_warnx(gpt, "Adding %s GPT table failed", name);
		return -1;
	}
	if (gpt_write(gpt, *dtbl) == -1) {
		gpt_warnx(gpt, "Writing %s GPT table failed", name);
		return -1;
	}
	gpt_msg(gpt, "Recovered %s GPT table from %s", name, origname);
	return 0;
}

static int
recover(gpt_t gpt)
{
	off_t last = gpt_last(gpt);
	map_t map;
	struct mbr *mbr;

	if (map_find(gpt, MAP_TYPE_MBR) != NULL) {
		gpt_warnx(gpt, "Device contains an MBR");
		return -1;
	}

	gpt->gpt = map_find(gpt, MAP_TYPE_PRI_GPT_HDR);
	gpt->tpg = map_find(gpt, MAP_TYPE_SEC_GPT_HDR);
	gpt->tbl = map_find(gpt, MAP_TYPE_PRI_GPT_TBL);
	gpt->lbt = map_find(gpt, MAP_TYPE_SEC_GPT_TBL);

	if (gpt->gpt == NULL && gpt->tpg == NULL) {
		gpt_warnx(gpt, "No primary or secondary GPT headers, "
		    "can't recover");
		return -1;
	}
	if (gpt->tbl == NULL && gpt->lbt == NULL) {
		gpt_warnx(gpt, "No primary or secondary GPT tables, "
		    "can't recover");
		return -1;
	}

	if (gpt->gpt != NULL &&
	    le64toh(((struct gpt_hdr *)(gpt->gpt->map_data))->hdr_lba_alt) !=
	    (uint64_t)last) {
		gpt_warnx(gpt, "Media size has changed, please use "
		   "'%s resizedisk'", getprogname());
		return -1;
	}

	if (gpt->tbl != NULL && gpt->lbt == NULL) {
		if (recover_gpt_tbl(gpt, MAP_TYPE_SEC_GPT_TBL,
		    last - gpt->tbl->map_size) == -1)
			return -1;
	} else if (gpt->tbl == NULL && gpt->lbt != NULL) {
		if (recover_gpt_tbl(gpt, MAP_TYPE_PRI_GPT_TBL, 2LL) == -1)
			return -1;
	}

	if (gpt->gpt != NULL && gpt->tpg == NULL) {
		if (recover_gpt_hdr(gpt, MAP_TYPE_SEC_GPT_HDR, last) == -1)
			return -1;
	} else if (gpt->gpt == NULL && gpt->tpg != NULL) {
		if (recover_gpt_hdr(gpt, MAP_TYPE_PRI_GPT_HDR, 1LL) == -1)
			return -1;
	}

	/*
	 * Create PMBR if it doesn't already exist.
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
		gpt_create_pmbr_part(mbr->mbr_part, last, 0);

		map = map_add(gpt, 0LL, 1LL, MAP_TYPE_PMBR, mbr, 1);
		if (gpt_write(gpt, map) == -1) {
			gpt_warn(gpt, "Can't write PMBR");
			return -1;
		}
		gpt_msg(gpt,
		    "Recreated PMBR (you may need to rerun 'gpt biosboot'");
	}
	return 0;
}

static int
cmd_recover(gpt_t gpt, int argc, char *argv[])
{
	int ch;

	while ((ch = getopt(argc, argv, "r")) != -1) {
		switch(ch) {
		default:
			return usage();
		}
	}

	if (argc != optind)
		return usage();

	return recover(gpt);
}
