/*	$NetBSD: fetch.c,v 1.1.1.8 2009/08/21 15:12:27 joerg Exp $	*/
/*-
 * Copyright (c) 1998-2004 Dag-Erling Coïdan Smørgrav
 * Copyright (c) 2008 Joerg Sonnenberger <joerg@NetBSD.org>
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
 * $FreeBSD: fetch.c,v 1.41 2007/12/19 00:26:36 des Exp $
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif
#ifndef NETBSD
#include <nbcompat.h>
#endif

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fetch.h"
#include "common.h"

auth_t	 fetchAuthMethod;
int	 fetchLastErrCode;
char	 fetchLastErrString[MAXERRSTRING];
int	 fetchTimeout;
volatile int	 fetchRestartCalls = 1;
int	 fetchDebug;


/*** Local data **************************************************************/

/*
 * Error messages for parser errors
 */
#define URL_MALFORMED		1
#define URL_BAD_SCHEME		2
#define URL_BAD_PORT		3
static struct fetcherr url_errlist[] = {
	{ URL_MALFORMED,	FETCH_URL,	"Malformed URL" },
	{ URL_BAD_SCHEME,	FETCH_URL,	"Invalid URL scheme" },
	{ URL_BAD_PORT,		FETCH_URL,	"Invalid server port" },
	{ -1,			FETCH_UNKNOWN,	"Unknown parser error" }
};


/*** Public API **************************************************************/

/*
 * Select the appropriate protocol for the URL scheme, and return a
 * read-only stream connected to the document referenced by the URL.
 * Also fill out the struct url_stat.
 */
fetchIO *
fetchXGet(struct url *URL, struct url_stat *us, const char *flags)
{

	if (us != NULL) {
		us->size = -1;
		us->atime = us->mtime = 0;
	}
	if (strcasecmp(URL->scheme, SCHEME_FILE) == 0)
		return (fetchXGetFile(URL, us, flags));
	else if (strcasecmp(URL->scheme, SCHEME_FTP) == 0)
		return (fetchXGetFTP(URL, us, flags));
	else if (strcasecmp(URL->scheme, SCHEME_HTTP) == 0)
		return (fetchXGetHTTP(URL, us, flags));
	else if (strcasecmp(URL->scheme, SCHEME_HTTPS) == 0)
		return (fetchXGetHTTP(URL, us, flags));
	url_seterr(URL_BAD_SCHEME);
	return (NULL);
}

/*
 * Select the appropriate protocol for the URL scheme, and return a
 * read-only stream connected to the document referenced by the URL.
 */
fetchIO *
fetchGet(struct url *URL, const char *flags)
{
	return (fetchXGet(URL, NULL, flags));
}

/*
 * Select the appropriate protocol for the URL scheme, and return a
 * write-only stream connected to the document referenced by the URL.
 */
fetchIO *
fetchPut(struct url *URL, const char *flags)
{

	if (strcasecmp(URL->scheme, SCHEME_FILE) == 0)
		return (fetchPutFile(URL, flags));
	else if (strcasecmp(URL->scheme, SCHEME_FTP) == 0)
		return (fetchPutFTP(URL, flags));
	else if (strcasecmp(URL->scheme, SCHEME_HTTP) == 0)
		return (fetchPutHTTP(URL, flags));
	else if (strcasecmp(URL->scheme, SCHEME_HTTPS) == 0)
		return (fetchPutHTTP(URL, flags));
	url_seterr(URL_BAD_SCHEME);
	return (NULL);
}

/*
 * Select the appropriate protocol for the URL scheme, and return the
 * size of the document referenced by the URL if it exists.
 */
int
fetchStat(struct url *URL, struct url_stat *us, const char *flags)
{

	if (us != NULL) {
		us->size = -1;
		us->atime = us->mtime = 0;
	}
	if (strcasecmp(URL->scheme, SCHEME_FILE) == 0)
		return (fetchStatFile(URL, us, flags));
	else if (strcasecmp(URL->scheme, SCHEME_FTP) == 0)
		return (fetchStatFTP(URL, us, flags));
	else if (strcasecmp(URL->scheme, SCHEME_HTTP) == 0)
		return (fetchStatHTTP(URL, us, flags));
	else if (strcasecmp(URL->scheme, SCHEME_HTTPS) == 0)
		return (fetchStatHTTP(URL, us, flags));
	url_seterr(URL_BAD_SCHEME);
	return (-1);
}

