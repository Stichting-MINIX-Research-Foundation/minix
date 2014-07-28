/*	$NetBSD: ftp.c,v 1.35 2010/03/21 16:48:43 joerg Exp $	*/
/*-
 * Copyright (c) 1998-2004 Dag-Erling Coïdan Smørgrav
 * Copyright (c) 2008, 2009, 2010 Joerg Sonnenberger <joerg@NetBSD.org>
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
 * $FreeBSD: ftp.c,v 1.101 2008/01/23 20:57:59 des Exp $
 */

/*
 * Portions of this code were taken from or based on ftpio.c:
 *
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.org> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * Major Changelog:
 *
 * Dag-Erling Coïdan Smørgrav
 * 9 Jun 1998
 *
 * Incorporated into libfetch
 *
 * Jordan K. Hubbard
 * 17 Jan 1996
 *
 * Turned inside out. Now returns xfers as new file ids, not as a special
 * `state' of FTP_t
 *
 * $ftpioId: ftpio.c,v 1.30 1998/04/11 07:28:53 phk Exp $
 *
 */

#ifdef __linux__
/* Keep this down to Linux, it can create surprises else where. */
#define _GNU_SOURCE
#endif

#if HAVE_CONFIG_H
#include "config.h"
#endif
#if !defined(NETBSD) && !defined(__minix)
#include <nbcompat.h>
#endif

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#if defined(HAVE_INTTYPES_H) || defined(NETBSD)
#include <inttypes.h>
#endif
#include <stdarg.h>
#if !defined(NETBSD) && !defined(__minix)
#include <nbcompat/netdb.h>
#include <nbcompat/stdio.h>
#else
#include <netdb.h>
#include <stdio.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "common.h"
#include "ftperr.h"

#define FTP_ANONYMOUS_USER	"anonymous"

#define FTP_CONNECTION_ALREADY_OPEN	125
#define FTP_OPEN_DATA_CONNECTION	150
#define FTP_OK				200
#define FTP_FILE_STATUS			213
#define FTP_SERVICE_READY		220
#define FTP_TRANSFER_COMPLETE		226
#define FTP_PASSIVE_MODE		227
#define FTP_LPASSIVE_MODE		228
#define FTP_EPASSIVE_MODE		229
#define FTP_LOGGED_IN			230
#define FTP_FILE_ACTION_OK		250
#define FTP_DIRECTORY_CREATED		257 /* multiple meanings */
#define FTP_FILE_CREATED		257 /* multiple meanings */
#define FTP_WORKING_DIRECTORY		257 /* multiple meanings */
#define FTP_NEED_PASSWORD		331
#define FTP_NEED_ACCOUNT		332
#define FTP_FILE_OK			350
#define FTP_SYNTAX_ERROR		500
#define FTP_PROTOCOL_ERROR		999

#define isftpreply(foo)				\
	(isdigit((unsigned char)foo[0]) &&	\
	    isdigit((unsigned char)foo[1]) &&	\
	    isdigit((unsigned char)foo[2]) &&	\
	    (foo[3] == ' ' || foo[3] == '\0'))
#define isftpinfo(foo) \
	(isdigit((unsigned char)foo[0]) &&	\
	    isdigit((unsigned char)foo[1]) &&	\
	    isdigit((unsigned char)foo[2]) &&	\
	    foo[3] == '-')

#define MINBUFSIZE 4096

#ifdef INET6
/*
 * Translate IPv4 mapped IPv6 address to IPv4 address
 */
static void
unmappedaddr(struct sockaddr_in6 *sin6, socklen_t *len)
{
	struct sockaddr_in *sin4;
	uint32_t addr;
	int port;

	if (sin6->sin6_family != AF_INET6 ||
	    !IN6_IS_ADDR_V4MAPPED(&sin6->sin6_addr))
		return;
	sin4 = (struct sockaddr_in *)sin6;
	addr = *(uint32_t *)&sin6->sin6_addr.s6_addr[12];
	port = sin6->sin6_port;
	memset(sin4, 0, sizeof(struct sockaddr_in));
	sin4->sin_addr.s_addr = addr;
	sin4->sin_port = port;
	sin4->sin_family = AF_INET;
	*len = sizeof(struct sockaddr_in);
#ifdef HAVE_SA_LEN
	sin4->sin_len = sizeof(struct sockaddr_in);
#endif
}
#endif

/*
 * Get server response
 */
static int
ftp_chkerr(conn_t *conn)
{
	if (fetch_getln(conn) == -1) {
		fetch_syserr();
		return (-1);
	}
	if (isftpinfo(conn->buf)) {
		while (conn->buflen && !isftpreply(conn->buf)) {
			if (fetch_getln(conn) == -1) {
				fetch_syserr();
				return (-1);
			}
		}
	}

	while (conn->buflen &&
	    isspace((unsigned char)conn->buf[conn->buflen - 1]))
		conn->buflen--;
	conn->buf[conn->buflen] = '\0';

	if (!isftpreply(conn->buf)) {
		ftp_seterr(FTP_PROTOCOL_ERROR);
		return (-1);
	}

	conn->err = (conn->buf[0] - '0') * 100
	    + (conn->buf[1] - '0') * 10
	    + (conn->buf[2] - '0');

	return (conn->err);
}

