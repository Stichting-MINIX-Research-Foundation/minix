/*
 * Copyright (c) 2004 Darren Tucker.
 *
 * Based originally on asprintf.c from OpenBSD:
 * Copyright (c) 1997 Todd C. Miller <Todd.Miller@courtesan.com>
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

#include "includes.h"
#ifndef HAVE_ASPRINTF

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdarg.h>

#define INIT_SZ	128

int
asprintf(char **str, const char *fmt, ...)
{
	int ret = -1;
	va_list ap;
	char *string, *newstr;
	size_t len;

	va_start(ap, fmt);
	if ((string = malloc(INIT_SZ)) == NULL)
		goto fail;

	ret = vsnprintf(string, INIT_SZ, fmt, ap);
	if (ret >= 0 && ret < INIT_SZ) { /* succeeded with initial alloc */
		*str = string;
	} else if (ret == INT_MAX) { /* shouldn't happen */
		goto fail;
	} else {	/* bigger than initial, realloc allowing for nul */
		len = (size_t)ret + 1;
		if ((newstr = realloc(string, len)) == NULL) {
			free(string);
			goto fail;
		} else {
			ret = vsnprintf(newstr, len, fmt, ap);
			if (ret >= 0 && (size_t)ret < len) {
				*str = newstr;
			} else { /* failed with realloc'ed string, give up */
				free(newstr);
				goto fail;
			}
		}
	} 
	va_end(ap);
	return (ret);

fail:
	va_end(ap);
	*str = NULL;
	errno = ENOMEM;
	return (-1);
}
#endif