/*
 * Select the appropriate protocol for the URL scheme, and return a
 * list of files in the directory pointed to by the URL.
 */
int
fetchList(struct url_list *ue, struct url *URL, const char *pattern,
    const char *flags)
{

	if (strcasecmp(URL->scheme, SCHEME_FILE) == 0)
		return (fetchListFile(ue, URL, pattern, flags));
	else if (strcasecmp(URL->scheme, SCHEME_FTP) == 0)
		return (fetchListFTP(ue, URL, pattern, flags));
	else if (strcasecmp(URL->scheme, SCHEME_HTTP) == 0)
		return (fetchListHTTP(ue, URL, pattern, flags));
	else if (strcasecmp(URL->scheme, SCHEME_HTTPS) == 0)
		return (fetchListHTTP(ue, URL, pattern, flags));
	url_seterr(URL_BAD_SCHEME);
	return -1;
}

/*
 * Attempt to parse the given URL; if successful, call fetchXGet().
 */
fetchIO *
fetchXGetURL(const char *URL, struct url_stat *us, const char *flags)
{
	struct url *u;
	fetchIO *f;

	if ((u = fetchParseURL(URL)) == NULL)
		return (NULL);

	f = fetchXGet(u, us, flags);

	fetchFreeURL(u);
	return (f);
}

/*
 * Attempt to parse the given URL; if successful, call fetchGet().
 */
fetchIO *
fetchGetURL(const char *URL, const char *flags)
{
	return (fetchXGetURL(URL, NULL, flags));
}

/*
 * Attempt to parse the given URL; if successful, call fetchPut().
 */
fetchIO *
fetchPutURL(const char *URL, const char *flags)
{
	struct url *u;
	fetchIO *f;

	if ((u = fetchParseURL(URL)) == NULL)
		return (NULL);

	f = fetchPut(u, flags);

	fetchFreeURL(u);
	return (f);
}

/*
 * Attempt to parse the given URL; if successful, call fetchStat().
 */
int
fetchStatURL(const char *URL, struct url_stat *us, const char *flags)
{
	struct url *u;
	int s;

	if ((u = fetchParseURL(URL)) == NULL)
		return (-1);

	s = fetchStat(u, us, flags);

	fetchFreeURL(u);
	return (s);
}

/*
 * Attempt to parse the given URL; if successful, call fetchList().
 */
int
fetchListURL(struct url_list *ue, const char *URL, const char *pattern,
    const char *flags)
{
	struct url *u;
	int rv;

	if ((u = fetchParseURL(URL)) == NULL)
		return -1;

	rv = fetchList(ue, u, pattern, flags);

	fetchFreeURL(u);
	return rv;
}

/*
 * Make a URL
 */
struct url *
fetchMakeURL(const char *scheme, const char *host, int port, const char *doc,
    const char *user, const char *pwd)
{
	struct url *u;

	if (!scheme || (!host && !doc)) {
		url_seterr(URL_MALFORMED);
		return (NULL);
	}

	if (port < 0 || port > 65535) {
		url_seterr(URL_BAD_PORT);
		return (NULL);
	}

	/* allocate struct url */
	if ((u = calloc(1, sizeof(*u))) == NULL) {
		fetch_syserr();
		return (NULL);
	}

	if ((u->doc = strdup(doc ? doc : "/")) == NULL) {
		fetch_syserr();
		free(u);
		return (NULL);
	}

#define seturl(x) snprintf(u->x, sizeof(u->x), "%s", x)
	seturl(scheme);
	seturl(host);
	seturl(user);
	seturl(pwd);
#undef seturl
	u->port = port;

	return (u);
}

int
fetch_urlpath_safe(char x)
{
	if ((x >= '0' && x <= '9') || (x >= 'A' && x <= 'Z') ||
	    (x >= 'a' && x <= 'z'))
		return 1;

	switch (x) {
	case '$':
	case '-':
	case '_':
	case '.':
	case '+':
	case '!':
	case '*':
	case '\'':
	case '(':
	case ')':
	case ',':
	/* The following are allowed in segment and path components: */
	case '?':
	case ':':
	case '@':
	case '&':
	case '=':
	case '/':
	case ';':
	/* If something is already quoted... */
	case '%':
		return 1;
	default:
		return 0;
	}
}

