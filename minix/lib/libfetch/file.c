/*	$NetBSD: file.c,v 1.15 2009/10/15 12:36:57 joerg Exp $	*/
/*-
 * Copyright (c) 1998-2004 Dag-Erling Coïdan Smørgrav
 * Copyright (c) 2008, 2009 Joerg Sonnenberger <joerg@NetBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 * $FreeBSD: file.c,v 1.18 2007/12/14 10:26:58 des Exp $
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif
#if !defined(NETBSD) && !defined(__minix)
#include <nbcompat.h>
#endif

#include <sys/stat.h>

#include <dirent.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "common.h"

static int	fetch_stat_file(int, struct url_stat *);

static ssize_t
fetchFile_read(void *cookie, void *buf, size_t len)
{
	return read(*(int *)cookie, buf, len);
}

static ssize_t
fetchFile_write(void *cookie, const void *buf, size_t len)
{
	return write(*(int *)cookie, buf, len);
}

static void
fetchFile_close(void *cookie)
{
	int fd = *(int *)cookie;

	free(cookie);

	close(fd);
}

fetchIO *
fetchXGetFile(struct url *u, struct url_stat *us, const char *flags)
{
	char *path;
	fetchIO *f;
	struct url_stat local_us;
	int if_modified_since, fd, *cookie;

	if_modified_since = CHECK_FLAG('i');
	if (if_modified_since && us == NULL)
		us = &local_us;

	if ((path = fetchUnquotePath(u)) == NULL) {
		fetch_syserr();
		return NULL;
	}

	fd = open(path, O_RDONLY);
	free(path);
	if (fd == -1) {
		fetch_syserr();
		return NULL;
	}

	if (us && fetch_stat_file(fd, us) == -1) {
		close(fd);
		fetch_syserr();
		return NULL;
	}

	if (if_modified_since && u->last_modified > 0 &&
	    u->last_modified >= us->mtime) {
		close(fd);
		fetchLastErrCode = FETCH_UNCHANGED;
		snprintf(fetchLastErrString, MAXERRSTRING, "Unchanged");
		return NULL;
	}

	if (u->offset && lseek(fd, u->offset, SEEK_SET) == -1) {
		close(fd);
		fetch_syserr();
		return NULL;
	}

	cookie = malloc(sizeof(int));
	if (cookie == NULL) {
		close(fd);
		fetch_syserr();
		return NULL;
	}

	*cookie = fd;
	f = fetchIO_unopen(cookie, fetchFile_read, fetchFile_write, fetchFile_close);
	if (f == NULL) {
		close(fd);
		free(cookie);
	}
	return f;
}

fetchIO *
fetchGetFile(struct url *u, const char *flags)
{
	return (fetchXGetFile(u, NULL, flags));
}

fetchIO *
fetchPutFile(struct url *u, const char *flags)
{
	char *path;
	fetchIO *f;
	int fd, *cookie;

	if ((path = fetchUnquotePath(u)) == NULL) {
		fetch_syserr();
		return NULL;
	}

	if (CHECK_FLAG('a'))
		fd = open(path, O_WRONLY | O_APPEND);
	else
		fd = open(path, O_WRONLY);

	free(path);

	if (fd == -1) {
		fetch_syserr();
		return NULL;
	}

	if (u->offset && lseek(fd, u->offset, SEEK_SET) == -1) {
		close(fd);
		fetch_syserr();
		return NULL;
	}

	cookie = malloc(sizeof(int));
	if (cookie == NULL) {
		close(fd);
		fetch_syserr();
		return NULL;
	}

	*cookie = fd;
	f = fetchIO_unopen(cookie, fetchFile_read, fetchFile_write, fetchFile_close);
	if (f == NULL) {
		close(fd);
		free(cookie);
	}
	return f;
}

static int
fetch_stat_file(int fd, struct url_stat *us)
{
	struct stat sb;

	us->size = -1;
	us->atime = us->mtime = 0;
	if (fstat(fd, &sb) == -1) {
		fetch_syserr();
		return (-1);
	}
	us->size = sb.st_size;
	us->atime = sb.st_atime;
	us->mtime = sb.st_mtime;
	return (0);
}

int
fetchStatFile(struct url *u, struct url_stat *us, const char *flags)
{
	char *path;
	int fd, rv;

	if ((path = fetchUnquotePath(u)) == NULL) {
		fetch_syserr();
		return -1;
	}

	fd = open(path, O_RDONLY);
	free(path);

	if (fd == -1) {
		fetch_syserr();
		return -1;
	}

	rv = fetch_stat_file(fd, us);
	close(fd);

	return rv;
}

int
fetchListFile(struct url_list *ue, struct url *u, const char *pattern, const char *flags)
{
	char *path;
	struct dirent *de;
	DIR *dir;
	int ret;

	if ((path = fetchUnquotePath(u)) == NULL) {
		fetch_syserr();
		return -1;
	}
		
	dir = opendir(path);
	free(path);

	if (dir == NULL) {
		fetch_syserr();
		return -1;
	}

	ret = 0;

	while ((de = readdir(dir)) != NULL) {
		if (pattern && fnmatch(pattern, de->d_name, 0) != 0)
			continue;
		ret = fetch_add_entry(ue, u, de->d_name, 0);
		if (ret)
			break;
	}

	closedir(dir);

	return ret;
}
