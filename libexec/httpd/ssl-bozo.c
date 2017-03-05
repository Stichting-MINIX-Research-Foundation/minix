/*	$NetBSD: ssl-bozo.c,v 1.18 2014/07/17 06:27:52 mrg Exp $	*/

/*	$eterna: ssl-bozo.c,v 1.15 2011/11/18 09:21:15 mrg Exp $	*/

/*
 * Copyright (c) 1997-2014 Matthew R. Green
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer and
 *    dedication in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/* this code implements SSL and backend IO for bozohttpd */

#include <stdarg.h>
#include <stdio.h>
#include <syslog.h>
#include <unistd.h>

#include "bozohttpd.h"

#ifndef NO_SSL_SUPPORT

#include <openssl/ssl.h>
#include <openssl/err.h>

#ifndef USE_ARG
#define USE_ARG(x)	/*LINTED*/(void)&(x)
#endif

/* this structure encapsulates the ssl info */
typedef struct sslinfo_t {
	SSL_CTX			*ssl_context;
	const SSL_METHOD	*ssl_method;
	SSL			*bozossl;
	char			*certificate_file;
	char			*privatekey_file;
} sslinfo_t;

/*
 * bozo_clear_ssl_queue:  print the contents of the SSL error queue
 */
static void
bozo_clear_ssl_queue(bozohttpd_t *httpd)
{
	unsigned long sslcode = ERR_get_error();

	do {
		static const char sslfmt[] = "SSL Error: %s:%s:%s";

		if (httpd->logstderr || isatty(STDERR_FILENO)) {
			fprintf(stderr, sslfmt,
			    ERR_lib_error_string(sslcode),
			    ERR_func_error_string(sslcode),
			    ERR_reason_error_string(sslcode));
		} else {
			syslog(LOG_ERR, sslfmt,
			    ERR_lib_error_string(sslcode),
			    ERR_func_error_string(sslcode),
			    ERR_reason_error_string(sslcode));
		}
	} while (0 != (sslcode = ERR_get_error()));
}

/*
 * bozo_ssl_warn works just like bozo_warn, plus the SSL error queue
 */
BOZO_PRINTFLIKE(2, 3) static void
bozo_ssl_warn(bozohttpd_t *httpd, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	if (httpd->logstderr || isatty(STDERR_FILENO)) {
		vfprintf(stderr, fmt, ap);
		fputs("\n", stderr);
	} else
		vsyslog(LOG_ERR, fmt, ap);
	va_end(ap);

	bozo_clear_ssl_queue(httpd);
}


/*
 * bozo_ssl_err works just like bozo_err, plus the SSL error queue
 */
BOZO_PRINTFLIKE(3, 4) BOZO_DEAD static void
bozo_ssl_err(bozohttpd_t *httpd, int code, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	if (httpd->logstderr || isatty(STDERR_FILENO)) {
		vfprintf(stderr, fmt, ap);
		fputs("\n", stderr);
	} else
		vsyslog(LOG_ERR, fmt, ap);
	va_end(ap);

	bozo_clear_ssl_queue(httpd);
	exit(code);
}

/*
 * bozo_check_error_queue:  print warnings if the error isn't expected
 */
static void
bozo_check_error_queue(bozohttpd_t *httpd, const char *tag, int ret)
{
	if (ret > 0)
		return;

	const sslinfo_t *sslinfo = httpd->sslinfo;
	const int sslerr = SSL_get_error(sslinfo->bozossl, ret);

	if (sslerr != SSL_ERROR_ZERO_RETURN &&
	    sslerr != SSL_ERROR_SYSCALL &&
	    sslerr != SSL_ERROR_NONE)
		bozo_ssl_warn(httpd, "%s: SSL_ERROR %d", tag, sslerr);
}

static BOZO_PRINTFLIKE(2, 0) int
bozo_ssl_printf(bozohttpd_t *httpd, const char * fmt, va_list ap)
{
	char	*buf;
	int	 nbytes;

	if ((nbytes = vasprintf(&buf, fmt, ap)) != -1)  {
		const sslinfo_t *sslinfo = httpd->sslinfo;
		int ret = SSL_write(sslinfo->bozossl, buf, nbytes);
		bozo_check_error_queue(httpd, "write", ret);
	}

	free(buf);

	return nbytes;
}

static ssize_t
bozo_ssl_read(bozohttpd_t *httpd, int fd, void *buf, size_t nbytes)
{
	const sslinfo_t *sslinfo = httpd->sslinfo;
	int	ret;

	USE_ARG(fd);
	ret = SSL_read(sslinfo->bozossl, buf, (int)nbytes);
	bozo_check_error_queue(httpd, "read", ret);

	return (ssize_t)ret;
}

