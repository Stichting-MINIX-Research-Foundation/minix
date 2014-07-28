/*	$NetBSD: common.c,v 1.27 2010/06/13 21:38:09 joerg Exp $	*/
/*-
 * Copyright (c) 1998-2004 Dag-Erling Coïdan Smørgrav
 * Copyright (c) 2008, 2010 Joerg Sonnenberger <joerg@NetBSD.org>
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
 * $FreeBSD: common.c,v 1.53 2007/12/19 00:26:36 des Exp $
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif
#if !defined(NETBSD) && !defined(__minix)
#include <nbcompat.h>
#endif

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/uio.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <errno.h>
#if defined(HAVE_INTTYPES_H) || defined(NETBSD)
#include <inttypes.h>
#endif
#if !defined(NETBSD) && !defined(__minix)
#include <nbcompat/netdb.h>
#else
#include <netdb.h>
#endif
#include <pwd.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#ifndef MSG_NOSIGNAL
#include <signal.h>
#endif

#include "common.h"

/*** Local data **************************************************************/

/*
 * Error messages for resolver errors
 */
static struct fetcherr netdb_errlist[] = {
#ifdef EAI_NODATA
	{ EAI_NODATA,	FETCH_RESOLV,	"Host not found" },
#endif
	{ EAI_AGAIN,	FETCH_TEMP,	"Transient resolver failure" },
	{ EAI_FAIL,	FETCH_RESOLV,	"Non-recoverable resolver failure" },
	{ EAI_NONAME,	FETCH_RESOLV,	"No address record" },
	{ -1,		FETCH_UNKNOWN,	"Unknown resolver error" }
};

/*** Error-reporting functions ***********************************************/

/*
 * Map error code to string
 */
static struct fetcherr *
fetch_finderr(struct fetcherr *p, int e)
{
	while (p->num != -1 && p->num != e)
		p++;
	return (p);
}

/*
 * Set error code
 */
void
fetch_seterr(struct fetcherr *p, int e)
{
	p = fetch_finderr(p, e);
	fetchLastErrCode = p->cat;
	snprintf(fetchLastErrString, MAXERRSTRING, "%s", p->string);
}

/*
 * Set error code according to errno
 */
void
fetch_syserr(void)
{
	switch (errno) {
	case 0:
		fetchLastErrCode = FETCH_OK;
		break;
	case EPERM:
	case EACCES:
	case EROFS:
#ifdef EAUTH
	case EAUTH:
#endif
#ifdef ENEEDAUTH
	case ENEEDAUTH:
#endif
		fetchLastErrCode = FETCH_AUTH;
		break;
	case ENOENT:
	case EISDIR: /* XXX */
		fetchLastErrCode = FETCH_UNAVAIL;
		break;
	case ENOMEM:
		fetchLastErrCode = FETCH_MEMORY;
		break;
	case EBUSY:
	case EAGAIN:
		fetchLastErrCode = FETCH_TEMP;
		break;
	case EEXIST:
		fetchLastErrCode = FETCH_EXISTS;
		break;
	case ENOSPC:
		fetchLastErrCode = FETCH_FULL;
		break;
	case EADDRINUSE:
	case EADDRNOTAVAIL:
	case ENETDOWN:
	case ENETUNREACH:
#if defined(ENETRESET)
	case ENETRESET:
#endif
	case EHOSTUNREACH:
		fetchLastErrCode = FETCH_NETWORK;
		break;
#if defined(ECONNABORTED)
	case ECONNABORTED:
#endif
	case ECONNRESET:
		fetchLastErrCode = FETCH_ABORT;
		break;
	case ETIMEDOUT:
		fetchLastErrCode = FETCH_TIMEOUT;
		break;
	case ECONNREFUSED:
#if defined(EHOSTDOWN)
	case EHOSTDOWN:
#endif
		fetchLastErrCode = FETCH_DOWN;
		break;
default:
		fetchLastErrCode = FETCH_UNKNOWN;
	}
	snprintf(fetchLastErrString, MAXERRSTRING, "%s", strerror(errno));
}