/*
 * Send a command and check reply
 */
#ifndef __minix
static int
ftp_cmd(conn_t *conn, const char *fmt, ...)
{
	va_list ap;
	size_t len;
	char *msg;
	int r;

	va_start(ap, fmt);
	len = vasprintf(&msg, fmt, ap);
	va_end(ap);

	if (msg == NULL) {
		errno = ENOMEM;
		fetch_syserr();
		return (-1);
	}

	r = fetch_write(conn, msg, len);
	free(msg);

	if (r == -1) {
		fetch_syserr();
		return (-1);
	}

	return (ftp_chkerr(conn));
}
#else
static int
ftp_cmd(conn_t *conn, const char *fmt, ...)
{
	va_list ap;
	size_t len;
	char msg[MINBUFSIZE];
	int r;

	va_start(ap, fmt);
	len = vsnprintf(&msg[0], MINBUFSIZE, fmt, ap);
	va_end(ap);

	if (len >= MINBUFSIZE) {
		errno = ENOMEM;
		fetch_syserr();
		return (-1);
	}

	r = fetch_write(conn, msg, len);

	if (r == -1) {
		fetch_syserr();
		return (-1);
	}

	return (ftp_chkerr(conn));
}
#endif
/*
 * Return a pointer to the filename part of a path
 */
static const char *
ftp_filename(const char *file, int *len, int *type, int subdir)
{
	const char *s;

	if ((s = strrchr(file, '/')) == NULL || subdir)
		s = file;
	else
		s = s + 1;
	*len = strlen(s);
	if (*len > 7 && strncmp(s + *len - 7, ";type=", 6) == 0) {
		*type = s[*len - 1];
		*len -= 7;
	} else {
		*type = '\0';
	}
	return (s);
}

/*
 * Get current working directory from the reply to a CWD, PWD or CDUP
 * command.
 */
static int
ftp_pwd(conn_t *conn, char **pwd)
{
	char *src, *dst, *end;
	int q;

	if (conn->err != FTP_WORKING_DIRECTORY &&
	    conn->err != FTP_FILE_ACTION_OK)
		return (FTP_PROTOCOL_ERROR);
	end = conn->buf + conn->buflen;
	src = conn->buf + 4;
	if (src >= end || *src++ != '"')
		return (FTP_PROTOCOL_ERROR);
	*pwd = malloc(end - src + 1);
	if (*pwd == NULL)
		return (FTP_PROTOCOL_ERROR);
	for (q = 0, dst = *pwd; src < end; ++src) {
		if (!q && *src == '"')
			q = 1;
		else if (q && *src != '"')
			break;
		else if (q)
			*dst++ = '"', q = 0;
		else
			*dst++ = *src;
	}
	*dst = '\0';
	if (**pwd != '/') {
		free(*pwd);
		*pwd = NULL;
		return (FTP_PROTOCOL_ERROR);
	}
	return (FTP_OK);
}

/*
 * Change working directory to the directory that contains the specified
 * file.
 */
