/*	$NetBSD: utmpx.c,v 1.30 2012/06/24 15:26:03 christos Exp $	 */

/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas.
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

#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: utmpx.c,v 1.30 2012/06/24 15:26:03 christos Exp $");
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <assert.h>
#include <db.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <utmp.h>
#include <utmpx.h>
#include <vis.h>

static FILE *fp;
static int readonly = 0;
static int version = 1;
static struct utmpx ut;
static char utfile[MAXPATHLEN] = _PATH_UTMPX;

static struct utmpx *utmp_update(const struct utmpx *);

static const char vers[] = "utmpx-2.00";

struct otimeval {
	long tv_sec;
	long tv_usec;
};

static void
old2new(struct utmpx *utx)
{
	struct otimeval otv;
	struct timeval *tv = &utx->ut_tv;
	(void)memcpy(&otv, tv, sizeof(otv));
	tv->tv_sec = otv.tv_sec;
	tv->tv_usec = (suseconds_t)otv.tv_usec;
}

static void
new2old(struct utmpx *utx)
{
	struct timeval tv;
	struct otimeval *otv = (void *)&utx->ut_tv;
	(void)memcpy(&tv, otv, sizeof(tv));
	otv->tv_sec = (long)tv.tv_sec;
	otv->tv_usec = (long)tv.tv_usec;
}

void
setutxent(void)
{

	(void)memset(&ut, 0, sizeof(ut));
	if (fp == NULL)
		return;
	(void)fseeko(fp, (off_t)sizeof(ut), SEEK_SET);
}


void
endutxent(void)
{

	(void)memset(&ut, 0, sizeof(ut));
	if (fp != NULL) {
		(void)fclose(fp);
		fp = NULL;
		readonly = 0;
	}
}


struct utmpx *
getutxent(void)
{

	if (fp == NULL) {
		struct stat st;

		if ((fp = fopen(utfile, "re+")) == NULL)
			if ((fp = fopen(utfile, "w+")) == NULL) {
				if ((fp = fopen(utfile, "r")) == NULL)
					goto fail;
				else
					readonly = 1;
			}
					

		/* get file size in order to check if new file */
		if (fstat(fileno(fp), &st) == -1)
			goto failclose;

		if (st.st_size == 0) {
			/* new file, add signature record */
			(void)memset(&ut, 0, sizeof(ut));
			ut.ut_type = SIGNATURE;
			(void)memcpy(ut.ut_user, vers, sizeof(vers));
			if (fwrite(&ut, sizeof(ut), 1, fp) != 1)
				goto failclose;
		} else {
			/* old file, read signature record */
			if (fread(&ut, sizeof(ut), 1, fp) != 1)
				goto failclose;
			if (memcmp(ut.ut_user, vers, 5) != 0 ||
			    ut.ut_type != SIGNATURE)
				goto failclose;
		}
		version = ut.ut_user[6] - '0';
	}

	if (fread(&ut, sizeof(ut), 1, fp) != 1)
		goto fail;
	if (version == 1)
		old2new(&ut);

	return &ut;
failclose:
	(void)fclose(fp);
fail:
	(void)memset(&ut, 0, sizeof(ut));
	return NULL;
}


struct utmpx *
getutxid(const struct utmpx *utx)
{

	_DIAGASSERT(utx != NULL);

	if (utx->ut_type == EMPTY)
		return NULL;

	do {
		if (ut.ut_type == EMPTY)
			continue;
		switch (utx->ut_type) {
		case EMPTY:
			return NULL;
		case RUN_LVL:
		case BOOT_TIME:
		case OLD_TIME:
		case NEW_TIME:
			if (ut.ut_type == utx->ut_type)
				return &ut;
			break;
		case INIT_PROCESS:
		case LOGIN_PROCESS:
		case USER_PROCESS:
		case DEAD_PROCESS:
			switch (ut.ut_type) {
			case INIT_PROCESS:
			case LOGIN_PROCESS:
			case USER_PROCESS:
			case DEAD_PROCESS:
				if (memcmp(ut.ut_id, utx->ut_id,
				    sizeof(ut.ut_id)) == 0)
					return &ut;
				break;
			default:
				break;
			}
			break;
		default:
			return NULL;
		}
	} while (getutxent() != NULL);
	return NULL;
}