/*
 * Emit status message
 */
void
fetch_info(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
}


/*** Network-related utility functions ***************************************/

/*
 * Return the default port for a scheme
 */
int
fetch_default_port(const char *scheme)
{
	struct servent *se;

	if ((se = getservbyname(scheme, "tcp")) != NULL)
		return (ntohs(se->s_port));
	if (strcasecmp(scheme, SCHEME_FTP) == 0)
		return (FTP_DEFAULT_PORT);
	if (strcasecmp(scheme, SCHEME_HTTP) == 0)
		return (HTTP_DEFAULT_PORT);
	return (0);
}

/*
 * Return the default proxy port for a scheme
 */
int
fetch_default_proxy_port(const char *scheme)
{
	if (strcasecmp(scheme, SCHEME_FTP) == 0)
		return (FTP_DEFAULT_PROXY_PORT);
	if (strcasecmp(scheme, SCHEME_HTTP) == 0)
		return (HTTP_DEFAULT_PROXY_PORT);
	return (0);
}


/*
 * Create a connection for an existing descriptor.
 */
conn_t *
fetch_reopen(int sd)
{
	conn_t *conn;

	/* allocate and fill connection structure */
	if ((conn = calloc(1, sizeof(*conn))) == NULL)
		return (NULL);
	conn->ftp_home = NULL;
	conn->cache_url = NULL;
	conn->next_buf = NULL;
	conn->next_len = 0;
	conn->sd = sd;
	return (conn);
}


/*
 * Bind a socket to a specific local address
 */
int
fetch_bind(int sd, int af, const char *addr)
{
	struct addrinfo hints, *res, *res0;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = af;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = 0;
	if (getaddrinfo(addr, NULL, &hints, &res0))
		return (-1);
	for (res = res0; res; res = res->ai_next) {
		if (bind(sd, res->ai_addr, res->ai_addrlen) == 0)
			return (0);
	}
	return (-1);
}


/*
 * Establish a TCP connection to the specified port on the specified host.
 */
conn_t *
fetch_connect(struct url *url, int af, int verbose)
{
	conn_t *conn;
	char pbuf[10];
	const char *bindaddr;
	struct addrinfo hints, *res, *res0;
	int sd, error;

	if (verbose)
		fetch_info("looking up %s", url->host);

	/* look up host name and set up socket address structure */
	snprintf(pbuf, sizeof(pbuf), "%d", url->port);
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = af;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = 0;
	if ((error = getaddrinfo(url->host, pbuf, &hints, &res0)) != 0) {
		netdb_seterr(error);
		return (NULL);
	}
	bindaddr = getenv("FETCH_BIND_ADDRESS");

	if (verbose)
		fetch_info("connecting to %s:%d", url->host, url->port);

	/* try to connect */
	for (sd = -1, res = res0; res; sd = -1, res = res->ai_next) {
		if ((sd = socket(res->ai_family, res->ai_socktype,
			 res->ai_protocol)) == -1)
			continue;
		if (bindaddr != NULL && *bindaddr != '\0' &&
		    fetch_bind(sd, res->ai_family, bindaddr) != 0) {
			fetch_info("failed to bind to '%s'", bindaddr);
			close(sd);
			continue;
		}
		if (connect(sd, res->ai_addr, res->ai_addrlen) == 0)
			break;
		close(sd);
	}
	freeaddrinfo(res0);
	if (sd == -1) {
		fetch_syserr();
		return (NULL);
	}

	if ((conn = fetch_reopen(sd)) == NULL) {
		fetch_syserr();
		close(sd);
		return (NULL);
	}
	conn->cache_url = fetchCopyURL(url);
	conn->cache_af = af;
	return (conn);
}

static conn_t *connection_cache;
static int cache_global_limit = 0;
static int cache_per_host_limit = 0;

/*
 * Initialise cache with the given limits.
 */