static int
ftp_cwd(conn_t *conn, const char *path, int subdir)
{
	const char *beg, *end;
	char *pwd, *dst;
	int e, i, len;

	if (*path != '/') {
		ftp_seterr(501);
		return (-1);
	}
	++path;

	/* Simple case: still in the home directory and no directory change. */
	if (conn->ftp_home == NULL && strchr(path, '/') == NULL &&
	    (!subdir || *path == '\0'))
		return 0;

	if ((e = ftp_cmd(conn, "PWD\r\n")) != FTP_WORKING_DIRECTORY ||
	    (e = ftp_pwd(conn, &pwd)) != FTP_OK) {
		ftp_seterr(e);
		return (-1);
	}
	if (conn->ftp_home == NULL && (conn->ftp_home = strdup(pwd)) == NULL) {
		fetch_syserr();
		free(pwd);
		return (-1);
	}
	if (*path == '/') {
		while (path[1] == '/')
			++path;
		dst = strdup(path);
	} else if (strcmp(conn->ftp_home, "/") == 0) {
		dst = strdup(path - 1);
	} else {
#ifndef __minix
		asprintf(&dst, "%s/%s", conn->ftp_home, path);
#else
		if((dst = malloc(sizeof(char)*MINBUFSIZE)) != NULL) {
			len = snprintf(dst, MINBUFSIZE, "%s/%s", conn->ftp_home, path);
			
			if(len >= MINBUFSIZE) {
				free(dst);
				dst = NULL;
			}
		}
#endif
	}
	if (dst == NULL) {
		fetch_syserr();
		free(pwd);
		return (-1);
	}

	if (subdir)
		end = dst + strlen(dst);
	else
		end = strrchr(dst, '/');

	for (;;) {
		len = strlen(pwd);

		/* Look for a common prefix between PWD and dir to fetch. */
		for (i = 0; i <= len && i <= end - dst; ++i)
			if (pwd[i] != dst[i])
				break;
		/* Keep going up a dir until we have a matching prefix. */
		if (strcmp(pwd, "/") == 0)
			break;
		if (pwd[i] == '\0' && (dst[i - 1] == '/' || dst[i] == '/'))
			break;
		free(pwd);
		if ((e = ftp_cmd(conn, "CDUP\r\n")) != FTP_FILE_ACTION_OK ||
		    (e = ftp_cmd(conn, "PWD\r\n")) != FTP_WORKING_DIRECTORY ||
		    (e = ftp_pwd(conn, &pwd)) != FTP_OK) {
			ftp_seterr(e);
			free(dst);
			return (-1);
		}
	}
	free(pwd);

#ifdef FTP_COMBINE_CWDS
	/* Skip leading slashes, even "////". */
	for (beg = dst + i; beg < end && *beg == '/'; ++beg, ++i)
		/* nothing */ ;

	/* If there is no trailing dir, we're already there. */
	if (beg >= end) {
		free(dst);
		return (0);
	}

	/* Change to the directory all in one chunk (e.g., foo/bar/baz). */
	e = ftp_cmd(conn, "CWD %.*s\r\n", (int)(end - beg), beg);
	if (e == FTP_FILE_ACTION_OK) {
		free(dst);
		return (0);
	}
#endif /* FTP_COMBINE_CWDS */

	/* That didn't work so go back to legacy behavior (multiple CWDs). */
	for (beg = dst + i; beg < end; beg = dst + i + 1) {
		while (*beg == '/')
			++beg, ++i;
		for (++i; dst + i < end && dst[i] != '/'; ++i)
			/* nothing */ ;
		e = ftp_cmd(conn, "CWD %.*s\r\n", dst + i - beg, beg);
		if (e != FTP_FILE_ACTION_OK) {
			free(dst);
			ftp_seterr(e);
			return (-1);
		}
	}
	free(dst);
	return (0);
}

/*
 * Set transfer mode and data type
 */
static int
ftp_mode_type(conn_t *conn, int mode, int type)
{
	int e;

	switch (mode) {
	case 0:
	case 's':
		mode = 'S';
	case 'S':
		break;
	default:
		return (FTP_PROTOCOL_ERROR);
	}
	if ((e = ftp_cmd(conn, "MODE %c\r\n", mode)) != FTP_OK) {
		if (mode == 'S') {
			/*
			 * Stream mode is supposed to be the default - so
			 * much so that some servers not only do not
			 * support any other mode, but do not support the
			 * MODE command at all.
			 *
			 * If "MODE S" fails, it is unlikely that we
			 * previously succeeded in setting a different
			 * mode.  Therefore, we simply hope that the
			 * server is already in the correct mode, and
			 * silently ignore the failure.
			 */
		} else {
			return (e);
		}
	}

	switch (type) {
	case 0:
	case 'i':
		type = 'I';
	case 'I':
		break;
	case 'a':
		type = 'A';
	case 'A':
		break;
	case 'd':
		type = 'D';
	case 'D':
		/* can't handle yet */
	default:
		return (FTP_PROTOCOL_ERROR);
	}
	if ((e = ftp_cmd(conn, "TYPE %c\r\n", type)) != FTP_OK)
		return (e);

	return (FTP_OK);
}

/*
 * Request and parse file stats
 */
static int
ftp_stat(conn_t *conn, const char *file, struct url_stat *us)
{
	char *ln;
	const char *filename;
	int filenamelen, type;
	struct tm tm;
	time_t t;
	int e;

	us->size = -1;
	us->atime = us->mtime = 0;

	filename = ftp_filename(file, &filenamelen, &type, 0);

	if ((e = ftp_mode_type(conn, 0, type)) != FTP_OK) {
		ftp_seterr(e);
		return (-1);
	}

	e = ftp_cmd(conn, "SIZE %.*s\r\n", filenamelen, filename);
	if (e != FTP_FILE_STATUS) {
		ftp_seterr(e);
		return (-1);
	}
	for (ln = conn->buf + 4; *ln && isspace((unsigned char)*ln); ln++)
		/* nothing */ ;
	for (us->size = 0; *ln && isdigit((unsigned char)*ln); ln++)
		us->size = us->size * 10 + *ln - '0';
	if (*ln && !isspace((unsigned char)*ln)) {
		ftp_seterr(FTP_PROTOCOL_ERROR);
		us->size = -1;
		return (-1);
	}
	if (us->size == 0)
		us->size = -1;

	e = ftp_cmd(conn, "MDTM %.*s\r\n", filenamelen, filename);
	if (e != FTP_FILE_STATUS) {
		ftp_seterr(e);
		return (-1);
	}
	for (ln = conn->buf + 4; *ln && isspace((unsigned char)*ln); ln++)
		/* nothing */ ;
	switch (strspn(ln, "0123456789")) {
	case 14:
		break;
	case 15:
		ln++;
		ln[0] = '2';
		ln[1] = '0';
		break;
	default:
		ftp_seterr(FTP_PROTOCOL_ERROR);
		return (-1);
	}
	if (sscanf(ln, "%04d%02d%02d%02d%02d%02d",
	    &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
	    &tm.tm_hour, &tm.tm_min, &tm.tm_sec) != 6) {
		ftp_seterr(FTP_PROTOCOL_ERROR);
		return (-1);
	}
	tm.tm_mon--;
	tm.tm_year -= 1900;
	tm.tm_isdst = -1;
	t = timegm(&tm);
	if (t == (time_t)-1)
		t = time(NULL);
	us->mtime = t;
	us->atime = t;

	return (0);
}

