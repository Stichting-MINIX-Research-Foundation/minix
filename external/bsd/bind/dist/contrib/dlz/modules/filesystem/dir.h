/*	$NetBSD: dir.h,v 1.1.1.3 2014/12/10 03:34:31 christos Exp $	*/

/*
 * Copyright (C) 2013 Internet Systems Consortium, Inc. ("ISC")
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <dirent.h>

#define DIR_NAMEMAX 256
#define DIR_PATHMAX 1024

typedef struct direntry {
	char 		name[DIR_NAMEMAX];
	unsigned int	length;
} direntry_t;

typedef struct dir {
	char		dirname[DIR_PATHMAX];
	direntry_t	entry;
	DIR *		handle;
} dir_t;

void
dir_init(dir_t *dir);

isc_result_t
dir_open(dir_t *dir, const char *dirname);

isc_result_t
dir_read(dir_t *dir);

isc_result_t
dir_reset(dir_t *dir);

void
dir_close(dir_t *dir);
