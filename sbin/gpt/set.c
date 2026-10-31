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
__RCSID("$NetBSD: set.c,v 1.15 2019/03/25 20:15:49 martin Exp $");
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

static int cmd_set(gpt_t, int, char *[]);

static const char *sethelp[] = {
	"-a attribute [-i index] [-b startsec]",
	"-N [-i index] [-b startsec]",
	"-l",
};

struct gpt_cmd c_set = {
	"set",
	cmd_set,
	sethelp, __arraycount(sethelp),
	GPT_OPTDEV,
};

#define usage() gpt_usage(NULL, &c_set)

static int
cmd_set(gpt_t gpt, int argc, char *argv[])
{
	int ch;
	unsigned int entry = 0;
	uint64_t attributes = 0, clear = 0;
	off_t start = 0;
	map_t m;

	while ((ch = getopt(argc, argv, "a:b:i:lN")) != -1) {
		switch(ch) {
		case 'a':
			if (gpt == NULL || gpt_attr_get(gpt, &attributes) == -1)
				return usage();
			break;
		case 'b':
			if (gpt == NULL || gpt_human_get(gpt, &start) == -1)
				return usage();
			break;
		case 'i':
			if (gpt == NULL || gpt_uint_get(gpt, &entry) == -1)
				return usage();
			break;
		case 'l':
			gpt_attr_help("\t");
			return 0;
		case 'N':
			clear = ~clear;
			break;
		default:
			return usage();
		}
	}

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

	if (gpt == NULL || argc != optind || (attributes != 0 && clear != 0))
		return usage();

	return gpt_attr_update(gpt, entry, attributes, clear);
}