/*
 * I/O functions for FTP
 */
struct ftpio {
	conn_t	*cconn;		/* Control connection */
	conn_t	*dconn;		/* Data connection */
	int	 dir;		/* Direction */
	int	 eof;		/* EOF reached */
	int	 err;		/* Error code */
};

static ssize_t	 ftp_readfn(void *, void *, size_t);
static ssize_t	 ftp_writefn(void *, const void *, size_t);
static void	 ftp_closefn(void *);

static ssize_t
ftp_readfn(void *v, void *buf, size_t len)
{
	struct ftpio *io;
	int r;

	io = (struct ftpio *)v;
	if (io == NULL) {
		errno = EBADF;
		return (-1);
	}
	if (io->cconn == NULL || io->dconn == NULL || io->dir == O_WRONLY) {
		errno = EBADF;
		return (-1);
	}
	if (io->err) {
		errno = io->err;
		return (-1);
	}
	if (io->eof)
		return (0);
	r = fetch_read(io->dconn, buf, len);
	if (r > 0)
		return (r);
	if (r == 0) {
		io->eof = 1;
		return (0);
	}
	if (errno != EINTR)
		io->err = errno;
	return (-1);
}

static ssize_t
ftp_writefn(void *v, const void *buf, size_t len)
{
	struct ftpio *io;
	int w;

	io = (struct ftpio *)v;
	if (io == NULL) {
		errno = EBADF;
		return (-1);
	}
	if (io->cconn == NULL || io->dconn == NULL || io->dir == O_RDONLY) {
		errno = EBADF;
		return (-1);
	}
	if (io->err) {
		errno = io->err;
		return (-1);
	}
	w = fetch_write(io->dconn, buf, len);
	if (w >= 0)
		return (w);
	if (errno != EINTR)
		io->err = errno;
	return (-1);
}

static int
ftp_disconnect(conn_t *conn)
{
	ftp_cmd(conn, "QUIT\r\n");
	return fetch_close(conn);
}

static void
ftp_closefn(void *v)
{
	struct ftpio *io;
	int r;

	io = (struct ftpio *)v;
	if (io == NULL) {
		errno = EBADF;
		return;
	}
	if (io->dir == -1)
		return;
	if (io->cconn == NULL || io->dconn == NULL) {
		errno = EBADF;
		return;
	}
	fetch_close(io->dconn);
	io->dconn = NULL;
	io->dir = -1;
	r = ftp_chkerr(io->cconn);
	fetch_cache_put(io->cconn, ftp_disconnect);
	free(io);
	return;
}

static fetchIO *
ftp_setup(conn_t *cconn, conn_t *dconn, int mode)
{
	struct ftpio *io;
	fetchIO *f;

	if (cconn == NULL || dconn == NULL)
		return (NULL);
	if ((io = malloc(sizeof(*io))) == NULL)
		return (NULL);
	io->cconn = cconn;
	io->dconn = dconn;
	io->dir = mode;
	io->eof = io->err = 0;
	f = fetchIO_unopen(io, ftp_readfn, ftp_writefn, ftp_closefn);
	if (f == NULL)
		free(io);
	return (f);
}

/*
 * Transfer file
 */
