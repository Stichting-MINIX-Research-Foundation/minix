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
__FBSDID("$FreeBSD: src/sbin/gpt/show.c,v 1.14 2006/06/22 22:22:32 marcel Exp $");
#endif
#ifdef __RCSID
__RCSID("$NetBSD: show.c,v 1.44 2019/03/26 11:23:55 martin Exp $");
#endif

#include <sys/bootblock.h>
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

static int cmd_show(gpt_t, int, char *[]);

static const char *showhelp[] = {
	"[-aglu] [-i index]",
};

#define SHOW_UUID  1
#define SHOW_GUID  2
#define SHOW_LABEL 4
#define SHOW_ALL   8

struct gpt_cmd c_show = {
	"show",
	cmd_show,
	showhelp, __arraycount(showhelp),
	GPT_READONLY,
};

#define usage() gpt_usage(NULL, &c_show)

static void
print_part_type(int map_type, int flags, void *map_data, off_t map_start)
{
	off_t start;
	map_t p;
	struct mbr *mbr;
	struct gpt_ent *ent;
	unsigned int i;
	char buf[128], *b = buf;
	uint8_t utfbuf[__arraycount(ent->ent_name) * 3 + 1];

	switch (map_type) {
	case MAP_TYPE_UNUSED:
		printf("Unused");
		break;
	case MAP_TYPE_MBR:
		if (map_start != 0)
			printf("Extended ");
		printf("MBR");
		break;
	case MAP_TYPE_PRI_GPT_HDR:
		printf("Pri GPT header");
		break;
	case MAP_TYPE_SEC_GPT_HDR:
		printf("Sec GPT header");
		break;
	case MAP_TYPE_PRI_GPT_TBL:
		printf("Pri GPT table");
		break;
	case MAP_TYPE_SEC_GPT_TBL:
		printf("Sec GPT table");
		break;
	case MAP_TYPE_MBR_PART:
		p = map_data;
		if (p->map_start != 0)
			printf("Extended ");
		printf("MBR part ");
		mbr = p->map_data;
		for (i = 0; i < 4; i++) {
			start = le16toh(mbr->mbr_part[i].part_start_hi);
			start = (start << 16) +
			    le16toh(mbr->mbr_part[i].part_start_lo);
			if (map_start == p->map_start + start)
				break;
		}
		if (i == 4) {
			/* wasn't there */
			printf("[partition not found?]");
		} else {
			printf("%d%s", mbr->mbr_part[i].part_typ,
			    mbr->mbr_part[i].part_flag == 0x80 ?
			    " (active)" : "");
		}
		break;
	case MAP_TYPE_GPT_PART:
		printf("GPT part ");
		ent = map_data;
		if (flags & SHOW_LABEL) {
			utf16_to_utf8(ent->ent_name,
			    __arraycount(ent->ent_name), utfbuf,
			    __arraycount(utfbuf));
			b = (char *)utfbuf;
		} else if (flags & SHOW_GUID) {
			gpt_uuid_snprintf( buf, sizeof(buf), "%d",
			    ent->ent_guid);
		} else if (flags & SHOW_UUID) {
			gpt_uuid_snprintf(buf, sizeof(buf),
			    "%d", ent->ent_type);
		} else {
			gpt_uuid_snprintf(buf, sizeof(buf), "%ls",
			    ent->ent_type);
		}
		printf("- %s", b);
		break;
	case MAP_TYPE_PMBR:
		printf("PMBR");
		mbr = map_data;
		if (mbr->mbr_part[0].part_typ == MBR_PTYPE_PMBR &&
		    mbr->mbr_part[0].part_flag == 0x80)
			    printf(" (active)");
		break;
	default:
		printf("Unknown %#x", map_type);
		break;
	}
}

static int
show(gpt_t gpt, int xshow)
{
	map_t m;

	printf("  %*s", gpt->lbawidth, "start");
	printf("  %*s", gpt->lbawidth, "size");
	printf("  index  contents\n");

	m = map_first(gpt);
	while (m != NULL) {
		printf("  %*llu", gpt->lbawidth, (long long)m->map_start);
		printf("  %*llu", gpt->lbawidth, (long long)m->map_size);
		putchar(' ');
		putchar(' ');
		if (m->map_index > 0)
			printf("%5d", m->map_index);
		else
			printf("     ");
		putchar(' ');
		putchar(' ');
		print_part_type(m->map_type, xshow, m->map_data, m->map_start);
		putchar('\n');
		m = m->map_next;
	}
	return 0;
}

static void
gpt_show_sec_num(const char *prompt, int64_t secsize, off_t num)
{
#ifdef HN_AUTOSCALE
	char human_num[5];
	if (humanize_number(human_num, sizeof(human_num),
	    (int64_t)num*secsize,
	    "", HN_AUTOSCALE, HN_NOSPACE|HN_B) < 0)
		human_num[0] = '\0';
#endif
	printf("%s: %" PRIu64, prompt, (uint64_t)num);
#ifdef HN_AUTOSCALE
	if (human_num[0] != '\0')
		printf(" (%s)", human_num);
#endif
	printf("\n");
}

