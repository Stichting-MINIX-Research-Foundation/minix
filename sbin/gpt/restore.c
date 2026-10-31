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
__RCSID("$NetBSD: restore.c,v 1.20 2020/06/07 05:42:25 thorpej Exp $");
#endif

#include <sys/types.h>
#include <sys/bootblock.h>
#include <sys/disklabel_gpt.h>

#include <err.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <prop/proplib.h>

#include "map.h"
#include "gpt.h"
#include "gpt_private.h"

static int cmd_restore(gpt_t, int, char *[]);

static const char *restorehelp[] = {
	"[-F] [-i infile]",
};

struct gpt_cmd c_restore = {
	"restore",
	cmd_restore,
	restorehelp, __arraycount(restorehelp),
	GPT_SYNC,
};

#define usage() gpt_usage(NULL, &c_restore)

#define PROP_ERR(x)     if (!(x)) {		\
	gpt_warnx(gpt, "proplib failure");	\
	return -1;				\
}

static int
restore_mbr(gpt_t gpt, struct mbr *mbr, prop_dictionary_t mbr_dict, off_t last)
{
	unsigned int i;
	struct mbr_part *part;

	PROP_ERR(prop_dictionary_get_uint(mbr_dict, "index", &i));
	part = &mbr->mbr_part[i];

	PROP_ERR(prop_dictionary_get_uint8(mbr_dict, "flag", &part->part_flag));
	PROP_ERR(prop_dictionary_get_uint8(mbr_dict, "start_head",
					   &part->part_shd));
	PROP_ERR(prop_dictionary_get_uint8(mbr_dict, "start_sector",
					   &part->part_ssect ));
	PROP_ERR(prop_dictionary_get_uint8(mbr_dict, "start_cylinder",
					   &part->part_scyl));
	PROP_ERR(prop_dictionary_get_uint8(mbr_dict, "type",
					   &part->part_typ));
	PROP_ERR(prop_dictionary_get_uint8(mbr_dict, "end_head",
					   &part->part_ehd));
	PROP_ERR(prop_dictionary_get_uint8(mbr_dict, "end_sector",
					   &part->part_esect));
	PROP_ERR(prop_dictionary_get_uint8(mbr_dict, "end_cylinder",
					   &part->part_ecyl));
	PROP_ERR(prop_dictionary_get_uint16(mbr_dict, "lba_start_low",
					    &part->part_start_lo));
	part->part_start_lo = htole16(part->part_start_lo);
	PROP_ERR(prop_dictionary_get_uint16(mbr_dict, "lba_start_high",
					    &part->part_start_hi));
	part->part_start_hi = htole16(part->part_start_hi);

	/* adjust PMBR size to size of device */
	if (part->part_typ == MBR_PTYPE_PMBR) {
		if (last > 0xffffffff) {
			mbr->mbr_part[0].part_size_lo = htole16(0xffff);
			mbr->mbr_part[0].part_size_hi = htole16(0xffff);
		} else {
			mbr->mbr_part[0].part_size_lo = htole16((uint16_t)last);
			mbr->mbr_part[0].part_size_hi = htole16(
			    (uint16_t)(last >> 16));
		}
	} else {
		PROP_ERR(prop_dictionary_get_uint16(mbr_dict, "lba_size_low",
						    &part->part_size_lo));
		part->part_size_lo = htole16(part->part_size_lo);
		PROP_ERR(prop_dictionary_get_uint16(mbr_dict, "lba_size_high",
						    &part->part_size_hi));
		part->part_size_hi = htole16(part->part_size_hi);
	}
	return 0;
}