struct utmpx *
getutxline(const struct utmpx *utx)
{

	_DIAGASSERT(utx != NULL);

	do {
		switch (ut.ut_type) {
		case EMPTY:
			break;
		case LOGIN_PROCESS:
		case USER_PROCESS:
			if (strncmp(ut.ut_line, utx->ut_line,
			    sizeof(ut.ut_line)) == 0)
				return &ut;
			break;
		default:
			break;
		}
	} while (getutxent() != NULL);
	return NULL;
}


struct utmpx *
pututxline(const struct utmpx *utx)
{
	struct utmpx temp, *u = NULL;
	int gotlock = 0;

	_DIAGASSERT(utx != NULL);

	if (utx == NULL)
		return NULL;

	if (strcmp(_PATH_UTMPX, utfile) == 0) {
		if (geteuid() == 0) {
			if (fp != NULL && readonly)
				endutxent();
		} else {
			if (fp == NULL || readonly)
				return utmp_update(utx);
		}
	}


	(void)memcpy(&temp, utx, sizeof(temp));

	if (fp == NULL) {
		(void)getutxent();
		if (fp == NULL || readonly)
			return NULL;
	}

	if (getutxid(&temp) == NULL) {
		setutxent();
		if (getutxid(&temp) == NULL) {
			if (lockf(fileno(fp), F_LOCK, (off_t)0) == -1)
				return NULL;
			gotlock++;
			if (fseeko(fp, (off_t)0, SEEK_END) == -1)
				goto fail;
		}
	}

	if (!gotlock) {
		/* we are not appending */
		if (fseeko(fp, -(off_t)sizeof(ut), SEEK_CUR) == -1)
			return NULL;
	}

	if (version == 1)
		new2old(&temp);
	if (fwrite(&temp, sizeof (temp), 1, fp) != 1)
		goto fail;

	if (fflush(fp) == -1)
		goto fail;

	u = memcpy(&ut, &temp, sizeof(ut));
fail:
	if (gotlock) {
		if (lockf(fileno(fp), F_ULOCK, (off_t)0) == -1)
			return NULL;
	}
	return u;
}


static struct utmpx *
utmp_update(const struct utmpx *utx)
{
	char buf[sizeof(*utx) * 4 + 1];
	pid_t pid;
	int status;

	_DIAGASSERT(utx != NULL);

	(void)strvisx(buf, (const char *)(const void *)utx, sizeof(*utx),
	    VIS_WHITE);
	switch (pid = fork()) {
	case 0:
		(void)execl(_PATH_UTMP_UPDATE,
		    strrchr(_PATH_UTMP_UPDATE, '/') + 1, buf, NULL);
		_exit(1);
		/*NOTREACHED*/
	case -1:
		return NULL;
	default:
		if (waitpid(pid, &status, 0) == -1)
			return NULL;
		if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
			return memcpy(&ut, utx, sizeof(ut));
		return NULL;
	}

}

/*
 * The following are extensions and not part of the X/Open spec.
 */
int
updwtmpx(const char *file, const struct utmpx *utx)
{
	int fd;
	int saved_errno;

	_DIAGASSERT(file != NULL);
	_DIAGASSERT(utx != NULL);

#ifndef __minix
	fd = open(file, O_WRONLY|O_APPEND|O_SHLOCK);
#else
	fd = open(file, O_WRONLY|O_APPEND);
#endif

	if (fd == -1) {
#ifndef __minix
		if ((fd = open(file, O_CREAT|O_WRONLY|O_EXLOCK, 0644)) == -1)
			return -1;
#else
		if ((fd = open(file, O_CREAT|O_WRONLY, 0644)) < 0)
			return -1;
		if (flock(fd, LOCK_EX) < 0)
			return -1;
#endif
		(void)memset(&ut, 0, sizeof(ut));
		ut.ut_type = SIGNATURE;
		(void)memcpy(ut.ut_user, vers, sizeof(vers));
		if (write(fd, &ut, sizeof(ut)) == -1)
			goto failed;
	} else {
#ifdef __minix
		if (flock(fd, LOCK_SH) < 0 )
			return -1;
#endif
	}
	if (write(fd, utx, sizeof(*utx)) == -1)
		goto failed;
	if (close(fd) == -1)
		return -1;
	return 0;

  failed:
	saved_errno = errno;
	(void) close(fd);
	errno = saved_errno;
	return -1;
}


