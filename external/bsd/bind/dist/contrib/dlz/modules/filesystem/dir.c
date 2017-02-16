/*	$NetBSD: dir.c,v 1.1.1.3 2014/12/10 03:34:31 christos Exp $	*/

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
#include <sys/stat.h>

#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>

#include "dlz_minimal.h"
#include "dir.h"

void
dir_init(dir_t *dir) {
	dir->entry.name[0] = '\0';
	dir->entry.length = 0;

	dir->handle = NULL;
}

isc_result_t
dir_open(dir_t *dir, const char *dirname) {
	char *p;
	isc_result_t result = ISC_R_SUCCESS;

	if (strlen(dirname) + 3 > sizeof(dir->dirname))
		return (ISC_R_NOSPACE);
	strcpy(dir->dirname, dirname);

	p = dir->dirname + strlen(dir->dirname);
	if (dir->dirname < p && *(p - 1) != '/')
		*p++ = '/';
	*p++ = '*';
	*p = '\0';

	dir->handle = opendir(dirname);
	if (dir->handle == NULL) {
		switch (errno) {
		case ENOTDIR:
		case ELOOP:
		case EINVAL:
		case ENAMETOOLONG:
		case EBADF:
			result = ISC_R_INVALIDFILE;
		case ENOENT:
			result = ISC_R_FILENOTFOUND;
		case EACCES:
		case EPERM:
			result = ISC_R_NOPERM;
		case ENOMEM:
			result = ISC_R_NOMEMORY;
		default:
			result = ISC_R_UNEXPECTED;
		}
	}

	return (result);
}

/*!
 * \brief Return previously retrieved file or get next one.

 * Unix's dirent has
 * separate open and read functions, but the Win32 and DOS interfaces open
 * the dir stream and reads the first file in one operation.
 */
isc_result_t
dir_read(dir_t *dir) {
	struct dirent *entry;

	entry = readdir(dir->handle);
	if (entry == NULL)
		return (ISC_R_NOMORE);

	if (sizeof(dir->entry.name) <= strlen(entry->d_name))
	    return (ISC_R_UNEXPECTED);

	strcpy(dir->entry.name, entry->d_name);

	dir->entry.length = strlen(entry->d_name);
	return (ISC_R_SUCCESS);
}

/*!
 * \brief Close directory stream.
 */
void
dir_close(dir_t *dir) {
       (void)closedir(dir->handle);
       dir->handle = NULL;
}

/*!
 * \brief Reposition directory stream at start.
 */
isc_result_t
dir_reset(dir_t *dir) {
	rewinddir(dir->handle);

	return (ISC_R_SUCCESS);
}