static fetchIO *
ftp_transfer(conn_t *conn, const char *oper, const char *file, const char *op_arg,
    int mode, off_t offset, const char *flags)
{
	union anonymous {
		struct sockaddr_storage ss;
		struct sockaddr sa;
		struct sockaddr_in6 sin6;
		struct sockaddr_in sin4;
	} u;
	const char *bindaddr;
	const char *filename;
	int filenamelen, type;
	int low, pasv, verbose;
	int e, sd = -1;
	socklen_t l;
	char *s;
	fetchIO *df;

	/* check flags */
	low = CHECK_FLAG('l');
	pasv = !CHECK_FLAG('a');
	verbose = CHECK_FLAG('v');

	/* passive mode */
	if (!pasv)
		pasv = ((s = getenv("FTP_PASSIVE_MODE")) != NULL &&
		    strncasecmp(s, "no", 2) != 0);

	/* isolate filename */
	filename = ftp_filename(file, &filenamelen, &type, op_arg != NULL);

	/* set transfer mode and data type */
	if ((e = ftp_mode_type(conn, 0, type)) != FTP_OK)
		goto ouch;

	/* find our own address, bind, and listen */
	l = sizeof(u.ss);
	if (getsockname(conn->sd, &u.sa, &l) == -1)
		goto sysouch;
#ifdef INET6
	if (u.ss.ss_family == AF_INET6)
		unmappedaddr(&u.sin6, &l);
#endif

retry_mode:

	/* open data socket */
	if ((sd = socket(u.ss.ss_family, SOCK_STREAM, IPPROTO_TCP)) == -1) {
		fetch_syserr();
		return (NULL);
	}

	if (pasv) {
		unsigned char addr[64];
		char *ln, *p;
		unsigned int i;
		int port;

		/* send PASV command */
		if (verbose)
			fetch_info("setting passive mode");
		switch (u.ss.ss_family) {
		case AF_INET:
			if ((e = ftp_cmd(conn, "PASV\r\n")) != FTP_PASSIVE_MODE)
				goto ouch;
			break;
#ifdef INET6
		case AF_INET6:
			if ((e = ftp_cmd(conn, "EPSV\r\n")) != FTP_EPASSIVE_MODE) {
				if (e == -1)
					goto ouch;
				if ((e = ftp_cmd(conn, "LPSV\r\n")) !=
				    FTP_LPASSIVE_MODE)
					goto ouch;
			}
			break;
#endif
		default:
			e = FTP_PROTOCOL_ERROR; /* XXX: error code should be prepared */
			goto ouch;
		}

		/*
		 * Find address and port number. The reply to the PASV command
		 * is IMHO the one and only weak point in the FTP protocol.
		 */
		ln = conn->buf;
		switch (e) {
		case FTP_PASSIVE_MODE:
		case FTP_LPASSIVE_MODE:
			for (p = ln + 3; *p && !isdigit((unsigned char)*p); p++)
				/* nothing */ ;
			if (!*p) {
				e = FTP_PROTOCOL_ERROR;
				goto ouch;
			}
			l = (e == FTP_PASSIVE_MODE ? 6 : 21);
			for (i = 0; *p && i < l; i++, p++)
				addr[i] = strtol(p, &p, 10);
			if (i < l) {
				e = FTP_PROTOCOL_ERROR;
				goto ouch;
			}
			break;
		case FTP_EPASSIVE_MODE:
			for (p = ln + 3; *p && *p != '('; p++)
				/* nothing */ ;
			if (!*p) {
				e = FTP_PROTOCOL_ERROR;
				goto ouch;
			}
			++p;
			if (sscanf(p, "%c%c%c%d%c", &addr[0], &addr[1], &addr[2],
				&port, &addr[3]) != 5 ||
			    addr[0] != addr[1] ||
			    addr[0] != addr[2] || addr[0] != addr[3]) {
				e = FTP_PROTOCOL_ERROR;
				goto ouch;
			}
			break;
		case FTP_SYNTAX_ERROR:
			if (verbose)
				fetch_info("passive mode failed");
			/* Close socket and retry with passive mode. */
			pasv = 0;
			close(sd);
			sd = -1;
			goto retry_mode;
		}

		/* seek to required offset */
		if (offset)
			if (ftp_cmd(conn, "REST %lu\r\n", (unsigned long)offset) != FTP_FILE_OK)
				goto sysouch;

		/* construct sockaddr for data socket */
		l = sizeof(u.ss);
		if (getpeername(conn->sd, &u.sa, &l) == -1)
			goto sysouch;
#ifdef INET6
		if (u.ss.ss_family == AF_INET6)
			unmappedaddr(&u.sin6, &l);
#endif
		switch (u.ss.ss_family) {
#ifdef INET6
		case AF_INET6:
			if (e == FTP_EPASSIVE_MODE)
				u.sin6.sin6_port = htons(port);
			else {
				memcpy(&u.sin6.sin6_addr, addr + 2, 16);
				memcpy(&u.sin6.sin6_port, addr + 19, 2);
			}
			break;
#endif
		case AF_INET:
			if (e == FTP_EPASSIVE_MODE)
				u.sin4.sin_port = htons(port);
			else {
				memcpy(&u.sin4.sin_addr, addr, 4);
				memcpy(&u.sin4.sin_port, addr + 4, 2);
			}
			break;
		default:
			e = FTP_PROTOCOL_ERROR; /* XXX: error code should be prepared */
			break;
		}

		/* connect to data port */
		if (verbose)
			fetch_info("opening data connection");
		bindaddr = getenv("FETCH_BIND_ADDRESS");
		if (bindaddr != NULL && *bindaddr != '\0' &&
		    fetch_bind(sd, u.ss.ss_family, bindaddr) != 0)
			goto sysouch;
		if (connect(sd, &u.sa, l) == -1)
			goto sysouch;

		/* make the server initiate the transfer */
		if (verbose)
			fetch_info("initiating transfer");
		if (op_arg)
			e = ftp_cmd(conn, "%s%s%s\r\n", oper, *op_arg ? " " : "", op_arg);
		else
			e = ftp_cmd(conn, "%s %.*s\r\n", oper,
			    filenamelen, filename);
		if (e != FTP_CONNECTION_ALREADY_OPEN && e != FTP_OPEN_DATA_CONNECTION)
			goto ouch;

	} else {
		uint32_t a;
		uint16_t p;
#if defined(IPV6_PORTRANGE) || defined(IP_PORTRANGE)
		int arg;
#endif
		int d;
#ifdef INET6
		char *ap;
		char hname[INET6_ADDRSTRLEN];
#endif

		switch (u.ss.ss_family) {
#ifdef INET6
		case AF_INET6:
			u.sin6.sin6_port = 0;
#ifdef IPV6_PORTRANGE
			arg = low ? IPV6_PORTRANGE_DEFAULT : IPV6_PORTRANGE_HIGH;
			if (setsockopt(sd, IPPROTO_IPV6, IPV6_PORTRANGE,
				(char *)&arg, sizeof(arg)) == -1)
				goto sysouch;
#endif
			break;
#endif
		case AF_INET:
			u.sin4.sin_port = 0;
#ifdef IP_PORTRANGE
			arg = low ? IP_PORTRANGE_DEFAULT : IP_PORTRANGE_HIGH;
			if (setsockopt(sd, IPPROTO_IP, IP_PORTRANGE,
				(char *)&arg, sizeof(arg)) == -1)
				goto sysouch;
#endif
			break;
		}
		if (verbose)
			fetch_info("binding data socket");
		if (bind(sd, &u.sa, l) == -1)
			goto sysouch;
		if (listen(sd, 1) == -1)
			goto sysouch;

		/* find what port we're on and tell the server */
		if (getsockname(sd, &u.sa, &l) == -1)
			goto sysouch;
		switch (u.ss.ss_family) {
		case AF_INET:
			a = ntohl(u.sin4.sin_addr.s_addr);
			p = ntohs(u.sin4.sin_port);
			e = ftp_cmd(conn, "PORT %d,%d,%d,%d,%d,%d\r\n",
			    (a >> 24) & 0xff, (a >> 16) & 0xff,
			    (a >> 8) & 0xff, a & 0xff,
			    (p >> 8) & 0xff, p & 0xff);
			break;
#ifdef INET6
		case AF_INET6:
#define UC(b)	(((int)b)&0xff)
			e = -1;
			u.sin6.sin6_scope_id = 0;
			if (getnameinfo(&u.sa, l,
				hname, sizeof(hname),
				NULL, 0, NI_NUMERICHOST) == 0) {
				e = ftp_cmd(conn, "EPRT |%d|%s|%d|\r\n", 2, hname,
				    htons(u.sin6.sin6_port));
				if (e == -1)
					goto ouch;
			}
			if (e != FTP_OK) {
				ap = (char *)&u.sin6.sin6_addr;
				e = ftp_cmd(conn,
				    "LPRT %d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\r\n",
				    6, 16,
				    UC(ap[0]), UC(ap[1]), UC(ap[2]), UC(ap[3]),
				    UC(ap[4]), UC(ap[5]), UC(ap[6]), UC(ap[7]),
				    UC(ap[8]), UC(ap[9]), UC(ap[10]), UC(ap[11]),
				    UC(ap[12]), UC(ap[13]), UC(ap[14]), UC(ap[15]),
				    2,
				    (ntohs(u.sin6.sin6_port) >> 8) & 0xff,
				    ntohs(u.sin6.sin6_port)        & 0xff);
			}
			break;
#endif
		default:
			e = FTP_PROTOCOL_ERROR; /* XXX: error code should be prepared */
			goto ouch;
		}
		if (e != FTP_OK)
			goto ouch;

#ifndef __minix
		/* seek to required offset */
		if (offset)
			if (ftp_cmd(conn, "REST %llu\r\n", (unsigned long long)offset) != FTP_FILE_OK)
				goto sysouch;
#else
/* seek to required offset */
		if (offset)
			if (ftp_cmd(conn, "REST %lu\r\n", (unsigned long)offset) != FTP_FILE_OK)
				goto sysouch;
#endif

		/* make the server initiate the transfer */
		if (verbose)
			fetch_info("initiating transfer");
		if (op_arg)
			e = ftp_cmd(conn, "%s%s%s\r\n", oper, *op_arg ? " " : "", op_arg);
		else
			e = ftp_cmd(conn, "%s %.*s\r\n", oper,
			    filenamelen, filename);
		if (e != FTP_CONNECTION_ALREADY_OPEN && e != FTP_OPEN_DATA_CONNECTION)
			goto ouch;

		/* accept the incoming connection and go to town */
		if ((d = accept(sd, NULL, NULL)) == -1)
			goto sysouch;
		close(sd);
		sd = d;
	}

	if ((df = ftp_setup(conn, fetch_reopen(sd), mode)) == NULL)
		goto sysouch;
	return (df);

sysouch:
	fetch_syserr();
	if (sd >= 0)
		close(sd);
	return (NULL);

ouch:
	if (e != -1)
		ftp_seterr(e);
	if (sd >= 0)
		close(sd);
	return (NULL);
}

