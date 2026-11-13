/*-
 * Copyright (c) 2005 Marcel Moolenaar
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
__FBSDID("$FreeBSD: src/sbin/gpt/label.c,v 1.3 2006/10/04 18:20:25 marcel Exp $");
#endif
#ifdef __RCSID
__RCSID("$NetBSD: label.c,v 1.30 2019/06/21 02:14:59 jnemeth Exp $");
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
#include "gpt_uuid.h"

static int cmd_label(gpt_t, int, char *[]);

static const char *labelhelp[] = {
	"-a <-l label | -f file>",
	"[-b blocknr] [-i index] [-L label] [-s sectors] [-t uuid] "
	    "<-l label | -f file>",
};

struct gpt_cmd c_label = {
	"label",
	cmd_label,
	labelhelp, __arraycount(labelhelp),
	GPT_SYNC,
};

#define usage() gpt_usage(NULL, &c_label)

static void
change(struct gpt_ent *ent, void *v, int backup)
{
	uint8_t *name = v;
	utf8_to_utf16(name, ent->ent_name, __arraycount(ent->ent_name));
}

static int
name_from_file(gpt_t gpt, void *v)
{
	FILE *f;
	char *p;
	size_t maxlen = 1024;
	size_t len;
	const char *fn = optarg;
	char **name = v;

	if (*name != NULL)
		return -1;

	if (strcmp(fn, "-") != 0) {
		f = fopen(fn, "r");
		if (f == NULL) {
			gpt_warn(gpt, "Can't open `%s'", fn);
			return -1;
		}
	} else
		f = stdin;

	if ((*name = malloc(maxlen)) == NULL) {
		gpt_warn(gpt, "Can't copy string");
		goto cleanup;
	}
	len = fread(*name, 1, maxlen - 1, f);
	if (ferror(f)) {
		gpt_warn(gpt, "Can't label from `%s'", fn);
		goto cleanup;
	}
	if (f != stdin)
		fclose(f);
	(*name)[len] = '\0';
	/* Only keep the first line, excluding the newline character. */
	p = strchr(*name, '\n');
	if (p != NULL)
		*p = '\0';
	return 0;
cleanup:
	free(*name);
	if (f != stdin)
		fclose(f);
	return -1;
}

static int
cmd_label(gpt_t gpt, int argc, char *argv[])
{
	int ch;
	struct gpt_find find;
	char *name = NULL;

	memset(&find, 0, sizeof(find));
	find.msg = "label changed";

	/* Get the label options */
	while ((ch = getopt(argc, argv, GPT_FIND "f:l:")) != -1) {
		switch(ch) {
		case 'f':
			if (name_from_file(gpt, &name) == -1)
				goto usage;
			break;
		case 'l':
			if (gpt_name_get(gpt, &name) == -1)
				goto usage;
			break;
		default:
			if (gpt_add_find(gpt, &find, ch) == -1)
				goto usage;
			break;
		}
	}

	if (name == NULL || argc != optind)
		goto usage;

	return gpt_change_ent(gpt, &find, change, name);
usage:
	usage();
	free(name);
	return -1;
}