void
fetchConnectionCacheInit(int global_limit, int per_host_limit)
{

	if (global_limit < 0)
		cache_global_limit = INT_MAX;
	else if (per_host_limit > global_limit)
		cache_global_limit = per_host_limit;
	else
		cache_global_limit = global_limit;
	if (per_host_limit < 0)
		cache_per_host_limit = INT_MAX;
	else
		cache_per_host_limit = per_host_limit;
}

/*
 * Flush cache and free all associated resources.
 */
void
fetchConnectionCacheClose(void)
{
	conn_t *conn;

	while ((conn = connection_cache) != NULL) {
		connection_cache = conn->next_cached;
		(*conn->cache_close)(conn);
	}
}

/*
 * Check connection cache for an existing entry matching
 * protocol/host/port/user/password/family.
 */
conn_t *
fetch_cache_get(const struct url *url, int af)
{
	conn_t *conn, *last_conn = NULL;

	for (conn = connection_cache; conn; conn = conn->next_cached) {
		if (conn->cache_url->port == url->port &&
		    strcmp(conn->cache_url->scheme, url->scheme) == 0 &&
		    strcmp(conn->cache_url->host, url->host) == 0 &&
		    strcmp(conn->cache_url->user, url->user) == 0 &&
		    strcmp(conn->cache_url->pwd, url->pwd) == 0 &&
		    (conn->cache_af == AF_UNSPEC || af == AF_UNSPEC ||
		     conn->cache_af == af)) {
			if (last_conn != NULL)
				last_conn->next_cached = conn->next_cached;
			else
				connection_cache = conn->next_cached;
			return conn;
		}
	}

	return NULL;
}

/*
 * Put the connection back into the cache for reuse.
 * If the connection is freed due to LRU or if the cache
 * is explicitly closed, the given callback is called.
 */
void
fetch_cache_put(conn_t *conn, int (*closecb)(conn_t *))
{
	conn_t *iter, *last;
	int global_count, host_count;

	if (conn->cache_url == NULL || cache_global_limit == 0) {
		(*closecb)(conn);
		return;
	}

	global_count = host_count = 0;
	last = NULL;
	for (iter = connection_cache; iter;
	    last = iter, iter = iter->next_cached) {
		++global_count;
		if (strcmp(conn->cache_url->host, iter->cache_url->host) == 0)
			++host_count;
		if (global_count < cache_global_limit &&
		    host_count < cache_per_host_limit)
			continue;
		--global_count;
		if (last != NULL)
			last->next_cached = iter->next_cached;
		else
			connection_cache = iter->next_cached;
		(*iter->cache_close)(iter);
	}

	conn->cache_close = closecb;
	conn->next_cached = connection_cache;
	connection_cache = conn;
}

/*
 * Enable SSL on a connection.
 */
int
fetch_ssl(conn_t *conn, int verbose)
{

#ifdef WITH_SSL
	/* Init the SSL library and context */
	if (!SSL_library_init()){
		fprintf(stderr, "SSL library init failed\n");
		return (-1);
	}

	SSL_load_error_strings();

	conn->ssl_meth = SSLv23_client_method();
	conn->ssl_ctx = SSL_CTX_new(conn->ssl_meth);
	SSL_CTX_set_mode(conn->ssl_ctx, SSL_MODE_AUTO_RETRY);

	conn->ssl = SSL_new(conn->ssl_ctx);
	if (conn->ssl == NULL){
		fprintf(stderr, "SSL context creation failed\n");
		return (-1);
	}
	SSL_set_fd(conn->ssl, conn->sd);
	if (SSL_connect(conn->ssl) == -1){
		ERR_print_errors_fp(stderr);
		return (-1);
	}

	if (verbose) {
		X509_NAME *name;
		char *str;

		fprintf(stderr, "SSL connection established using %s\n",
		    SSL_get_cipher(conn->ssl));
		conn->ssl_cert = SSL_get_peer_certificate(conn->ssl);
		name = X509_get_subject_name(conn->ssl_cert);
		str = X509_NAME_oneline(name, 0, 0);
		printf("Certificate subject: %s\n", str);
		free(str);
		name = X509_get_issuer_name(conn->ssl_cert);
		str = X509_NAME_oneline(name, 0, 0);
		printf("Certificate issuer: %s\n", str);
		free(str);
	}

	return (0);
#else
	(void)conn;
	(void)verbose;
	fprintf(stderr, "SSL support disabled\n");
	return (-1);
#endif
}