static int
show_one(gpt_t gpt, unsigned int entry)
{
	map_t m;
	struct gpt_ent *ent;
	char s1[128], s2[128];
	uint8_t utfbuf[__arraycount(ent->ent_name) * 3 + 1];

	for (m = map_first(gpt); m != NULL; m = m->map_next)
		if (entry == m->map_index)
			break;
	if (m == NULL) {
		gpt_warnx(gpt, "Could not find index %d", entry);
		return -1;
	}
	ent = m->map_data;

	printf("Details for index %d:\n", entry);
	gpt_show_sec_num("Start", gpt->secsz, m->map_start);
	gpt_show_sec_num("Size", gpt->secsz, m->map_size);

	gpt_uuid_snprintf(s1, sizeof(s1), "%s", ent->ent_type);
	gpt_uuid_snprintf(s2, sizeof(s2), "%d", ent->ent_type);
	if (strcmp(s1, s2) == 0)
		strlcpy(s1, "unknown", sizeof(s1));
	printf("Type: %s (%s)\n", s1, s2);

	gpt_uuid_snprintf(s2, sizeof(s1), "%d", ent->ent_guid);
	printf("GUID: %s\n", s2);

	utf16_to_utf8(ent->ent_name, __arraycount(ent->ent_name), utfbuf,
	    __arraycount(utfbuf));
	printf("Label: %s\n", (char *)utfbuf);

	printf("Attributes: ");
	if (ent->ent_attr == 0) {
		printf("None\n");
	} else  {	
		char buf[1024];
		printf("%s\n", gpt_attr_list(buf, sizeof(buf), ent->ent_attr));
	}

	return 0;
}

static int
show_all(gpt_t gpt)
{
	map_t m;
	struct gpt_ent *ent;
	char s1[128], s2[128];
#ifdef HN_AUTOSCALE
	char human_num[8];
#endif
	uint8_t utfbuf[__arraycount(ent->ent_name) * 3 + 1];
#define PFX "                                 "

	printf("  %*s", gpt->lbawidth, "start");
	printf("  %*s", gpt->lbawidth, "size");
	printf("  index  contents\n");

	m = map_first(gpt);
	while (m != NULL) {
		printf("  %*llu", gpt->lbawidth, (long long)m->map_start);
		printf("  %*llu", gpt->lbawidth, (long long)m->map_size);
		putchar(' ');
		putchar(' ');
		if (m->map_index > 0) {
			printf("%5d  ", m->map_index);
			print_part_type(m->map_type, 0, m->map_data,
			    m->map_start);
			putchar('\n');

			ent = m->map_data;

			gpt_uuid_snprintf(s1, sizeof(s1), "%s", ent->ent_type);
			gpt_uuid_snprintf(s2, sizeof(s2), "%d", ent->ent_type);
			if (strcmp(s1, s2) == 0)
				strlcpy(s1, "unknown", sizeof(s1));
			printf(PFX "Type: %s\n", s1);
			printf(PFX "TypeID: %s\n", s2);

			gpt_uuid_snprintf(s2, sizeof(s1), "%d", ent->ent_guid);
			printf(PFX "GUID: %s\n", s2);

			printf(PFX "Size: ");
#ifdef HN_AUTOSCALE
			if (humanize_number(human_num, sizeof(human_num),
			    (int64_t)(m->map_size * gpt->secsz),
			    "", HN_AUTOSCALE, HN_B) < 0) {
#endif
				printf("%ju",
				    (int64_t)(m->map_size * gpt->secsz));
#ifdef HN_AUTOSCALE
			} else {
				printf("%s", human_num);
			}
#endif
			putchar('\n');

			utf16_to_utf8(ent->ent_name,
			    __arraycount(ent->ent_name), utfbuf,
			    __arraycount(utfbuf));
			printf(PFX "Label: %s\n", (char *)utfbuf);

			printf(PFX "Attributes: ");
			if (ent->ent_attr == 0) {
				printf("None\n");
			} else  {	
				char buf[1024];

				printf("%s\n", gpt_attr_list(buf, sizeof(buf),
				    ent->ent_attr));
			}
		} else {
			printf("       ");
			print_part_type(m->map_type, 0, m->map_data,
			    m->map_start);
			putchar('\n');
		}
		m = m->map_next;
	}
	return 0;
}

static int
cmd_show(gpt_t gpt, int argc, char *argv[])
{
	int ch;
	int xshow = 0;
	unsigned int entry = 0;
	off_t start = 0;
	map_t m;

	while ((ch = getopt(argc, argv, "gi:b:lua")) != -1) {
		switch(ch) {
		case 'a':
			xshow |= SHOW_ALL;
			break;
		case 'g':
			xshow |= SHOW_GUID;
			break;
		case 'i':
			if (gpt_uint_get(gpt, &entry) == -1)
				return usage();
			break;
		case 'b':
			if (gpt_human_get(gpt, &start) == -1)
				return usage();
			break;
		case 'l':
			xshow |= SHOW_LABEL;
			break;
		case 'u':
			xshow |= SHOW_UUID;
			break;
		default:
			return usage();
		}
	}

	if (argc != optind)
		return usage();

	if (map_find(gpt, MAP_TYPE_PRI_GPT_HDR) == NULL)
		printf("GPT not found, displaying data from MBR.\n\n");

	if (xshow & SHOW_ALL)
		return show_all(gpt);

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

	return entry > 0 ? show_one(gpt, entry) : show(gpt, xshow);
}
