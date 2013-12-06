/*	$NetBSD: extern.h,v 1.23 2013/08/19 13:03:12 joerg Exp $	*/

/*
 * Copyright (c) 1997 Christos Zoulas.  All rights reserved.
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
 */

#include <stdarg.h>
#include <ucontext.h>

#ifndef __LOCALE_T_DECLARED
typedef struct _locale		*locale_t;
#define __LOCALE_T_DECLARED
#endif /* __LOCALE_T_DECLARED */

__BEGIN_DECLS
extern char *__minbrk;
int __getcwd(char *, size_t);
int __getlogin(char *, size_t);
int __setlogin(const char *);
void _resumecontext(void) __dead;
__dso_hidden int	_strerror_lr(int, char *, size_t, locale_t);
const char *__strerror(int , char *, size_t);
const char *__strsignal(int , char *, size_t);
char *__dtoa(double, int, int, int *, int *, char **);
void __freedtoa(char *);
int __sysctl(const int *, unsigned int, void *, size_t *, const void *, size_t);

struct sigaction;
int __sigaction_sigtramp(int, const struct sigaction *,
    struct sigaction *, const void *, int);

#ifdef WIDE_DOUBLE
char *__hdtoa(double, const char *, int, int *, int *, char **);
char *__hldtoa(long double, const char *, int, int *, int *,  char **);
char *__ldtoa(long double *, int, int, int *, int *, char **);
#endif

#ifndef __LIBC12_SOURCE__
struct syslog_data;
void	syslog_ss(int, struct syslog_data *, const char *, ...)
    __RENAME(__syslog_ss60) __printflike(3, 4);
void    vsyslog_ss(int, struct syslog_data *, const char *, va_list) 
    __RENAME(__vsyslog_ss60) __printflike(3, 0); 
void	syslogp_ss(int, struct syslog_data *, const char *, const char *, 
    const char *, ...) __RENAME(__syslogp_ss60) __printflike(5, 0);
void	vsyslogp_ss(int, struct syslog_data *, const char *, const char *, 
    const char *, va_list) __RENAME(__vsyslogp_ss60) __printflike(5, 0);
#endif

void	_malloc_prefork(void);
void	_malloc_postfork(void);

int	_sys_setcontext(const ucontext_t *);

__END_DECLS