/*
 * Authenticate
 */
static int
ftp_authenticate(conn_t *conn, struct url *url, struct url *purl)
{
	const char *user, *pwd, *login_name;
	char pbuf[URL_USERLEN + 1 + URL_HOSTLEN + 1];
	int e, len;

	/* XXX FTP_AUTH, and maybe .netrc */

	/* send user name and password */
	if (url->user[0] == '\0')
		fetch_netrc_auth(url);
	user = url->user;
	if (*user == '\0')
		user = getenv("FTP_LOGIN");
	if (user == NULL || *user == '\0')
		user = FTP_ANONYMOUS_USER;
	if (purl && url->port == fetch_default_port(url->scheme))
		e = ftp_cmd(conn, "USER %s@%s\r\n", user, url->host);
	else if (purl)
		e = ftp_cmd(conn, "USER %s@%s@%d\r\n", user, url->host, url->port);
	else
		e = ftp_cmd(conn, "USER %s\r\n", user);

	/* did the server request a password? */
	if (e == FTP_NEED_PASSWORD) {
		pwd = url->pwd;
		if (*pwd == '\0')
			pwd = getenv("FTP_PASSWORD");
		if (pwd == NULL || *pwd == '\0') {
			if ((login_name = getlogin()) == 0)
				login_name = FTP_ANONYMOUS_USER;
			if ((len = snprintf(pbuf, URL_USERLEN + 2, "%s@", login_name)) < 0)
				len = 0;
			else if (len > URL_USERLEN + 1)
				len = URL_USERLEN + 1;
			gethostname(pbuf + len, sizeof(pbuf) - len);
			/* MAXHOSTNAMELEN can differ from URL_HOSTLEN + 1 */
			pbuf[sizeof(pbuf) - 1] = '\0';
			pwd = pbuf;
		}
		e = ftp_cmd(conn, "PASS %s\r\n", pwd);
	}

	return (e);
}

