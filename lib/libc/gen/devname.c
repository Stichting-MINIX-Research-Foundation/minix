/*	$NetBSD: devname.c,v 1.22 2012/06/03 21:42:46 joerg Exp $	*/

/*-
 * Copyright (c) 2012 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Simon Burge.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__RCSID("$NetBSD: devname.c,v 1.22 2012/06/03 21:42:46 joerg Exp $");

#include "namespace.h"
#include "reentrant.h"
#include <sys/stat.h>

#include <cdbr.h>
#include <errno.h>
#include <fts.h>
#include <limits.h>
#include <paths.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef __weak_alias
__weak_alias(devname_r,_devname_r)
#endif

static once_t db_opened = ONCE_INITIALIZER;
static struct cdbr *db;
static devmajor_t pts;

static void
devname_dbopen(void)
{
	db = cdbr_open(_PATH_DEVCDB, CDBR_DEFAULT);
	pts = getdevmajor("pts", S_IFCHR);
}

__CTASSERT(sizeof(dev_t) == 8);

static int
devname_dblookup(dev_t dev, mode_t type, char *path, size_t len)
{
	const void *data;
	size_t datalen;
	uint8_t key[10];

	le64enc(key, dev);
	le16enc(key + 8, type);
	if (cdbr_find(db, key, sizeof(key), &data, &datalen) != 0)
		return ENOENT;
	if (datalen <= sizeof(key))
		return ENOENT;
	if (memcmp(key, data, sizeof(key)) != 0)
		return ENOENT;
	data = (const char *)data + sizeof(key);
	datalen -= sizeof(key);
	if (memchr(data, '\0', datalen) != (const char *)data + datalen - 1)
		return ENOENT;
	if (datalen > len)
		return ERANGE;
	memcpy(path, data, datalen);
	return 0;
}

static int
devname_ptslookup(dev_t dev, mode_t type, char *path, size_t len)
{
	int rv;

	if (type != S_IFCHR || pts == NODEVMAJOR || major(dev) != pts)
		return ENOENT;

	rv = snprintf(path, len, "%s%d", _PATH_DEV_PTS + sizeof(_PATH_DEV) - 1,
	    minor(dev));
	if (rv < 0 || (size_t)rv >= len)
		return ERANGE;
	return 0;
}

static int
devname_fts(dev_t dev, mode_t type, char *path, size_t len)
{
	FTS *ftsp;
	FTSENT *fe;
	static const char path_dev[] = _PATH_DEV;
	static char * const dirs[2] = { __UNCONST(path_dev), NULL };
	const size_t len_dev = strlen(path_dev);
	int rv;

	if ((ftsp = fts_open(dirs, FTS_NOCHDIR | FTS_PHYSICAL, NULL)) == NULL)
		return ENOENT;

	rv = ENOENT;
	while ((fe = fts_read(ftsp)) != NULL) {
		if (fe->fts_info != FTS_DEFAULT)
			continue;
		if (fe->fts_statp->st_rdev != dev)
			continue;
		if ((type & S_IFMT) != (fe->fts_statp->st_mode & S_IFMT))
			continue;
		if (strncmp(fe->fts_path, path_dev, len_dev))
			continue;
		if (strlcpy(path, fe->fts_path + len_dev, len) < len) {
			rv = 0;
			break;
		}
	}

	fts_close(ftsp);
	return rv;
}

int
devname_r(dev_t dev, mode_t type, char *path, size_t len)
{
	int rv;

	thr_once(&db_opened, devname_dbopen);

	if (db != NULL) {
		rv = devname_dblookup(dev, type, path, len);
		if (rv == 0 || rv == ERANGE)
			return rv;
	}

	rv = devname_ptslookup(dev, type, path, len);
	if (rv == 0 || rv == ERANGE)
		return rv;

	if (db != NULL)
		return ENOENT;
	rv = devname_fts(dev, type, path, len);
	return rv;
}

char *
devname(dev_t dev, mode_t type)
{
	static char path[PATH_MAX];

	return devname_r(dev, type, path, sizeof(path)) == 0 ? path : NULL;
}
