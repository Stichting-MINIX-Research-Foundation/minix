/*	$NetBSD: parseutils.c,v 1.6 2011/08/18 13:20:04 christos Exp $	*/

/*
 * Copyright (c) 1996, 1997
 * 	Matthias Drochner.  All rights reserved.
 * Copyright (c) 1996, 1997
 * 	Perry E. Metzger.  All rights reserved.
 * Copyright (c) 1997
 *	Jason R. Thorpe.  All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgements:
 *	This product includes software developed for the NetBSD Project
 *	by Matthias Drochner.
 *	This product includes software developed for the NetBSD Project
 *	by Perry E. Metzger.
 * 4. The names of the authors may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

#include <lib/libkern/libkern.h>
#include <lib/libsa/stand.h>
#include <sys/boot_flag.h>

#include "libi386.h"

/*
 * chops the head from the arguments and returns the arguments if any,
 * or possibly an empty string.
 */
char *
gettrailer(char *arg)
{
	char *options;

	for (options = arg; *options; options++) {
		switch (*options) {
		case ' ':
		case '\t':
			*options++ = '\0';
			break;
		default:
			continue;
		}
		break;
	}
	if (*options == '\0')
		return "";

	/* trim leading blanks/tabs */
	while (*options == ' ' || *options == '\t')
		options++;

	return options;
}

int
parseopts(const char *opts, int *howto)
{
	int r, tmpopt = 0;

	opts++; 	/* skip - */
	while (*opts) {
		r = 0;
		BOOT_FLAG(*opts, r);
		if (r == 0) {
			printf("-%c: unknown flag\n", *opts);
			command_help(NULL);
			return 0;
		}
		tmpopt |= r;
		opts++;
		if (*opts == ' ' || *opts == '\t') {
			do
				opts++;		/* skip whitespace */
			while (*opts == ' ' || *opts == '\t');
			if (*opts == '-')
				opts++;		/* skip - */
			else if (*opts != '\0') {
				printf("invalid arguments\n");
				command_help(NULL);
				return 0;
			}
		}
	}

	*howto = tmpopt;
	return 1;
}

int
parseboot(char *arg, char **filename, int *howto)
{
	char *opts = NULL;

	*filename = 0;
	*howto = 0;

	/* if there were no arguments */
	if (!*arg)
		return 1;

	/* format is... */
	/* [[xxNx:]filename] [-adqsv] */

	/* check for just args */
	if (arg[0] == '-')
		opts = arg;
	else {
		/* there's a file name */
		*filename = arg;

		opts = gettrailer(arg);
		if (!*opts)
			opts = NULL;
		else if (*opts != '-') {
			printf("invalid arguments\n");
			command_help(NULL);
			return 0;
		}
	}

	/* at this point, we have dealt with filenames. */

	/* now, deal with options */
	if (opts) {
		if (parseopts(opts, howto) == 0)
			return 0;
	}

	return 1;
}