/*
 * Copy an existing URL.
 */
struct url *
fetchCopyURL(const struct url *src)
{
	struct url *dst;
	char *doc;

	/* allocate struct url */
	if ((dst = malloc(sizeof(*dst))) == NULL) {
		fetch_syserr();
		return (NULL);
	}
	if ((doc = strdup(src->doc)) == NULL) {
		fetch_syserr();
		free(dst);
		return (NULL);
	}
	*dst = *src;
	dst->doc = doc;

	return dst;
}

/*
 * Split an URL into components. URL syntax is:
 * [method:/][/[user[:pwd]@]host[:port]/][document]
 * This almost, but not quite, RFC1738 URL syntax.
 */
struct url *
fetchParseURL(const char *URL)
{
	const char *p, *q;
	struct url *u;
	size_t i, count;
	int pre_quoted;

	/* allocate struct url */
	if ((u = calloc(1, sizeof(*u))) == NULL) {
		fetch_syserr();
		return (NULL);
	}

	if (*URL == '/') {
		pre_quoted = 0;
		strcpy(u->scheme, SCHEME_FILE);
		p = URL;
		goto quote_doc;
	}
	if (strncmp(URL, "file:", 5) == 0) {
		pre_quoted = 1;
		strcpy(u->scheme, SCHEME_FILE);
		URL += 5;
		if (URL[0] != '/' || URL[1] != '/' || URL[2] != '/') {
			url_seterr(URL_MALFORMED);
			goto ouch;
		}
		p = URL + 2;
		goto quote_doc;
	}
	if (strncmp(URL, "http:", 5) == 0 ||
	    strncmp(URL, "https:", 6) == 0) {
		pre_quoted = 1;
		if (URL[4] == ':') {
			strcpy(u->scheme, SCHEME_HTTP);
			URL += 5;
		} else {
			strcpy(u->scheme, SCHEME_HTTPS);
			URL += 6;
		}

		if (URL[0] != '/' || URL[1] != '/') {
			url_seterr(URL_MALFORMED);
			goto ouch;
		}
		URL += 2;
		p = URL;
		goto find_user;
	}
	if (strncmp(URL, "ftp:", 4) == 0) {
		pre_quoted = 1;
		strcpy(u->scheme, SCHEME_FTP);
		URL += 4;
		if (URL[0] != '/' || URL[1] != '/') {
			url_seterr(URL_MALFORMED);
			goto ouch;
		}
		URL += 2;
		p = URL;
		goto find_user;			
	}

	url_seterr(URL_BAD_SCHEME);
	goto ouch;

find_user:
	p = strpbrk(URL, "/@");
	if (p != NULL && *p == '@') {
		/* username */
		for (q = URL, i = 0; (*q != ':') && (*q != '@'); q++) {
			if (i < URL_USERLEN)
				u->user[i++] = *q;
		}

		/* password */
		if (*q == ':') {
			for (q++, i = 0; (*q != '@'); q++)
				if (i < URL_PWDLEN)
					u->pwd[i++] = *q;
		}

		p++;
	} else {
		p = URL;
	}

	/* hostname */
#ifdef INET6
	if (*p == '[' && (q = strchr(p + 1, ']')) != NULL &&
	    (*++q == '\0' || *q == '/' || *q == ':')) {
		if ((i = q - p - 2) > URL_HOSTLEN)
			i = URL_HOSTLEN;
		strncpy(u->host, ++p, i);
		p = q;
	} else
#endif
		for (i = 0; *p && (*p != '/') && (*p != ':'); p++)
			if (i < URL_HOSTLEN)
				u->host[i++] = *p;

	/* port */
	if (*p == ':') {
		for (q = ++p; *q && (*q != '/'); q++)
			if (isdigit((unsigned char)*q))
				u->port = u->port * 10 + (*q - '0');
			else {
				/* invalid port */
				url_seterr(URL_BAD_PORT);
				goto ouch;
			}
		p = q;
	}

	/* document */
	if (!*p)
		p = "/";

quote_doc:
	count = 1;
	for (i = 0; p[i] != '\0'; ++i) {
		if ((!pre_quoted && p[i] == '%') ||
		    !fetch_urlpath_safe(p[i]))
			count += 3;
		else
			++count;
	}

	if ((u->doc = malloc(count)) == NULL) {
		fetch_syserr();
		goto ouch;
	}
	for (i = 0; *p != '\0'; ++p) {
		if ((!pre_quoted && *p == '%') ||
		    !fetch_urlpath_safe(*p)) {
			u->doc[i++] = '%';
			if ((unsigned char)*p < 160)
				u->doc[i++] = '0' + ((unsigned char)*p) / 16;
			else
				u->doc[i++] = 'a' - 10 + ((unsigned char)*p) / 16;
			if ((unsigned char)*p % 16 < 10)
				u->doc[i++] = '0' + ((unsigned char)*p) % 16;
			else
				u->doc[i++] = 'a' - 10 + ((unsigned char)*p) % 16;
		} else
			u->doc[i++] = *p;
	}
	u->doc[i] = '\0';

	return (u);

ouch:
	free(u);
	return (NULL);
}

