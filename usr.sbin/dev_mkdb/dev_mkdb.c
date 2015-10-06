/*	$NetBSD: dev_mkdb.c,v 1.29 2012/06/03 21:42:47 joerg Exp $	*/

/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__RCSID("$NetBSD: dev_mkdb.c,v 1.29 2012/06/03 21:42:47 joerg Exp $");

#include <sys/queue.h>
#include <sys/stat.h>

#include <cdbw.h>
#include <db.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <fts.h>
#include <paths.h>
#include <search.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <util.h>

#define	HASH_SIZE	65536
#define	FILE_PERMISSION	S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH

static struct cdbw *db;
static DB *db_compat;
static const char *db_name;
static char *db_name_tmp;

static void	usage(void) __dead;

static void
cdb_open(void)
{
	db = cdbw_open();
	if (db == NULL)
		err(1, "opening cdb writer failed");
}

static void
cdb_close(void)
{
	int fd;

	fd = open(db_name_tmp, O_CREAT|O_EXCL|O_WRONLY, FILE_PERMISSION);
	if (fd == -1)
		err(1, "opening %s failed", db_name_tmp);
	if (cdbw_output(db, fd, "NetBSD6 devdb", NULL))
		err(1, "failed to write temporary database %s", db_name_tmp);
	cdbw_close(db);
	db = NULL;
	if (close(fd))
		err(1, "failed to write temporary database %s", db_name_tmp);
}

static void
cdb_add_entry(dev_t dev, mode_t type, const char *relpath)
{
	uint8_t *buf;
	size_t len;

	len = strlen(relpath) + 1;
	buf = malloc(len + 10);
	le64enc(buf, dev);
	le16enc(buf + 8, type);
	memcpy(buf + 10, relpath, len);
	cdbw_put(db, buf, 10, buf, len + 10);
	free(buf);
}

static void
compat_open(void)
{
	static HASHINFO openinfo = {
		4096,		/* bsize */
		128,		/* ffactor */
		1024,		/* nelem */
		2048 * 1024,	/* cachesize */
		NULL,		/* hash() */
		0		/* lorder */
	};

	db_compat = dbopen(db_name_tmp, O_CREAT|O_EXCL|O_EXLOCK|O_RDWR|O_TRUNC,
	    FILE_PERMISSION, DB_HASH, &openinfo);

	if (db_compat == NULL)
		err(1, "failed to create temporary database %s",
		    db_name_tmp);
}

static void
compat_close(void)
{
	if ((*db_compat->close)(db_compat))
		err(1, "failed to write temporary database %s", db_name_tmp);
}

static void
compat_add_entry(dev_t dev, mode_t type, const char *relpath)
{
	/*
	 * Keys are a mode_t followed by a dev_t.  The former is the type of
	 * the file (mode & S_IFMT), the latter is the st_rdev field.  Note
	 * that the structure may contain padding, so we have to clear it
	 * out here.
	 */
	struct {
		mode_t type;
		dev_t dev;
	} bkey;
	struct {
		mode_t type;
		int32_t dev;
	} obkey;
	DBT data, key;

	(void)memset(&bkey, 0, sizeof(bkey));
	key.data = &bkey;
	key.size = sizeof(bkey);
	data.data = __UNCONST(relpath);
	data.size = strlen(relpath) + 1;
	bkey.type = type;
	bkey.dev = dev;
	if ((*db_compat->put)(db_compat, &key, &data, 0))
		err(1, "failed to write temporary database %s", db_name_tmp);

	/*
	 * If the device fits into the old 32bit format, add compat entry
	 * for pre-NetBSD6 libc.
	 */

	if ((dev_t)(int32_t)dev != dev)
		return;

	(void)memset(&obkey, 0, sizeof(obkey));
	key.data = &obkey;
	key.size = sizeof(obkey);
	data.data = __UNCONST(relpath);
	data.size = strlen(relpath) + 1;
	obkey.type = type;
	obkey.dev = (int32_t)dev;
	if ((*db_compat->put)(db_compat, &key, &data, 0))
		err(1, "failed to write temporary database %s", db_name_tmp);
}

int
main(int argc, char **argv)
{
	struct stat *st;
	FTS *ftsp;
	FTSENT *p;
	int ch;
	char *pathv[2];
	size_t dlen;
	int compat_mode;

	setprogname(argv[0]);
	compat_mode = 0;

	while ((ch = getopt(argc, argv, "co:")) != -1)
		switch (ch) {
		case 'c':
			compat_mode = 1;
			break;
		case 'o':
			db_name = optarg;
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc > 1)
		usage();

	pathv[1] = NULL;
	if (argc == 1)
		pathv[0] = argv[0];
	else
		pathv[0] = __UNCONST(_PATH_DEV);
	
	ftsp = fts_open(pathv, FTS_NOCHDIR | FTS_PHYSICAL, NULL);
	if (ftsp == NULL)
		err(1, "fts_open: %s", pathv[0]);

	if (db_name == NULL) {
		if (compat_mode)
			db_name = _PATH_DEVDB;
		else
			db_name = _PATH_DEVCDB;
	}
	easprintf(&db_name_tmp, "%s.XXXXXXX", db_name);
	mktemp(db_name_tmp);

	if (compat_mode)
		compat_open();
	else
		cdb_open();

	while ((p = fts_read(ftsp)) != NULL) {
		if (p->fts_info != FTS_DEFAULT)
			continue;

		st = p->fts_statp;
		if (!S_ISCHR(st->st_mode) && !S_ISBLK(st->st_mode))
			continue;
		dlen = strlen(pathv[0]);
		while (pathv[0][dlen] == '/')
			++dlen;
		if (compat_mode)
			compat_add_entry(st->st_rdev, st->st_mode & S_IFMT,
			    p->fts_path + dlen);
		else
			cdb_add_entry(st->st_rdev, st->st_mode & S_IFMT,
			    p->fts_path + dlen);
	}
	(void)fts_close(ftsp);

	if (compat_mode)
		compat_close();
	else
		cdb_close();

	if (rename(db_name_tmp, db_name) == -1)
		err(1, "rename %s to %s", db_name_tmp, db_name);
	return 0;
}

static void
usage(void)
{

	(void)fprintf(stderr, "Usage: %s [-c] [-o database] [directory]\n",
	    getprogname());
	exit(1);
}
