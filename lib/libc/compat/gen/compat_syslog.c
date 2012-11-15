/*	$NetBSD: compat_syslog.c,v 1.2 2012/10/11 17:09:55 christos Exp $	*/

/*-
 * Copyright (c) 2012 The NetBSD Foundation, Inc.
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

#include "namespace.h"
#include <sys/cdefs.h>

#define	__LIBC12_SOURCE__
#include <stdarg.h>

#include <sys/types.h>
#include <sys/syslog.h>
#include <compat/sys/syslog.h>

void	syslog_ss(int, struct syslog_data60 *, const char *, ...)
    __printflike(3, 4);
void    vsyslog_ss(int, struct syslog_data60 *, const char *, va_list) 
    __printflike(3, 0); 
void	syslogp_ss(int, struct syslog_data60 *, const char *, const char *, 
    const char *, ...) __printflike(5, 0);
void	vsyslogp_ss(int, struct syslog_data60 *, const char *, const char *, 
    const char *, va_list) __printflike(5, 0);

#ifdef __weak_alias
__weak_alias(closelog_r,_closelog_r)
__weak_alias(openlog_r,_openlog_r)
__weak_alias(setlogmask_r,_setlogmask_r)
__weak_alias(syslog_r,_syslog_r)
__weak_alias(vsyslog_r,_vsyslog_r)
__weak_alias(syslogp_r,_syslogp_r)
__weak_alias(vsyslogp_r,_vsyslogp_r)

__weak_alias(syslog_ss,_syslog_ss)
__weak_alias(vsyslog_ss,_vsyslog_ss)
__weak_alias(syslogp_ss,_syslogp_ss)
__weak_alias(vsyslogp_ss,_vsyslogp_ss)
#endif /* __weak_alias */

__warn_references(closelog_r,
    "warning: reference to compatibility closelog_r();"
    " include <sys/syslog.h> for correct reference")
__warn_references(openlog_r,
    "warning: reference to compatibility openlog_r();"
    " include <sys/syslog.h> for correct reference")
__warn_references(setlogmask_r,
    "warning: reference to compatibility setlogmask_r();"
    " include <sys/syslog.h> for correct reference")
__warn_references(syslog_r,
    "warning: reference to compatibility syslog_r();"
    " include <sys/syslog.h> for correct reference")
__warn_references(vsyslog_r,
    "warning: reference to compatibility vsyslog_r();"
    " include <sys/syslog.h> for correct reference")
__warn_references(syslogp_r,
    "warning: reference to compatibility syslogp_r();"
    " include <sys/syslog.h> for correct reference")
__warn_references(vsyslogp_r,
    "warning: reference to compatibility vsyslogp_r();"
    " include <sys/syslog.h> for correct reference")

static void
syslog_data_convert(struct syslog_data *d, const struct syslog_data60 *s)
{
	d->log_file = s->log_file;
	d->log_connected = s->connected;
	d->log_opened = s->opened;
	d->log_stat = s->log_stat;
	d->log_tag = s->log_tag;
	d->log_fac = s->log_fac;
	d->log_mask = s->log_mask;
}

void
closelog_r(struct syslog_data60 *data60)
{
	struct syslog_data data = SYSLOG_DATA_INIT;
	syslog_data_convert(&data, data60);
	__closelog_r60(&data);
}

void
openlog_r(const char *ident, int logstat, int logfac,
    struct syslog_data60 *data60)
{
	struct syslog_data data = SYSLOG_DATA_INIT;
	syslog_data_convert(&data, data60);
	__openlog_r60(ident, logstat, logfac, &data);
}

int
setlogmask_r(int pmask, struct syslog_data60 *data60)
{
	struct syslog_data data = SYSLOG_DATA_INIT;
	syslog_data_convert(&data, data60);
	return __setlogmask_r60(pmask, &data);
}

void
syslog_r(int pri, struct syslog_data60 *data60, const char *fmt, ...)
{
	va_list ap;
	struct syslog_data data = SYSLOG_DATA_INIT;
	syslog_data_convert(&data, data60);

	va_start(ap, fmt);
	__vsyslog_r60(pri, &data, fmt, ap);
	va_end(ap);
}

void
vsyslog_r(int pri, struct syslog_data60 *data60, const char *fmt, __va_list ap)
{
	struct syslog_data data = SYSLOG_DATA_INIT;
	syslog_data_convert(&data, data60);
	__vsyslog_r60(pri, &data, fmt, ap);
}

void
syslogp_r(int pri, struct syslog_data60 *data60, const char *msgid,
    const char *sdfmt, const char *msgfmt, ...)
{
	va_list ap;
	struct syslog_data data = SYSLOG_DATA_INIT;
	syslog_data_convert(&data, data60);

	va_start(ap, msgfmt);
	__vsyslogp_r60(pri, &data, msgid, sdfmt, msgfmt, ap);
	va_end(ap);
}

void
vsyslogp_r(int pri, struct syslog_data60 *data60, const char *msgid,
    const char *sdfmt, const char *msgfmt, __va_list ap)
{
	struct syslog_data data = SYSLOG_DATA_INIT;
	syslog_data_convert(&data, data60);

	__vsyslogp_r60(pri, &data, msgid, sdfmt, msgfmt, ap);
}

/*
 * These are semi-private
 */
#define LOG_SIGNAL_SAFE (int)0x80000000

void
syslog_ss(int pri, struct syslog_data60 *data, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vsyslog_r(pri | LOG_SIGNAL_SAFE, data, fmt, ap);
	va_end(ap);
}

void
syslogp_ss(int pri, struct syslog_data60 *data, const char *msgid,
    const char *sdfmt, const char *msgfmt, ...)
{
	va_list ap;

	va_start(ap, msgfmt);
	vsyslogp_r(pri | LOG_SIGNAL_SAFE, data, msgid, sdfmt, msgfmt, ap);
	va_end(ap);
}

void
vsyslog_ss(int pri, struct syslog_data60 *data, const char *fmt, va_list ap)
{
	vsyslog_r(pri | LOG_SIGNAL_SAFE, data, fmt, ap);
}

void
vsyslogp_ss(int pri, struct syslog_data60 *data, const char *msgid,
    const char *sdfmt, const char *msgfmt, va_list ap)
{
	vsyslogp_r(pri | LOG_SIGNAL_SAFE, data, msgid, sdfmt, msgfmt, ap);
}
