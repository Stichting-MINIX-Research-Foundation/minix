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
__RCSID("$NetBSD: resizedisk.c,v 1.18 2020/05/24 14:42:44 jmcneill Exp $");
#endif

#include <sys/bootblock.h>
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


static int cmd_resizedisk(gpt_t, int, char *[]);

static const char *resizediskhelp[] = {
	"[-s size] [-q]",
};

struct gpt_cmd c_resizedisk = {
	"resizedisk",
	cmd_resizedisk,
	resizediskhelp, __arraycount(resizediskhelp),
	0,
};

#define usage() gpt_usage(NULL, &c_resizedisk)

/*
 * relocate the secondary GPT based on the following criteria:
 * - size not specified
 *   - disk has not changed size, do nothing
 *   - disk has grown, relocate secondary
 *   - disk has shrunk, create new secondary
 * - size specified
 *   - size is larger then disk or same as current location, do nothing
 *   - relocate or create new secondary
 * - when shrinking, verify that table fits
 */
static int 
resizedisk(gpt_t gpt, off_t sector, off_t size, bool quiet)
{
	map_t mbrmap;
	struct gpt_hdr *hdr;
	struct gpt_ent *ent;
	struct mbr *mbr;
	off_t last, oldloc, newloc, lastdata, gpt_size;
	int i;
	
	last = gpt->mediasz / gpt->secsz - 1;
	lastdata = 0;
	newloc = 0;

	if (sector > last) {
		gpt_warnx(gpt, "specified number of sectors %jd"
		    " is larger then the disk %jd", (uintmax_t)sector,
		    (uintmax_t)last);
		return -1;
	}

        mbrmap = map_find(gpt, MAP_TYPE_PMBR);
        if (mbrmap == NULL || mbrmap->map_start != 0) {
                gpt_warnx(gpt, "No valid PMBR found");
                return -1;
        }
        mbr = mbrmap->map_data;

	gpt->gpt = map_find(gpt, MAP_TYPE_PRI_GPT_HDR);
	if (gpt == NULL) {
		gpt_warnx(gpt, "No primary GPT header; run create or recover");
		return -1;
	}

	gpt->tbl = map_find(gpt, MAP_TYPE_PRI_GPT_TBL);
	if (gpt->tbl == NULL) {
		gpt_warnx(gpt, "No primary GPT table; Run recover");
		return -1;
	}

	hdr = gpt->gpt->map_data;
	oldloc = (off_t)le64toh((uint64_t)hdr->hdr_lba_alt);

	gpt->tpg = map_find(gpt, MAP_TYPE_SEC_GPT_HDR);
	gpt->lbt = map_find(gpt, MAP_TYPE_SEC_GPT_TBL);
	if (gpt->tpg == NULL || gpt->lbt == NULL) {
		if (gpt_gpt(gpt, oldloc, 1) == -1) {
			gpt_warnx(gpt,
			    "Error reading backup GPT information at %#jx",
			    oldloc);
			return -1;
		}
	}

	gpt->tpg = map_find(gpt, MAP_TYPE_SEC_GPT_HDR);
	if (gpt->tpg == NULL) {
		gpt_warnx(gpt, "No secondary GPT header; Run recover");
		return -1;
	}
	gpt->lbt = map_find(gpt, MAP_TYPE_SEC_GPT_TBL);
	if (gpt->lbt == NULL) {
		gpt_warnx(gpt, "No secondary GPT table; Run recover");
		return -1;
	}

	gpt_size = gpt->tbl->map_size;
	if (sector == oldloc) {
		if (!quiet)
			gpt_warnx(gpt, "Device is already the specified size");
		return 0;
	}

	if (sector == 0 && last == oldloc) {
		if (!quiet)
			gpt_warnx(gpt, "Device hasn't changed size");
		return 0;
	}

	for (ent = gpt->tbl->map_data; ent <
	    (struct gpt_ent *)((char *)gpt->tbl->map_data +
	    le32toh(hdr->hdr_entries) * le32toh(hdr->hdr_entsz)); ent++) {
		if (!gpt_uuid_is_nil(ent->ent_type) &&
		    ((off_t)le64toh(ent->ent_lba_end) > lastdata)) {
			lastdata = (off_t)le64toh((uint64_t)ent->ent_lba_end);
		}
	}

	if (sector - gpt_size <= lastdata) {
		gpt_warnx(gpt, "Not enough space at %" PRIu64
		    " for secondary GPT table", sector);
		return -1;
	}

	if (last - gpt_size <= lastdata) {
		gpt_warnx(gpt, "Not enough space for new secondary GPT table");
		return -1;
	}

	if (sector > oldloc)
		newloc = sector;
	if (sector > 0 && sector < oldloc && last >= oldloc)
		newloc = sector;
	if (sector == 0 && last > oldloc)
		newloc = last;

	if (newloc > 0) {
		if (gpt->tpg == NULL) {
			gpt_warnx(gpt, "No secondary GPT header; run recover");
			return -1;
		}
		if (gpt->lbt == NULL) {
			gpt_warnx(gpt, "Run recover");
			return -1;
		}
		gpt->tpg->map_start = newloc;
		gpt->lbt->map_start = newloc - gpt_size;
	} else {
		if (sector > 0)
			newloc = sector;
		else
			newloc = last;

		if (gpt_add_hdr(gpt, MAP_TYPE_SEC_GPT_HDR, newloc) == -1)
			return -1;

		gpt->lbt = map_add(gpt, newloc - gpt_size, gpt_size,
		    MAP_TYPE_SEC_GPT_TBL, gpt->tbl->map_data, 0);
		if (gpt->lbt == NULL) {
			gpt_warn(gpt, "Error adding secondary GPT table");
			return -1;
		}
		memcpy(gpt->tpg->map_data, gpt->gpt->map_data, gpt->secsz);
	}

	hdr = gpt->gpt->map_data;
	hdr->hdr_lba_alt = (uint64_t)gpt->tpg->map_start;
	hdr->hdr_crc_self = 0;
	hdr->hdr_lba_end = htole64((uint64_t)(gpt->lbt->map_start - 1));
	hdr->hdr_crc_self =
	    htole32(crc32(gpt->gpt->map_data, GPT_HDR_SIZE));
	gpt_write(gpt, gpt->gpt);

	hdr = gpt->tpg->map_data;
	hdr->hdr_lba_self = htole64((uint64_t)gpt->tpg->map_start);
	hdr->hdr_lba_alt = htole64((uint64_t)gpt->gpt->map_start);
	hdr->hdr_lba_end = htole64((uint64_t)(gpt->lbt->map_start - 1));
	hdr->hdr_lba_table = htole64((uint64_t)gpt->lbt->map_start);

	if (gpt_write_backup(gpt) == -1)
		return -1;

	for (i = 0; i < 4; i++)
		if (mbr->mbr_part[0].part_typ == MBR_PTYPE_PMBR)
			break;
	if (i == 4) {
		gpt_warnx(gpt, "No valid PMBR partition found");
		return -1;
	}
	if (last > 0xffffffff) {
		mbr->mbr_part[0].part_size_lo = htole16(0xffff);
		mbr->mbr_part[0].part_size_hi = htole16(0xffff);
	} else {
		mbr->mbr_part[0].part_size_lo = htole16((uint16_t)last);
		mbr->mbr_part[0].part_size_hi = htole16((uint16_t)(last >> 16));
	}
	if (gpt_write(gpt, mbrmap) == -1) {
		gpt_warnx(gpt, "Error writing PMBR");
		return -1;
	}

	return 0;
}

static int
cmd_resizedisk(gpt_t gpt, int argc, char *argv[])
{
	int ch;
	off_t sector, size = gpt->mediasz;
	bool quiet = false;

	while ((ch = getopt(argc, argv, "s:q")) != -1) {
		switch(ch) {
		case 's':
			if (gpt_add_ais(gpt, NULL, NULL, &size, ch) == -1)
				return -1;
			break;
		case 'q':
			quiet = true;
			break;
		default:
			return usage();
		}
	}

	if (argc != optind)
		return usage();

	if ((sector = gpt_check_ais(gpt, 0, (u_int)~0, size)) == -1)
		return -1;

	if (--sector == 0) {
		gpt_warnx(gpt, "New size %ju too small", (uintmax_t)size);
		return -1;
	}

	return resizedisk(gpt, sector, size, quiet);
}
