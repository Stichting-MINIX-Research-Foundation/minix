/*	$NetBSD: output_db.c,v 1.1 2010/04/25 00:54:46 joerg Exp $	*/

/*-
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Luke Mewburn and Christos Zoulas.
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
#ifndef lint
__RCSID("$NetBSD: output_db.c,v 1.1 2010/04/25 00:54:46 joerg Exp $");
#endif /* not lint */

#include <sys/param.h>

#include <assert.h>
#include <db.h>
#include <err.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <util.h>
#include <ctype.h>
#include <errno.h>
#include <stringlist.h>

#include "extern.h"

static DB *db;

static const HASHINFO hinfo = {
	.bsize = 256,
	.ffactor = 4,
	.nelem = 32768,
	.cachesize = 1024,
	.hash = NULL,
	.lorder = 0
};

static void	store(DBT *, DBT *, int);
static void	killproto(DBT *);
static const char *mkaliases(StringList *, char *, size_t);

int
db_open(const char *tname)
{
	db = dbopen(tname, O_RDWR | O_CREAT | O_EXCL,
	    (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH), DB_HASH, &hinfo);

	return db != NULL ? 0 : -1;
}

int
db_close(void)
{
	int rv;

	rv = (db->close)(db);
	db = NULL;

	return rv;
}

void
db_add(StringList *sl, size_t port, const char *proto, size_t *cnt,
    int warndup)
{
	size_t i;
	char	 keyb[BUFSIZ], datab[BUFSIZ], abuf[BUFSIZ];
	DBT	 data, key;
	key.data = keyb;
	data.data = datab;

	/* key `indirect key', data `full line' */
	data.size = snprintf(datab, sizeof(datab), "%zu", (*cnt)++) + 1;
	key.size = snprintf(keyb, sizeof(keyb), "%s %zu/%s %s",
	    sl->sl_str[0], port, proto, mkaliases(sl, abuf, sizeof(abuf))) + 1;
	store(&data, &key, warndup);

	/* key `\377port/proto', data = `indirect key' */
	key.size = snprintf(keyb, sizeof(keyb), "\377%zu/%s",
	    port, proto) + 1;
	store(&key, &data, warndup);

	/* key `\377port', data = `indirect key' */
	killproto(&key);
	store(&key, &data, warndup);

	/* add references for service and all aliases */
	for (i = 0; i < sl->sl_cur; i++) {
		/* key `\376service/proto', data = `indirect key' */
		key.size = snprintf(keyb, sizeof(keyb), "\376%s/%s",
		    sl->sl_str[i], proto) + 1;
		store(&key, &data, warndup);

		/* key `\376service', data = `indirect key' */
		killproto(&key);
		store(&key, &data, warndup);
	}
	sl_free(sl, 1);
}

static void
killproto(DBT *key)
{
	char *p, *d = key->data;

	if ((p = strchr(d, '/')) == NULL)
		abort();
	*p++ = '\0';
	key->size = p - d;
}

static void
store(DBT *key, DBT *data, int warndup)
{
#ifdef DEBUG
	int k = key->size - 1;
	int d = data->size - 1;
	(void)printf("store [%*.*s] [%*.*s]\n",
		k, k, (char *)key->data + 1,
		d, d, (char *)data->data + 1);
#endif
	switch ((db->put)(db, key, data, R_NOOVERWRITE)) {
	case 0:
		break;
	case 1:
		if (warndup)
			warnx("duplicate service `%s'",
			    &((char *)key->data)[1]);
		break;
	case -1:
		err(1, "put");
		break;
	default:
		abort();
		break;
	}
}

static const char *
mkaliases(StringList *sl, char *buf, size_t len)
{
	size_t nc, i, pos;

	buf[0] = 0;
	for (i = 1, pos = 0; i < sl->sl_cur; i++) {
		nc = strlcpy(buf + pos, sl->sl_str[i], len);
		if (nc >= len)
			goto out;
		pos += nc;
		len -= nc;
		nc = strlcpy(buf + pos, " ", len);
		if (nc >= len)
			goto out;
		pos += nc;
		len -= nc;
	}
	return buf;
out:
	warn("aliases for `%s' truncated", sl->sl_str[0]);
	return buf;
}