/*
 * Log on to FTP server
 */
static conn_t *
ftp_connect(struct url *url, struct url *purl, const char *flags)
{
	conn_t *conn;
	int e, direct, verbose;
#ifdef INET6
	int af = AF_UNSPEC;
#else
	int af = AF_INET;
#endif

	direct = CHECK_FLAG('d');
	verbose = CHECK_FLAG('v');
	if (CHECK_FLAG('4'))
		af = AF_INET;
#ifdef INET6
	else if (CHECK_FLAG('6'))
		af = AF_INET6;
#endif
	if (direct)
		purl = NULL;

	/* check for proxy */
	if (purl) {
		/* XXX proxy authentication! */
		/* XXX connetion caching */
		if (!purl->port)
			purl->port = fetch_default_port(purl->scheme);

		conn = fetch_connect(purl, af, verbose);
	} else {
		/* no proxy, go straight to target */
		if (!url->port)
			url->port = fetch_default_port(url->scheme);

		while ((conn = fetch_cache_get(url, af)) != NULL) {
			e = ftp_cmd(conn, "NOOP\r\n");
			if (e == FTP_OK)
				return conn;
			fetch_close(conn);
		}
		conn = fetch_connect(url, af, verbose);
		purl = NULL;
	}

	/* check connection */
	if (conn == NULL)
		/* fetch_connect() has already set an error code */
		return (NULL);

	/* expect welcome message */
	if ((e = ftp_chkerr(conn)) != FTP_SERVICE_READY)
		goto fouch;

	/* authenticate */
	if ((e = ftp_authenticate(conn, url, purl)) != FTP_LOGGED_IN)
		goto fouch;

	/* TODO: Request extended features supported, if any (RFC 3659). */

	/* done */
	return (conn);

fouch:
	if (e != -1)
		ftp_seterr(e);
	fetch_close(conn);
	return (NULL);
}

/*
 * Check the proxy settings
 */
static struct url *
ftp_get_proxy(struct url * url, const char *flags)
{
	struct url *purl;
	char *p;

	if (flags != NULL && strchr(flags, 'd') != NULL)
		return (NULL);
	if (fetch_no_proxy_match(url->host))
		return (NULL);
	if (((p = getenv("FTP_PROXY")) || (p = getenv("ftp_proxy")) ||
		(p = getenv("HTTP_PROXY")) || (p = getenv("http_proxy"))) &&
	    *p && (purl = fetchParseURL(p)) != NULL) {
		if (!*purl->scheme) {
			if (getenv("FTP_PROXY") || getenv("ftp_proxy"))
				strcpy(purl->scheme, SCHEME_FTP);
			else
				strcpy(purl->scheme, SCHEME_HTTP);
		}
		if (!purl->port)
			purl->port = fetch_default_proxy_port(purl->scheme);
		if (strcasecmp(purl->scheme, SCHEME_FTP) == 0 ||
		    strcasecmp(purl->scheme, SCHEME_HTTP) == 0)
			return (purl);
		fetchFreeURL(purl);
	}
	return (NULL);
}

