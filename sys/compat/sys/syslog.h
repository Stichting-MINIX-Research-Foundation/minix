/*	$NetBSD: syslog.h,v 1.1 2012/10/10 22:51:12 christos Exp $	*/

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
#ifndef _COMPAT_SYS_SYSLOG_H_
#define _COMPAT_SYS_SYSLOG_H_

struct syslog_data60 {
	int	log_file;
	int	connected;
	int	opened;
	int	log_stat;
	const char 	*log_tag;
	int 	log_fac;
	int 	log_mask;
};

__BEGIN_DECLS
#ifdef __LIBC12_SOURCE__
void	closelog_r(struct syslog_data60 *);
void	openlog_r(const char *, int, int, struct syslog_data60 *);
int	setlogmask_r(int, struct syslog_data60 *);
void	syslog_r(int, struct syslog_data60 *, const char *, ...)
    __printflike(3, 4);
void	vsyslog_r(int, struct syslog_data60 *, const char *, __va_list)
    __printflike(3, 0);
void	syslogp_r(int, struct syslog_data60 *, const char *, const char *,
    const char *, ...) __printflike(5, 6);
void	vsyslogp_r(int, struct syslog_data60 *, const char *, const char *,
    const char *, __va_list) __printflike(5, 0);

struct syslog_data;
void	__closelog_r60(struct syslog_data *);
void	__openlog_r60(const char *, int, int, struct syslog_data *);
int	__setlogmask_r60(int, struct syslog_data *);
void	__syslog_r60(int, struct syslog_data *, const char *, ...)
    __printflike(3, 4);
void	__vsyslog_r60(int, struct syslog_data *, const char *, __va_list)
    __printflike(3, 0);
void 	__syslogp_r60(int, struct syslog_data *, const char *, const char *,
    const char *, ...) __printflike(5, 6);
void	__vsyslogp_r60(int, struct syslog_data *, const char *, const char *,
    const char *, __va_list) __printflike(5, 0);
#endif
__END_DECLS

#endif /* !_COMPAT_SYS_SYSLOG_H_ */