/*
 * Read a character from a connection w/ timeout
 */
ssize_t
fetch_read(conn_t *conn, char *buf, size_t len)
{
	struct timeval now, timeout, waittv;
	fd_set readfds;
	ssize_t rlen;
	int r;

	if (len == 0)
		return 0;

	if (conn->next_len != 0) {
		if (conn->next_len < len)
			len = conn->next_len;
		memmove(buf, conn->next_buf, len);
		conn->next_len -= len;
		conn->next_buf += len;
		return len;
	}

	if (fetchTimeout) {
		FD_ZERO(&readfds);
		gettimeofday(&timeout, NULL);
		timeout.tv_sec += fetchTimeout;
	}

	for (;;) {
		while (fetchTimeout && !FD_ISSET(conn->sd, &readfds)) {
			FD_SET(conn->sd, &readfds);
			gettimeofday(&now, NULL);
			waittv.tv_sec = timeout.tv_sec - now.tv_sec;
			waittv.tv_usec = timeout.tv_usec - now.tv_usec;
			if (waittv.tv_usec < 0) {
				waittv.tv_usec += 1000000;
				waittv.tv_sec--;
			}
			if (waittv.tv_sec < 0) {
				errno = ETIMEDOUT;
				fetch_syserr();
				return (-1);
			}
			errno = 0;
			r = select(conn->sd + 1, &readfds, NULL, NULL, &waittv);
			if (r == -1) {
				if (errno == EINTR && fetchRestartCalls)
					continue;
				fetch_syserr();
				return (-1);
			}
		}
#ifdef WITH_SSL
		if (conn->ssl != NULL)
			rlen = SSL_read(conn->ssl, buf, len);
		else
#endif
			rlen = read(conn->sd, buf, len);
		if (rlen >= 0)
			break;
	
		if (errno != EINTR || !fetchRestartCalls)
			return (-1);
	}
	return (rlen);
}


/*
 * Read a line of text from a connection w/ timeout
 */
#define MIN_BUF_SIZE 1024

int
fetch_getln(conn_t *conn)
{
	char *tmp, *next;
	size_t tmpsize;
	ssize_t len;

	if (conn->buf == NULL) {
		if ((conn->buf = malloc(MIN_BUF_SIZE)) == NULL) {
			errno = ENOMEM;
			return (-1);
		}
		conn->bufsize = MIN_BUF_SIZE;
	}

	conn->buflen = 0;
	next = NULL;

	do {
		/*
		 * conn->bufsize != conn->buflen at this point,
		 * so the buffer can be NUL-terminated below for
		 * the case of len == 0.
		 */
		len = fetch_read(conn, conn->buf + conn->buflen,
		    conn->bufsize - conn->buflen);
		if (len == -1)
			return (-1);
		if (len == 0)
			break;
		next = memchr(conn->buf + conn->buflen, '\n', len);
		conn->buflen += len;
		if (conn->buflen == conn->bufsize && next == NULL) {
			tmp = conn->buf;
			tmpsize = conn->bufsize * 2;
			if (tmpsize < conn->bufsize) {
				errno = ENOMEM;
				return (-1);
			}
			if ((tmp = realloc(tmp, tmpsize)) == NULL) {
				errno = ENOMEM;
				return (-1);
			}
			conn->buf = tmp;
			conn->bufsize = tmpsize;
		}
	} while (next == NULL);

	if (next != NULL) {
		*next = '\0';
		conn->next_buf = next + 1;
		conn->next_len = conn->buflen - (conn->next_buf - conn->buf);
		conn->buflen = next - conn->buf;
	} else {
		conn->buf[conn->buflen] = '\0';
		conn->next_len = 0;
	}
	return (0);
}