/*
 * Process an FTP request
 */
fetchIO *
ftp_request(struct url *url, const char *op, const char *op_arg,
    struct url_stat *us, struct url *purl, const char *flags)
{
	fetchIO *f;
	char *path;
	conn_t *conn;
	int if_modified_since, oflag;
	struct url_stat local_us;

	/* check if we should use HTTP instead */
	if (purl && strcasecmp(purl->scheme, SCHEME_HTTP) == 0) {
		if (strcmp(op, "STAT") == 0)
			return (http_request(url, "HEAD", us, purl, flags));
		else if (strcmp(op, "RETR") == 0)
			return (http_request(url, "GET", us, purl, flags));
		/*
		 * Our HTTP code doesn't support PUT requests yet, so try
		 * a direct connection.
		 */
	}

	/* connect to server */
	conn = ftp_connect(url, purl, flags);
	if (purl)
		fetchFreeURL(purl);
	if (conn == NULL)
		return (NULL);

	if ((path = fetchUnquotePath(url)) == NULL) {
		fetch_syserr();
		return NULL;
	}

	/* change directory */
	if (ftp_cwd(conn, path, op_arg != NULL) == -1) {
		free(path);
		return (NULL);
	}

	if_modified_since = CHECK_FLAG('i');
	if (if_modified_since && us == NULL)
		us = &local_us;

	/* stat file */
	if (us && ftp_stat(conn, path, us) == -1
	    && fetchLastErrCode != FETCH_PROTO
	    && fetchLastErrCode != FETCH_UNAVAIL) {
		free(path);
		return (NULL);
	}

	if (if_modified_since && url->last_modified > 0 &&
	    url->last_modified >= us->mtime) {
		free(path);
		fetchLastErrCode = FETCH_UNCHANGED;
		snprintf(fetchLastErrString, MAXERRSTRING, "Unchanged");
		return NULL;
	}

	/* just a stat */
	if (strcmp(op, "STAT") == 0) {
		free(path);
		return fetchIO_unopen(NULL, NULL, NULL, NULL);
	}
	if (strcmp(op, "STOR") == 0 || strcmp(op, "APPE") == 0)
		oflag = O_WRONLY;
	else
		oflag = O_RDONLY;

	/* initiate the transfer */
	f = (ftp_transfer(conn, op, path, op_arg, oflag, url->offset, flags));
	free(path);
	return f;
}

/*
 * Get and stat file
 */
fetchIO *
fetchXGetFTP(struct url *url, struct url_stat *us, const char *flags)
{
	return (ftp_request(url, "RETR", NULL, us, ftp_get_proxy(url, flags), flags));
}

/*
 * Get file
 */
fetchIO *
fetchGetFTP(struct url *url, const char *flags)
{
	return (fetchXGetFTP(url, NULL, flags));
}

/*
 * Put file
 */
fetchIO *
fetchPutFTP(struct url *url, const char *flags)
{
	return (ftp_request(url, CHECK_FLAG('a') ? "APPE" : "STOR", NULL, NULL,
	    ftp_get_proxy(url, flags), flags));
}

/*
 * Get file stats
 */
int
fetchStatFTP(struct url *url, struct url_stat *us, const char *flags)
{
	fetchIO *f;

	f = ftp_request(url, "STAT", NULL, us, ftp_get_proxy(url, flags), flags);
	if (f == NULL)
		return (-1);
	fetchIO_close(f);
	return (0);
}

/*
 * List a directory
 */
int
fetchListFTP(struct url_list *ue, struct url *url, const char *pattern, const char *flags)
{
	fetchIO *f;
	char buf[2 * PATH_MAX], *eol, *eos;
	ssize_t len;
	size_t cur_off;
	int ret;

	/* XXX What about proxies? */
	if (pattern == NULL || strcmp(pattern, "*") == 0)
		pattern = "";
	f = ftp_request(url, "NLST", pattern, NULL, ftp_get_proxy(url, flags), flags);
	if (f == NULL)
		return -1;

	cur_off = 0;
	ret = 0;

	while ((len = fetchIO_read(f, buf + cur_off, sizeof(buf) - cur_off)) > 0) {
		cur_off += len;
		while ((eol = memchr(buf, '\n', cur_off)) != NULL) {
			if (len == eol - buf)
				break;
			if (eol != buf) {
				if (eol[-1] == '\r')
					eos = eol - 1;
				else
					eos = eol;
				*eos = '\0';
				ret = fetch_add_entry(ue, url, buf, 0);
				if (ret)
					break;
				cur_off -= eol - buf + 1;
				memmove(buf, eol + 1, cur_off);
			}
		}
		if (ret)
			break;
	}
	if (cur_off != 0 || len < 0) {
		/* Not RFC conform, bail out. */
		fetchIO_close(f);
		return -1;
	}
	fetchIO_close(f);
	return ret;
}
