/* $NetBSD: asprintf.c,v 1.2 2007/07/20 00:10:06 tnn Exp $ */

/*-
 * Copyright (c) 2007 Joerg Sonnenberger <joerg@NetBSD.org>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>

#include <stdarg.h>

int
asprintf(char **ret, const char *fmt, ...)
{
	va_list ap;
	int retval;

	va_start(ap, fmt);
	retval = vasprintf(ret, fmt, ap);
	va_end(ap);

	return retval;
}

int
vasprintf(char **ret, const char *fmt, va_list ap)
{
	char *buf, *new_buf;
	size_t len;
	int retval;

	len = 128;
	buf = malloc(len);
	if (buf == NULL) {
		*ret = NULL;
		return -1;
	}

	retval = vsnprintf(buf, len, fmt, ap);
	if (retval < 0) {
		free(buf);
		*ret = NULL;
		return -1;
	}

	if (retval < len) {
		new_buf = realloc(buf, retval + 1);
		if (new_buf == NULL)
			*ret = buf;
		else
			*ret = new_buf;
		return retval;
	}

	len = (size_t)retval + 1;
	free(buf);
	buf = malloc(len);
	if (buf == NULL) {
		*ret = NULL;
		return -1;
	}
	retval = vsnprintf(buf, len, fmt, ap);
	if (retval != len - 1) {
		free(buf);
		*ret = NULL;
		return -1;
	}
	*ret = buf;
	return retval;
}