/*
 * Write a vector to a connection w/ timeout
 * Note: can modify the iovec.
 */
ssize_t
fetch_write(conn_t *conn, const void *buf, size_t len)
{
	struct timeval now, timeout, waittv;
	fd_set writefds;
	ssize_t wlen, total;
	int r;
#ifndef MSG_NOSIGNAL
	static int killed_sigpipe;
#endif

#ifndef MSG_NOSIGNAL
	if (!killed_sigpipe) {
		signal(SIGPIPE, SIG_IGN);
		killed_sigpipe = 1;
	}
#endif


	if (fetchTimeout) {
		FD_ZERO(&writefds);
		gettimeofday(&timeout, NULL);
		timeout.tv_sec += fetchTimeout;
	}

	total = 0;
	while (len) {
		while (fetchTimeout && !FD_ISSET(conn->sd, &writefds)) {
			FD_SET(conn->sd, &writefds);
			gettimeofday(&now, NULL);
			waittv.tv_sec = timeout.tv_sec - now.tv_sec;
			waittv.tv_usec = timeout.tv_usec - now.tv_usec;
			if (waittv.tv_usec < 0) {
				waittv.tv_usec += 1000000;
				waittv.tv_sec--;
			}
			if (waittv.tv_sec < 0) {
				errno = ETIMEDOUT;
				fetch_syserr();
				return (-1);
			}
			errno = 0;
			r = select(conn->sd + 1, NULL, &writefds, NULL, &waittv);
			if (r == -1) {
				if (errno == EINTR && fetchRestartCalls)
					continue;
				return (-1);
			}
		}
		errno = 0;
#ifdef WITH_SSL
		if (conn->ssl != NULL)
			wlen = SSL_write(conn->ssl, buf, len);
		else
#endif
#ifndef MSG_NOSIGNAL
			wlen = send(conn->sd, buf, len, 0);
#else
			wlen = send(conn->sd, buf, len, MSG_NOSIGNAL);
#endif
		if (wlen == 0) {
			/* we consider a short write a failure */
			errno = EPIPE;
			fetch_syserr();
			return (-1);
		}
		if (wlen < 0) {
			if (errno == EINTR && fetchRestartCalls)
				continue;
			return (-1);
		}
		total += wlen;
		buf = (const char *)buf + wlen;
		len -= wlen;
	}
	return (total);
}


/*
 * Close connection
 */
int
fetch_close(conn_t *conn)
{
	int ret;

	ret = close(conn->sd);
	if (conn->cache_url)
		fetchFreeURL(conn->cache_url);
	free(conn->ftp_home);
	free(conn->buf);
	free(conn);
	return (ret);
}


/*** Directory-related utility functions *************************************/

