#include <sys/cdefs.h>
 __RCSID("$NetBSD: common.c,v 1.14 2015/07/09 10:15:34 roy Exp $");

/*
 * dhcpcd - DHCP client daemon
 * Copyright (c) 2006-2015 Roy Marples <roy@marples.name>
 * All rights reserved

 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/time.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#ifdef BSD
#  include <paths.h>
#endif
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include "common.h"
#include "dhcpcd.h"
#include "if-options.h"

#ifndef _PATH_DEVNULL
#  define _PATH_DEVNULL "/dev/null"
#endif

const char *
get_hostname(char *buf, size_t buflen, int short_hostname)
{
	char *p;

	if (gethostname(buf, buflen) != 0)
		return NULL;
	buf[buflen - 1] = '\0';
	if (strcmp(buf, "(none)") == 0 ||
	    strcmp(buf, "localhost") == 0 ||
	    strncmp(buf, "localhost.", strlen("localhost.")) == 0 ||
	    buf[0] == '.')
		return NULL;

	if (short_hostname) {
		p = strchr(buf, '.');
		if (p)
			*p = '\0';
	}

	return buf;
}

#if USE_LOGFILE
void
logger_open(struct dhcpcd_ctx *ctx)
{

	if (ctx->logfile) {
		int f = O_CREAT | O_APPEND | O_TRUNC;

#ifdef O_CLOEXEC
		f |= O_CLOEXEC;
#endif
		ctx->log_fd = open(ctx->logfile, O_WRONLY | f, 0644);
		if (ctx->log_fd == -1)
			warn("open: %s", ctx->logfile);
#ifndef O_CLOEXEC
		else {
			if (fcntl(ctx->log_fd, F_GETFD, &f) == -1 ||
			    fcntl(ctx->log_fd, F_SETFD, f | FD_CLOEXEC) == -1)
				warn("fcntl: %s", ctx->logfile);
		}
#endif
	} else
		openlog(PACKAGE, LOG_PID, LOG_DAEMON);
}

void
logger_close(struct dhcpcd_ctx *ctx)
{

	if (ctx->log_fd != -1) {
		close(ctx->log_fd);
		ctx->log_fd = -1;
	}
	closelog();
}

void
logger(struct dhcpcd_ctx *ctx, int pri, const char *fmt, ...)
{
	va_list va;
	int serrno;
#ifndef HAVE_PRINTF_M
	char fmt_cpy[1024];
#endif

	if (pri >= LOG_DEBUG && ctx && !(ctx->options & DHCPCD_DEBUG))
		return;

	serrno = errno;
	va_start(va, fmt);

#ifndef HAVE_PRINTF_M
	/* Print strerrno(errno) in place of %m */
	if (ctx == NULL || !(ctx->options & DHCPCD_QUIET) || ctx->log_fd != -1)
	{
		const char *p;
		char *fp = fmt_cpy, *serr = NULL;
		size_t fmt_left = sizeof(fmt_cpy) - 1, fmt_wrote;

		for (p = fmt; *p != '\0'; p++) {
			if (p[0] == '%' && p[1] == '%') {
				if (fmt_left < 2)
					break;
				*fp++ = '%';
				*fp++ = '%';
				fmt_left -= 2;
				p++;
			} else if (p[0] == '%' && p[1] == 'm') {
				if (serr == NULL)
					serr = strerror(serrno);
				fmt_wrote = strlcpy(fp, serr, fmt_left);
				if (fmt_wrote > fmt_left)
					break;
				fp += fmt_wrote;
				fmt_left -= fmt_wrote;
				p++;
			} else {
				*fp++ = *p;
				--fmt_left;
			}
			if (fmt_left == 0)
				break;
		}
		*fp++ = '\0';
		fmt = fmt_cpy;
	}

#endif

	if (ctx == NULL || !(ctx->options & DHCPCD_QUIET)) {
		va_list vac;

		va_copy(vac, va);
		vfprintf(pri <= LOG_ERR ? stderr : stdout, fmt, vac);
		fputc('\n', pri <= LOG_ERR ? stderr : stdout);
		va_end(vac);
	}

