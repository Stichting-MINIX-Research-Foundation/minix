/*-
 * Copyright (c) 2004 Marcel Moolenaar
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
__FBSDID("$FreeBSD: src/sbin/gpt/remove.c,v 1.10 2006/10/04 18:20:25 marcel Exp $");
#endif
#ifdef __RCSID
__RCSID("$NetBSD: remove.c,v 1.23 2019/06/21 02:14:59 jnemeth Exp $");
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

static int cmd_remove(gpt_t, int, char *[]);

static const char *removehelp[] = {
	"-a",
	"[-b blocknr] [-i index] [-L label] [-s sectors] [-t type]",
};

struct gpt_cmd c_remove = {
	"remove",
	cmd_remove,
	removehelp, __arraycount(removehelp),
	GPT_SYNC,
};

#define usage() gpt_usage(NULL, &c_remove)

static void
change(struct gpt_ent *ent, void *v, int backup)
{
	/* Remove the primary entry by clearing the partition type. */
	gpt_uuid_copy(ent->ent_type, gpt_uuid_nil);
}

static int
cmd_remove(gpt_t gpt, int argc, char *argv[])
{
	int ch;
	struct gpt_find find;

	memset(&find, 0, sizeof(find));
	find.msg = "removed";

	/* Get the remove options */
	while ((ch = getopt(argc, argv, GPT_FIND)) != -1)
		if (gpt_add_find(gpt, &find, ch) == -1)
			return usage();

	if (argc != optind)
		return usage();

	return gpt_change_ent(gpt, &find, change, NULL);
}