int
fetch_add_entry(struct url_list *ue, struct url *base, const char *name,
    int pre_quoted)
{
	struct url *tmp;
	char *tmp_name;
	size_t base_doc_len, name_len, i;
	unsigned char c;

	if (strchr(name, '/') != NULL ||
	    strcmp(name, "..") == 0 ||
	    strcmp(name, ".") == 0)
		return 0;

	if (strcmp(base->doc, "/") == 0)
		base_doc_len = 0;
	else
		base_doc_len = strlen(base->doc);

	name_len = 1;
	for (i = 0; name[i] != '\0'; ++i) {
		if ((!pre_quoted && name[i] == '%') ||
		    !fetch_urlpath_safe(name[i]))
			name_len += 3;
		else
			++name_len;
	}

	tmp_name = malloc( base_doc_len + name_len + 1);
	if (tmp_name == NULL) {
		errno = ENOMEM;
		fetch_syserr();
		return (-1);
	}

	if (ue->length + 1 >= ue->alloc_size) {
		tmp = realloc(ue->urls, (ue->alloc_size * 2 + 1) * sizeof(*tmp));
		if (tmp == NULL) {
			free(tmp_name);
			errno = ENOMEM;
			fetch_syserr();
			return (-1);
		}
		ue->alloc_size = ue->alloc_size * 2 + 1;
		ue->urls = tmp;
	}

	tmp = ue->urls + ue->length;
	strcpy(tmp->scheme, base->scheme);
	strcpy(tmp->user, base->user);
	strcpy(tmp->pwd, base->pwd);
	strcpy(tmp->host, base->host);
	tmp->port = base->port;
	tmp->doc = tmp_name;
	memcpy(tmp->doc, base->doc, base_doc_len);
	tmp->doc[base_doc_len] = '/';

	for (i = base_doc_len + 1; *name != '\0'; ++name) {
		if ((!pre_quoted && *name == '%') ||
		    !fetch_urlpath_safe(*name)) {
			tmp->doc[i++] = '%';
			c = (unsigned char)*name / 16;
			if (c < 10)
				tmp->doc[i++] = '0' + c;
			else
				tmp->doc[i++] = 'a' - 10 + c;
			c = (unsigned char)*name % 16;
			if (c < 10)
				tmp->doc[i++] = '0' + c;
			else
				tmp->doc[i++] = 'a' - 10 + c;
		} else {
			tmp->doc[i++] = *name;
		}
	}
	tmp->doc[i] = '\0';

	tmp->offset = 0;
	tmp->length = 0;
	tmp->last_modified = -1;

	++ue->length;

	return (0);
}

void
fetchInitURLList(struct url_list *ue)
{
	ue->length = ue->alloc_size = 0;
	ue->urls = NULL;
}

int
fetchAppendURLList(struct url_list *dst, const struct url_list *src)
{
	size_t i, j, len;

	len = dst->length + src->length;
	if (len > dst->alloc_size) {
		struct url *tmp;

		tmp = realloc(dst->urls, len * sizeof(*tmp));
		if (tmp == NULL) {
			errno = ENOMEM;
			fetch_syserr();
			return (-1);
		}
		dst->alloc_size = len;
		dst->urls = tmp;
	}

	for (i = 0, j = dst->length; i < src->length; ++i, ++j) {
		dst->urls[j] = src->urls[i];
		dst->urls[j].doc = strdup(src->urls[i].doc);
		if (dst->urls[j].doc == NULL) {
			while (i-- > 0)
				free(dst->urls[j].doc);
			fetch_syserr();
			return -1;
		}
	}
	dst->length = len;

	return 0;
}

void
fetchFreeURLList(struct url_list *ue)
{
	size_t i;

	for (i = 0; i < ue->length; ++i)
		free(ue->urls[i].doc);
	free(ue->urls);
	ue->length = ue->alloc_size = 0;
}


/*** Authentication-related utility functions ********************************/

static const char *
fetch_read_word(FILE *f)
{
	static char word[1024];

	if (fscanf(f, " %1023s ", word) != 1)
		return (NULL);
	return (word);
}

/*
 * Get authentication data for a URL from .netrc
 */