#ifdef HAVE_PRINTF_M
	errno = serrno;
#endif
	if (ctx && ctx->log_fd != -1) {
		struct timeval tv;
		char buf[32];

		/* Write the time, syslog style. month day time - */
		if (gettimeofday(&tv, NULL) != -1) {
			time_t now;
			struct tm tmnow;

			tzset();
			now = tv.tv_sec;
			localtime_r(&now, &tmnow);
			strftime(buf, sizeof(buf), "%b %d %T ", &tmnow);
			dprintf(ctx->log_fd, "%s", buf);
		}

		vdprintf(ctx->log_fd, fmt, va);
		dprintf(ctx->log_fd, "\n");
	} else
		vsyslog(pri, fmt, va);
	va_end(va);
}
#endif

ssize_t
setvar(struct dhcpcd_ctx *ctx,
    char **e, const char *prefix, const char *var, const char *value)
{
	size_t len = strlen(var) + strlen(value) + 3;

	if (prefix)
		len += strlen(prefix) + 1;
	*e = malloc(len);
	if (*e == NULL) {
		logger(ctx, LOG_ERR, "%s: %m", __func__);
		return -1;
	}
	if (prefix)
		snprintf(*e, len, "%s_%s=%s", prefix, var, value);
	else
		snprintf(*e, len, "%s=%s", var, value);
	return (ssize_t)len;
}

ssize_t
setvard(struct dhcpcd_ctx *ctx,
    char **e, const char *prefix, const char *var, size_t value)
{

	char buffer[32];

	snprintf(buffer, sizeof(buffer), "%zu", value);
	return setvar(ctx, e, prefix, var, buffer);
}

ssize_t
addvar(struct dhcpcd_ctx *ctx,
    char ***e, const char *prefix, const char *var, const char *value)
{
	ssize_t len;

	len = setvar(ctx, *e, prefix, var, value);
	if (len != -1)
		(*e)++;
	return (ssize_t)len;
}

ssize_t
addvard(struct dhcpcd_ctx *ctx,
    char ***e, const char *prefix, const char *var, size_t value)
{
	char buffer[32];

	snprintf(buffer, sizeof(buffer), "%zu", value);
	return addvar(ctx, e, prefix, var, buffer);
}

char *
hwaddr_ntoa(const unsigned char *hwaddr, size_t hwlen, char *buf, size_t buflen)
{
	char *p;
	size_t i;

	if (buf == NULL) {
		return NULL;
	}

	if (hwlen * 3 > buflen) {
		errno = ENOBUFS;
		return 0;
	}

	p = buf;
	for (i = 0; i < hwlen; i++) {
		if (i > 0)
			*p ++= ':';
		p += snprintf(p, 3, "%.2x", hwaddr[i]);
	}
	*p ++= '\0';
	return buf;
}

size_t
hwaddr_aton(unsigned char *buffer, const char *addr)
{
	char c[3];
	const char *p = addr;
	unsigned char *bp = buffer;
	size_t len = 0;

	c[2] = '\0';
	while (*p) {
		c[0] = *p++;
		c[1] = *p++;
		/* Ensure that digits are hex */
		if (isxdigit((unsigned char)c[0]) == 0 ||
		    isxdigit((unsigned char)c[1]) == 0)
		{
			errno = EINVAL;
			return 0;
		}
		/* We should have at least two entries 00:01 */
		if (len == 0 && *p == '\0') {
			errno = EINVAL;
			return 0;
		}
		/* Ensure that next data is EOL or a seperator with data */
		if (!(*p == '\0' || (*p == ':' && *(p + 1) != '\0'))) {
			errno = EINVAL;
			return 0;
		}
		if (*p)
			p++;
		if (bp)
			*bp++ = (unsigned char)strtol(c, NULL, 16);
		len++;
	}
	return len;
}