static int
restore_ent(gpt_t gpt, prop_dictionary_t gpt_dict, void *secbuf, u_int gpt_size,
    u_int entries)
{
	unsigned int i;
	struct gpt_ent ent;
	const char *s;

	memset(&ent, 0, sizeof(ent));
	PROP_ERR(prop_dictionary_get_string(gpt_dict, "type", &s));
	if (gpt_uuid_parse(s, ent.ent_type) != 0) {
		gpt_warnx(gpt, "%s: not able to convert to an UUID", s);
		return -1;
	}
	PROP_ERR(prop_dictionary_get_string(gpt_dict, "guid", &s));
	if (gpt_uuid_parse(s, ent.ent_guid) != 0) {
		gpt_warnx(gpt, "%s: not able to convert to an UUID", s);
		return -1;
	}
	PROP_ERR(prop_dictionary_get_uint64(gpt_dict, "start",
					    &ent.ent_lba_start));
	ent.ent_lba_start = htole64(ent.ent_lba_start);
	PROP_ERR(prop_dictionary_get_uint64(gpt_dict, "end",
					    &ent.ent_lba_end));
	ent.ent_lba_end = htole64(ent.ent_lba_end);
	PROP_ERR(prop_dictionary_get_uint64(gpt_dict, "attributes",
					    &ent.ent_attr));
	ent.ent_attr = htole64(ent.ent_attr);

	if (prop_dictionary_get_string(gpt_dict, "name", &s)) {
		utf8_to_utf16((const uint8_t *)s, ent.ent_name,
		    __arraycount(ent.ent_name));
	}
	PROP_ERR(prop_dictionary_get_uint(gpt_dict, "index", &i));
	if (i > entries) {
		gpt_warnx(gpt, "Entity index out of bounds %u > %u\n",
		    i, entries);
		return -1;
	}
	memcpy((char *)secbuf + gpt->secsz + ((i - 1) * sizeof(ent)),
	    &ent, sizeof(ent));
	return 0;
}