static ssize_t
bozo_ssl_write(bozohttpd_t *httpd, int fd, const void *buf, size_t nbytes)
{
	const sslinfo_t *sslinfo = httpd->sslinfo;
	int	ret;

	USE_ARG(fd);
	ret = SSL_write(sslinfo->bozossl, buf, (int)nbytes);
	bozo_check_error_queue(httpd, "write", ret);

	return (ssize_t)ret;
}

void
bozo_ssl_init(bozohttpd_t *httpd)
{
	sslinfo_t *sslinfo = httpd->sslinfo;

	if (sslinfo == NULL || !sslinfo->certificate_file)
		return;
	SSL_library_init();
	SSL_load_error_strings();

	sslinfo->ssl_method = SSLv23_server_method();
	sslinfo->ssl_context = SSL_CTX_new(sslinfo->ssl_method);

	if (NULL == sslinfo->ssl_context)
		bozo_ssl_err(httpd, EXIT_FAILURE,
		    "SSL context creation failed");

	if (1 != SSL_CTX_use_certificate_chain_file(sslinfo->ssl_context,
	    sslinfo->certificate_file))
		bozo_ssl_err(httpd, EXIT_FAILURE,
		    "Unable to use certificate file '%s'",
		    sslinfo->certificate_file);

	if (1 != SSL_CTX_use_PrivateKey_file(sslinfo->ssl_context,
	    sslinfo->privatekey_file, SSL_FILETYPE_PEM))
		bozo_ssl_err(httpd, EXIT_FAILURE,
		    "Unable to use private key file '%s'",
		    sslinfo->privatekey_file);

	/* check consistency of key vs certificate */
	if (!SSL_CTX_check_private_key(sslinfo->ssl_context))
		bozo_ssl_err(httpd, EXIT_FAILURE,
		    "Check private key failed");
}

/*
 * returns non-zero for failure
 */
int
bozo_ssl_accept(bozohttpd_t *httpd)
{
	sslinfo_t *sslinfo = httpd->sslinfo;

	if (sslinfo == NULL || !sslinfo->ssl_context)
		return 0;

	sslinfo->bozossl = SSL_new(sslinfo->ssl_context);
	if (sslinfo->bozossl == NULL)
		bozo_err(httpd, 1, "SSL_new failed");

	SSL_set_rfd(sslinfo->bozossl, 0);
	SSL_set_wfd(sslinfo->bozossl, 1);

	const int ret = SSL_accept(sslinfo->bozossl);
	bozo_check_error_queue(httpd, "accept", ret);

	return ret != 1;
}

void
bozo_ssl_destroy(bozohttpd_t *httpd)
{
	const sslinfo_t *sslinfo = httpd->sslinfo;

	if (sslinfo && sslinfo->bozossl)
		SSL_free(sslinfo->bozossl);
}

void
bozo_ssl_set_opts(bozohttpd_t *httpd, const char *cert, const char *priv)
{
	sslinfo_t *sslinfo = httpd->sslinfo;

	if (sslinfo == NULL) {
		sslinfo = bozomalloc(httpd, sizeof(*sslinfo));
		if (sslinfo == NULL)
			bozo_err(httpd, 1, "sslinfo allocation failed");
		httpd->sslinfo = sslinfo;
	}
	sslinfo->certificate_file = strdup(cert);
	sslinfo->privatekey_file = strdup(priv);
	debug((httpd, DEBUG_NORMAL, "using cert/priv files: %s & %s",
		sslinfo->certificate_file,
		sslinfo->privatekey_file));
	if (!httpd->bindport)
		httpd->bindport = strdup("https");
}

#endif /* NO_SSL_SUPPORT */

int
bozo_printf(bozohttpd_t *httpd, const char *fmt, ...)
{
	va_list	args;
	int	cc;

	va_start(args, fmt);
#ifndef NO_SSL_SUPPORT
	if (httpd->sslinfo)
		cc = bozo_ssl_printf(httpd, fmt, args);
	else
#endif
		cc = vprintf(fmt, args);
	va_end(args);
	return cc;
}

ssize_t
bozo_read(bozohttpd_t *httpd, int fd, void *buf, size_t len)
{
#ifndef NO_SSL_SUPPORT
	if (httpd->sslinfo)
		return bozo_ssl_read(httpd, fd, buf, len);
#endif
	return read(fd, buf, len);
}

ssize_t
bozo_write(bozohttpd_t *httpd, int fd, const void *buf, size_t len)
{
#ifndef NO_SSL_SUPPORT
	if (httpd->sslinfo)
		return bozo_ssl_write(httpd, fd, buf, len);
#endif
	return write(fd, buf, len);
}

int
bozo_flush(bozohttpd_t *httpd, FILE *fp)
{
#ifndef NO_SSL_SUPPORT
	if (httpd->sslinfo)
		return 0;
#endif
	return fflush(fp);
}
