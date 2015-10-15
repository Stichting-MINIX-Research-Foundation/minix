/*	$NetBSD: fgetln.c,v 1.12 2015/10/09 14:42:40 christos Exp $	*/

/*
 * Copyright (c) 2015 Joerg Jung <jung@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * portable fgetln() version
 */

#ifdef HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#if !HAVE_FGETLN
#include <stdlib.h>
#ifndef HAVE_NBTOOL_CONFIG_H
/* These headers are required, but included from nbtool_config.h */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#endif

char *
fgetln(FILE *fp, size_t *len)
{
	static char *buf = NULL;
	static size_t bufsz = 0;
	size_t r = 0;
	char *p;
	int c, e;

	if (!fp || !len) {
		errno = EINVAL;
		return NULL;
	}
	if (!buf) {
		if (!(buf = calloc(1, BUFSIZ)))
			return NULL;
		bufsz = BUFSIZ;
	}
	while ((c = getc(fp)) != EOF) {
		buf[r++] = c;
		if (r == bufsz) {
			/*
			 * Original uses reallocarray() but we don't have it
			 * in tools.
			 */
			if (!(p = realloc(buf, 2 * bufsz))) {
				e = errno;
				free(buf);
				errno = e;
				buf = NULL, bufsz = 0;
				return NULL;
			}
			buf = p, bufsz = 2 * bufsz;
		}
		if (c == '\n')
			break;
	}
	return (*len = r) ? buf : NULL;
}
#endif


#ifdef TEST
int
main(int argc, char *argv[])
{
	char *p;
	size_t len;

	while ((p = fgetln(stdin, &len)) != NULL) {
		(void)printf("%zu %s", len, p);
		free(p);
	}
	return 0;
}
#endif
