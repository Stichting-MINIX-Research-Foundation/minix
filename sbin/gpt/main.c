/*	$NetBSD: main.c,v 1.14 2020/05/24 18:42:20 jmcneill Exp $	*/

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
 *
 * CRC32 code derived from work by Gary S. Brown.
 */

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <sys/cdefs.h>
#ifdef __RCSID
__RCSID("$NetBSD: main.c,v 1.14 2020/05/24 18:42:20 jmcneill Exp $");
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>
#include <errno.h>
#include <sys/stat.h>
#ifndef NBTOOL_CONFIG_H
#include <util.h>
#endif

#include "map.h"
#include "gpt.h"

static const struct gpt_cmd c_null;

extern const struct gpt_cmd
	c_add,
#ifndef HAVE_NBTOOL_CONFIG_H
	c_backup,
#endif
	c_biosboot,
	c_create,
	c_destroy,
	c_header,
	c_label,
	c_migrate,
	c_recover,
	c_remove,
	c_resize,
	c_resizedisk,
#ifndef HAVE_NBTOOL_CONFIG_H
	c_restore,
#endif
	c_set,
	c_show,
	c_type,
	c_unset,
	c_uuid;

static const struct gpt_cmd *cmdsw[] = {
	&c_add,
#ifndef HAVE_NBTOOL_CONFIG_H
	&c_backup,
#endif
	&c_biosboot,
	&c_create,
	&c_destroy,
	&c_header,
	&c_label,
	&c_migrate,
	&c_recover,
	&c_remove,
	&c_resize,
	&c_resizedisk,
#ifndef HAVE_NBTOOL_CONFIG_H
	&c_restore,
#endif
	&c_set,
	&c_show,
	&c_type,
	&c_unset,
	&c_uuid,
	&c_null,
};

__dead static void
usage(void)
{
	const char *p = getprogname();
	const char *f =
	    "[-nrqv] [-m mediasize] [-s sectorsize] [-T timestamp]";
	size_t i;

	if (strcmp(p, "gpt") == 0)
		fprintf(stderr,
		    "Usage: %s %s command device\n", p, f);
	else
		fprintf(stderr,
		    "Usage: %s %s device command\n", p, f);
	fprintf(stderr, "Commands:\n");
	for (i = 0; i < __arraycount(cmdsw); i++)
		gpt_usage("\t", cmdsw[i]);
	exit(EXIT_FAILURE);
}

static void
prefix(const char *cmd)
{
	char *pfx;

	if (asprintf(&pfx, "%s %s", getprogname(), cmd) < 0)
		pfx = NULL;
	else
		setprogname(pfx);
}

static time_t
get_tstamp(const char *b)
{
	struct stat st;
	char *eb;
	long long l;
#ifndef HAVE_NBTOOL_CONFIG_H
	time_t when;
#endif

	if (stat(b, &st) != -1)
		return (time_t)st.st_mtime;

#ifndef HAVE_NBTOOL_CONFIG_H
	errno = 0;
	if ((when = parsedate(b, NULL, NULL)) != -1 || errno == 0)
		return when;
#endif
	errno = 0;
	l = strtoll(b, &eb, 0);
	if (b == eb || *eb || errno)
		errx(EXIT_FAILURE, "Can't parse timestamp `%s'", b);
	return (time_t)l;
}

int
main(int argc, char *argv[])
{
	char *cmd, *p, *dev = NULL;
	int ch, i;
	u_int secsz = 0;
	off_t mediasz = 0;
	int flags = 0;
	int verbose = 0;
	time_t timestamp = 0;
	gpt_t gpt;

	setprogname(argv[0]);

	if (strcmp(getprogname(), "gpt") == 0) {
		if (argc < 3)
			usage();
		dev = argv[--argc];
	}

#ifdef __GLIBC__
#define GETOPT_BE_POSIX		"+"
#else
#define GETOPT_BE_POSIX		""
#endif

	/* Get the generic options */
	while ((ch = getopt(argc, argv, GETOPT_BE_POSIX "Hm:nqrs:T:v")) != -1) {
		switch(ch) {
		case 'H':
			flags |= GPT_HYBRID;
			break;
		case 'm':
			if (mediasz > 0)
				usage();
			mediasz = strtol(optarg, &p, 10);
			if (*p != 0 || mediasz < 1)
				usage();
			break;
		case 'n':
			flags |= GPT_NOSYNC;
			break;
		case 'r':
			flags |= GPT_READONLY;
			break;
		case 'q':
			flags |= GPT_QUIET;
			break;
		case 's':
			if (gpt_uint_get(NULL, &secsz) == -1)
				usage();
			break;
		case 'T':
			flags |= GPT_TIMESTAMP;
			timestamp = get_tstamp(optarg);
			break;
		case 'v':
			verbose++;
			break;
		default:
			usage();
		}
	}

	if (argc == optind)
		usage();

	if (dev == NULL)
		dev = argv[optind++];

	if (argc == optind)
		usage();

	cmd = argv[optind++];
	for (i = 0; cmdsw[i]->name != NULL && strcmp(cmd, cmdsw[i]->name); i++)
		continue;

	if (cmdsw[i]->fptr == NULL)
		errx(EXIT_FAILURE, "Unknown command: %s", cmd);

	prefix(cmd);

	if (*dev != '-') {
		gpt = gpt_open(dev, flags | cmdsw[i]->flags,
		    verbose, mediasz, secsz, timestamp);
		if (gpt == NULL)
			return EXIT_FAILURE;
	} else {
		if ((cmdsw[i]->flags & GPT_OPTDEV) == 0)
			errx(EXIT_FAILURE,
			     "Command %s needs a device parameter", cmd);
		argc++;
		gpt = NULL;
	}

	if ((*cmdsw[i]->fptr)(gpt, argc, argv) == -1)
		return EXIT_FAILURE;

	if (gpt)
		gpt_close(gpt);
	return EXIT_SUCCESS;
}