int
utmpxname(const char *fname)
{
	size_t len;

	_DIAGASSERT(fname != NULL);

	len = strlen(fname);

	if (len >= sizeof(utfile))
		return 0;

	/* must end in x! */
	if (fname[len - 1] != 'x')
		return 0;

	(void)strlcpy(utfile, fname, sizeof(utfile));
	endutxent();
	return 1;
}


void
getutmp(const struct utmpx *ux, struct utmp *u)
{

	_DIAGASSERT(ux != NULL);
	_DIAGASSERT(u != NULL);

	(void)memcpy(u->ut_name, ux->ut_name, sizeof(u->ut_name));
	(void)memcpy(u->ut_line, ux->ut_line, sizeof(u->ut_line));
	(void)memcpy(u->ut_host, ux->ut_host, sizeof(u->ut_host));
	u->ut_time = ux->ut_tv.tv_sec;
}

void
getutmpx(const struct utmp *u, struct utmpx *ux)
{

	_DIAGASSERT(ux != NULL);
	_DIAGASSERT(u != NULL);

	(void)memcpy(ux->ut_name, u->ut_name, sizeof(u->ut_name));
	(void)memcpy(ux->ut_line, u->ut_line, sizeof(u->ut_line));
	(void)memcpy(ux->ut_host, u->ut_host, sizeof(u->ut_host));
	ux->ut_tv.tv_sec = u->ut_time;
	ux->ut_tv.tv_usec = 0;
	(void)memset(&ux->ut_ss, 0, sizeof(ux->ut_ss));
	ux->ut_pid = 0;
	ux->ut_type = USER_PROCESS;
	ux->ut_session = 0;
	ux->ut_exit.e_termination = 0;
	ux->ut_exit.e_exit = 0;
}

struct lastlogx *
getlastlogx(const char *fname, uid_t uid, struct lastlogx *ll)
{
	DBT key, data;
	DB *db;

	_DIAGASSERT(fname != NULL);
	_DIAGASSERT(ll != NULL);

#ifdef __minix
	db = dbopen(fname, O_RDONLY, 0, DB_HASH, NULL);
#else
	db = dbopen(fname, O_RDONLY|O_SHLOCK, 0, DB_HASH, NULL);
#endif

	if (db == NULL)
		return NULL;
#ifdef __minix
	if (flock(db->fd(db), LOCK_SH) < 0)
		return NULL;
#endif

	key.data = &uid;
	key.size = sizeof(uid);

	if ((db->get)(db, &key, &data, 0) != 0)
		goto error;

	if (data.size != sizeof(*ll)) {
		errno = EFTYPE;
		goto error;
	}

	if (ll == NULL)
		if ((ll = malloc(sizeof(*ll))) == NULL)
			goto done;

	(void)memcpy(ll, data.data, sizeof(*ll));
	goto done;
error:
	ll = NULL;
done:
	(db->close)(db);
	return ll;
}

int
updlastlogx(const char *fname, uid_t uid, struct lastlogx *ll)
{
	DBT key, data;
	int error = 0;
	DB *db;

	_DIAGASSERT(fname != NULL);
	_DIAGASSERT(ll != NULL);

#ifndef __minix
	db = dbopen(fname, O_RDWR|O_CREAT|O_EXLOCK, 0644, DB_HASH, NULL);
#else 
	db = dbopen(fname, O_RDWR|O_CREAT, 0644, DB_HASH, NULL);
#endif

	if (db == NULL)
		return -1;

#ifdef __minix
	if (flock(db->fd(db), LOCK_EX) < 0)
		return -1;
#endif
	key.data = &uid;
	key.size = sizeof(uid);
	data.data = ll;
	data.size = sizeof(*ll);
	if ((db->put)(db, &key, &data, 0) != 0)
		error = -1;

	(db->close)(db);
	return error;
}

#if defined(__minix) && defined(__weak_alias)
__weak_alias(getlastlogx, __getlastlogx50)
__weak_alias(getutmp, __getutmp50)
__weak_alias(getutmpx, __getutmpx50)
__weak_alias(getutxent, __getutxent50)
__weak_alias(getutxid, __getutxid50)
__weak_alias(getutxline, __getutxline50)
__weak_alias(pututxline, __pututxline50)
__weak_alias(updlastlogx, __updlastlogx50)
__weak_alias(updwtmpx, __updwtmpx50)
#endif