static int
restore(gpt_t gpt, const char *infile, int force)
{
	gpt_uuid_t gpt_guid, uuid;
	off_t firstdata, last, lastdata, gpe_start, gpe_end;
	map_t map;
	struct mbr *mbr;
	struct gpt_hdr *hdr;
	unsigned int i, gpt_size;
	prop_dictionary_t props, gpt_dict, mbr_dict, type_dict;
	prop_object_iterator_t propiter;
	prop_data_t propdata;
	prop_array_t mbr_array, gpt_array;
	prop_number_t propnum;
	unsigned int entries;
	const char *s;
	void *secbuf = NULL;
	int rv = -1;

	last = gpt->mediasz / gpt->secsz - 1LL;

	if (map_find(gpt, MAP_TYPE_PRI_GPT_HDR) != NULL ||
	    map_find(gpt, MAP_TYPE_SEC_GPT_HDR) != NULL) {
		if (!force) {
			gpt_warnx(gpt, "Device contains a GPT");
			return -1;
		}
	}
	map = map_find(gpt, MAP_TYPE_MBR);
	if (map != NULL) {
		if (!force) {
			gpt_warnx(gpt, "Device contains an MBR");
			return -1;
		}
		/* Nuke the MBR in our internal map. */
		map->map_type = MAP_TYPE_UNUSED;
	}

	props = prop_dictionary_internalize_from_file(
	    strcmp(infile, "-") == 0 ? "/dev/stdin" : infile);
	if (props == NULL) {
		gpt_warnx(gpt, "Unable to read/parse backup file");
		return -1;
	}

	propnum = prop_dictionary_get(props, "sector_size");
	PROP_ERR(propnum);
	if (!prop_number_equals_signed(propnum, gpt->secsz)) {
		gpt_warnx(gpt, "Sector size does not match backup");
		prop_object_release(props);
		return -1;
	}

	gpt_dict = prop_dictionary_get(props, "GPT_HDR");
	PROP_ERR(gpt_dict);

	propnum = prop_dictionary_get(gpt_dict, "revision");
	PROP_ERR(propnum);
	if (!prop_number_equals_unsigned(propnum, 0x10000)) {
		gpt_warnx(gpt, "backup is not revision 1.0");
		prop_object_release(gpt_dict);
		prop_object_release(props);
		return -1;
	}

	PROP_ERR(prop_dictionary_get_uint(gpt_dict, "entries", &entries));

	gpt_size = (u_int)(entries * sizeof(struct gpt_ent) / gpt->secsz);
	if (gpt_size * sizeof(struct gpt_ent) % gpt->secsz)
		gpt_size++;

	PROP_ERR(prop_dictionary_get_string(gpt_dict, "guid", &s));
	if (gpt_uuid_parse(s, gpt_guid) != 0) {
		gpt_warnx(gpt, "%s: not able to convert to an UUID", s);
		goto out;
	}
	firstdata = gpt_size + 2;		/* PMBR and GPT header */
	lastdata = last - gpt_size - 1;		/* alt. GPT table and header */

	type_dict = prop_dictionary_get(props, "GPT_TBL");
	PROP_ERR(type_dict);
	gpt_array = prop_dictionary_get(type_dict, "gpt_array");
	PROP_ERR(gpt_array);
	propiter = prop_array_iterator(gpt_array);
	PROP_ERR(propiter);
	while ((gpt_dict = prop_object_iterator_next(propiter)) != NULL) {
		PROP_ERR(prop_dictionary_get_string(gpt_dict, "type", &s));
		if (gpt_uuid_parse(s, uuid) != 0) {
			gpt_warnx(gpt, "%s: not able to convert to an UUID", s);
			goto out;
		}
		if (gpt_uuid_is_nil(uuid))
			continue;
		PROP_ERR(prop_dictionary_get_int64(gpt_dict, "start",
						   &gpe_start));
		PROP_ERR(prop_dictionary_get_int64(gpt_dict, "end",
						   &gpe_end));
		if (gpe_start < firstdata || gpe_end > lastdata) {
			gpt_warnx(gpt, "Backup GPT doesn't fit");
			goto out;
		}
	}
	prop_object_iterator_release(propiter);

	/* GPT TABLE + GPT HEADER */
	if ((secbuf = calloc(gpt_size + 1, gpt->secsz)) == NULL) {
		gpt_warn(gpt, "not enough memory to create a sector buffer");
		goto out;
	}

	if (lseek(gpt->fd, 0LL, SEEK_SET) == -1) {
		gpt_warn(gpt, "Can't seek to beginning");
		goto out;
	}
	for (i = 0; i < firstdata; i++) {
		if (write(gpt->fd, secbuf, gpt->secsz) != (ssize_t)gpt->secsz) {
			gpt_warn(gpt, "Error writing");
			goto out;
		}
	}
	if (lseek(gpt->fd, (lastdata + 1) * gpt->secsz, SEEK_SET) == -1) {
		gpt_warn(gpt, "Can't seek to end");
		goto out;
	}
	for (i = (u_int)(lastdata + 1); i <= (u_int)last; i++) {
		if (write(gpt->fd, secbuf, gpt->secsz) != (ssize_t)gpt->secsz) {
			gpt_warn(gpt, "Error writing");
			goto out;
		}
	}

	mbr = secbuf;
	type_dict = prop_dictionary_get(props, "MBR");
	PROP_ERR(type_dict);
	propdata = prop_dictionary_get(type_dict, "code");
	PROP_ERR(propdata);
	memcpy(mbr->mbr_code, prop_data_value(propdata),
	    sizeof(mbr->mbr_code));
	mbr_array = prop_dictionary_get(type_dict, "mbr_array");
	PROP_ERR(mbr_array);
	propiter = prop_array_iterator(mbr_array);
	PROP_ERR(propiter);
	while ((mbr_dict = prop_object_iterator_next(propiter)) != NULL) {
		if (restore_mbr(gpt, mbr, mbr_dict, last) == -1)
			goto out;
	}

	prop_object_iterator_release(propiter);
	mbr->mbr_sig = htole16(MBR_SIG);
	if (lseek(gpt->fd, 0LL, SEEK_SET) == -1 ||
	    write(gpt->fd, mbr, gpt->secsz) != (ssize_t)gpt->secsz) {
		gpt_warn(gpt, "Unable to seek/write MBR");
		return -1;
	}

	propiter = prop_array_iterator(gpt_array);
	PROP_ERR(propiter);

	while ((gpt_dict = prop_object_iterator_next(propiter)) != NULL) {
		if (restore_ent(gpt, gpt_dict, secbuf, gpt_size, entries) == -1)
			goto out;
	}
	prop_object_iterator_release(propiter);

	size_t len = gpt_size * gpt->secsz;
	if (lseek(gpt->fd, 2 * gpt->secsz, SEEK_SET) == -1 ||
	    write(gpt->fd, (char *)secbuf + gpt->secsz, len) != (ssize_t) len) {
		gpt_warn(gpt, "Unable to write primary GPT");
		goto out;
	}

	if (lseek(gpt->fd, (lastdata + 1) * gpt->secsz, SEEK_SET) == -1 ||
	    write(gpt->fd, (char *)secbuf + gpt->secsz, len) != (ssize_t) len) {
		gpt_warn(gpt, "Unable to write secondary GPT");
		goto out;
	}

	memset(secbuf, 0, gpt->secsz);
	hdr = secbuf;
	memcpy(hdr->hdr_sig, GPT_HDR_SIG, sizeof(hdr->hdr_sig));
	hdr->hdr_revision = htole32(GPT_HDR_REVISION);
	hdr->hdr_size = htole32(GPT_HDR_SIZE);
	hdr->hdr_lba_self = htole64(GPT_HDR_BLKNO);
	hdr->hdr_lba_alt = htole64((uint64_t)last);
	hdr->hdr_lba_start = htole64((uint64_t)firstdata);
	hdr->hdr_lba_end = htole64((uint64_t)lastdata);
	gpt_uuid_copy(hdr->hdr_guid, gpt_guid);
	hdr->hdr_lba_table = htole64(2);
	hdr->hdr_entries = htole32(entries);
	hdr->hdr_entsz = htole32(sizeof(struct gpt_ent));
	hdr->hdr_crc_table = htole32(crc32((char *)secbuf + gpt->secsz, len));
	hdr->hdr_crc_self = htole32(crc32(hdr, GPT_HDR_SIZE));
	if (lseek(gpt->fd, gpt->secsz, SEEK_SET) == -1 ||
	    write(gpt->fd, hdr, gpt->secsz) != (ssize_t)gpt->secsz) {
		gpt_warn(gpt, "Unable to write primary header");
		goto out;
	}

	hdr->hdr_lba_self = htole64((uint64_t)last);
	hdr->hdr_lba_alt = htole64(GPT_HDR_BLKNO);
	hdr->hdr_lba_table = htole64((uint64_t)(lastdata + 1));
	hdr->hdr_crc_self = 0;
	hdr->hdr_crc_self = htole32(crc32(hdr, GPT_HDR_SIZE));
	if (lseek(gpt->fd, last * gpt->secsz, SEEK_SET) == -1 ||
	    write(gpt->fd, hdr, gpt->secsz) != (ssize_t)gpt->secsz) {
		gpt_warn(gpt, "Unable to write secondary header");
		goto out;
	}
	rv = 0;

out:
	free(secbuf);
	prop_object_release(props);
	return rv;
}

static int
cmd_restore(gpt_t gpt, int argc, char *argv[])
{
	int ch;
	int force = 0;
	const char *infile = "-";

	while ((ch = getopt(argc, argv, "Fi:")) != -1) {
		switch(ch) {
		case 'i':
			infile = optarg;
			break;
		case 'F':
			force = 1;
			break;
		default:
			return usage();
		}
	}

	if (argc != optind)
		return usage();

	return restore(gpt, infile, force);
}