/*
 * Free a URL
 */
void
fetchFreeURL(struct url *u)
{
	free(u->doc);
	free(u);
}

static char
xdigit2digit(char digit)
{
	digit = tolower((unsigned char)digit);
	if (digit >= 'a' && digit <= 'f')
		digit = digit - 'a' + 10;
	else
		digit = digit - '0';

	return digit;
}

/*
 * Unquote whole URL.
 * Skips optional parts like query or fragment identifier.
 */ 
char *
fetchUnquotePath(struct url *url)
{
	char *unquoted;
	const char *iter;
	size_t i;

	if ((unquoted = malloc(strlen(url->doc) + 1)) == NULL)
		return NULL;

	for (i = 0, iter = url->doc; *iter != '\0'; ++iter) {
		if (*iter == '#' || *iter == '?')
			break;
		if (iter[0] != '%' ||
		    !isxdigit((unsigned char)iter[1]) ||
		    !isxdigit((unsigned char)iter[2])) {
			unquoted[i++] = *iter;
			continue;
		}
		unquoted[i++] = xdigit2digit(iter[1]) * 16 +
		    xdigit2digit(iter[2]);
		iter += 2;
	}
	unquoted[i] = '\0';
	return unquoted;
}


/*
 * Extract the file name component of a URL.
 */
char *
fetchUnquoteFilename(struct url *url)
{
	char *unquoted, *filename;
	const char *last_slash;

	if ((unquoted = fetchUnquotePath(url)) == NULL)
		return NULL;

	if ((last_slash = strrchr(unquoted, '/')) == NULL)
		return unquoted;
	filename = strdup(last_slash + 1);
	free(unquoted);
	return filename;
}

char *
fetchStringifyURL(const struct url *url)
{
	size_t total;
	char *doc;

	/* scheme :// user : pwd @ host :port doc */
	total = strlen(url->scheme) + 3 + strlen(url->user) + 1 +
	    strlen(url->pwd) + 1 + strlen(url->host) + 6 + strlen(url->doc) + 1;
	if ((doc = malloc(total)) == NULL)
		return NULL;
	if (url->port != 0)
		snprintf(doc, total, "%s%s%s%s%s%s%s:%d%s",
		    url->scheme,
		    url->scheme[0] != '\0' ? "://" : "",
		    url->user,
		    url->pwd[0] != '\0' ? ":" : "",
		    url->pwd,
		    url->user[0] != '\0' || url->pwd[0] != '\0' ? "@" : "",
		    url->host,
		    (int)url->port,
		    url->doc);
	else {
		snprintf(doc, total, "%s%s%s%s%s%s%s%s",
		    url->scheme,
		    url->scheme[0] != '\0' ? "://" : "",
		    url->user,
		    url->pwd[0] != '\0' ? ":" : "",
		    url->pwd,
		    url->user[0] != '\0' || url->pwd[0] != '\0' ? "@" : "",
		    url->host,
		    url->doc);
	}
	return doc;
}