int
fetch_netrc_auth(struct url *url)
{
	char fn[PATH_MAX];
	const char *word;
	char *p;
	FILE *f;

	if ((p = getenv("NETRC")) != NULL) {
		if (snprintf(fn, sizeof(fn), "%s", p) >= (int)sizeof(fn)) {
			fetch_info("$NETRC specifies a file name "
			    "longer than PATH_MAX");
			return (-1);
		}
	} else {
		if ((p = getenv("HOME")) != NULL) {
			struct passwd *pwd;

			if ((pwd = getpwuid(getuid())) == NULL ||
			    (p = pwd->pw_dir) == NULL)
				return (-1);
		}
		if (snprintf(fn, sizeof(fn), "%s/.netrc", p) >= (int)sizeof(fn))
			return (-1);
	}

	if ((f = fopen(fn, "r")) == NULL)
		return (-1);
	while ((word = fetch_read_word(f)) != NULL) {
		if (strcmp(word, "default") == 0)
			break;
		if (strcmp(word, "machine") == 0 &&
		    (word = fetch_read_word(f)) != NULL &&
		    strcasecmp(word, url->host) == 0) {
			break;
		}
	}
	if (word == NULL)
		goto ferr;
	while ((word = fetch_read_word(f)) != NULL) {
		if (strcmp(word, "login") == 0) {
			if ((word = fetch_read_word(f)) == NULL)
				goto ferr;
			if (snprintf(url->user, sizeof(url->user),
				"%s", word) > (int)sizeof(url->user)) {
				fetch_info("login name in .netrc is too long");
				url->user[0] = '\0';
			}
		} else if (strcmp(word, "password") == 0) {
			if ((word = fetch_read_word(f)) == NULL)
				goto ferr;
			if (snprintf(url->pwd, sizeof(url->pwd),
				"%s", word) > (int)sizeof(url->pwd)) {
				fetch_info("password in .netrc is too long");
				url->pwd[0] = '\0';
			}
		} else if (strcmp(word, "account") == 0) {
			if ((word = fetch_read_word(f)) == NULL)
				goto ferr;
			/* XXX not supported! */
		} else {
			break;
		}
	}
	fclose(f);
	return (0);
 ferr:
	fclose(f);
	return (-1);
}

/*
 * The no_proxy environment variable specifies a set of domains for
 * which the proxy should not be consulted; the contents is a comma-,
 * or space-separated list of domain names.  A single asterisk will
 * override all proxy variables and no transactions will be proxied
 * (for compatability with lynx and curl, see the discussion at
 * <http://curl.haxx.se/mail/archive_pre_oct_99/0009.html>).
 */
int
fetch_no_proxy_match(const char *host)
{
	const char *no_proxy, *p, *q;
	size_t h_len, d_len;

	if ((no_proxy = getenv("NO_PROXY")) == NULL &&
	    (no_proxy = getenv("no_proxy")) == NULL)
		return (0);

	/* asterisk matches any hostname */
	if (strcmp(no_proxy, "*") == 0)
		return (1);

	h_len = strlen(host);
	p = no_proxy;
	do {
		/* position p at the beginning of a domain suffix */
		while (*p == ',' || isspace((unsigned char)*p))
			p++;

		/* position q at the first separator character */
		for (q = p; *q; ++q)
			if (*q == ',' || isspace((unsigned char)*q))
				break;

		d_len = q - p;
		if (d_len > 0 && h_len > d_len &&
		    strncasecmp(host + h_len - d_len,
			p, d_len) == 0) {
			/* domain name matches */
			return (1);
		}

		p = q + 1;
	} while (*q);

	return (0);
}

struct fetchIO {
	void *io_cookie;
	ssize_t (*io_read)(void *, void *, size_t);
	ssize_t (*io_write)(void *, const void *, size_t);
	void (*io_close)(void *);
};

void
fetchIO_close(fetchIO *f)
{
	if (f->io_close != NULL)
		(*f->io_close)(f->io_cookie);

	free(f);
}

fetchIO *
fetchIO_unopen(void *io_cookie, ssize_t (*io_read)(void *, void *, size_t),
    ssize_t (*io_write)(void *, const void *, size_t),
    void (*io_close)(void *))
{
	fetchIO *f;

	f = malloc(sizeof(*f));
	if (f == NULL)
		return f;

	f->io_cookie = io_cookie;
	f->io_read = io_read;
	f->io_write = io_write;
	f->io_close = io_close;

	return f;
}

ssize_t
fetchIO_read(fetchIO *f, void *buf, size_t len)
{
	if (f->io_read == NULL)
		return EBADF;
	return (*f->io_read)(f->io_cookie, buf, len);
}

ssize_t
fetchIO_write(fetchIO *f, const void *buf, size_t len)
{
	if (f->io_read == NULL)
		return EBADF;
	return (*f->io_write)(f->io_cookie, buf, len);
}
