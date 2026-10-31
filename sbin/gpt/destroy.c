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
__FBSDID("$FreeBSD: src/sbin/gpt/destroy.c,v 1.6 2005/08/31 01:47:19 marcel Exp $");
#endif
#ifdef __RCSID
__RCSID("$NetBSD: destroy.c,v 1.13 2019/10/16 19:03:53 jnemeth Exp $");
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

static int cmd_destroy(gpt_t, int, char *[]);

static const char *destroyhelp[] = {
	"[-r]",
};

struct gpt_cmd c_destroy = {
	"destroy",
	cmd_destroy,
	destroyhelp, __arraycount(destroyhelp),
	GPT_SYNC,
};

#define usage() gpt_usage(NULL, &c_destroy)


static int
destroy(gpt_t gpt, int force, int recoverable)
{
	map_t pri_hdr, sec_hdr, pmbr;

	pri_hdr = map_find(gpt, MAP_TYPE_PRI_GPT_HDR);
	sec_hdr = map_find(gpt, MAP_TYPE_SEC_GPT_HDR);
	pmbr = map_find(gpt, MAP_TYPE_PMBR);

	if (pri_hdr == NULL && sec_hdr == NULL) {
		gpt_warnx(gpt, "Device doesn't contain a GPT");
		return -1;
	}

	if (recoverable && sec_hdr == NULL) {
		gpt_warnx(gpt, "Recoverability not possible");
		return -1;
	}

	if (pri_hdr != NULL) {
		memset(pri_hdr->map_data, 0, gpt->secsz);
		if (gpt_write(gpt, pri_hdr) == -1) {
			gpt_warnx(gpt, "Error writing primary header");
			return -1;
		}
	}

	if (!recoverable && sec_hdr != NULL) {
		memset(sec_hdr->map_data, 0, gpt->secsz);
		if (gpt_write(gpt, sec_hdr) == -1) {
			gpt_warnx(gpt, "Error writing backup header");
			return -1;
		}
	}

	if (!recoverable && pmbr != NULL) {
		memset(pmbr->map_data, 0, gpt->secsz);
		if (gpt_write(gpt, pmbr) == -1) {
			gpt_warnx(gpt, "Error deleting PMBR");
			return -1;
		}
	}

	return 0;
}

static int
cmd_destroy(gpt_t gpt, int argc, char *argv[])
{
	int ch;
	int recoverable = 0;

	while ((ch = getopt(argc, argv, "fr")) != -1) {
		switch(ch) {
		case 'f':
			break;
		case 'r':
			recoverable = 1;
			break;
		default:
			return usage();
		}
	}

	if (argc != optind)
		return usage();

	return destroy(gpt, 0, recoverable);
}
