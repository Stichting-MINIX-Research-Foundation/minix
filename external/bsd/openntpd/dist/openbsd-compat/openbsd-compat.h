/* $Id: openbsd-compat.h,v 1.19 2006/01/19 13:11:27 dtucker Exp $ */

/*
 * Copyright (c) 2004 Darren Tucker.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <pwd.h>
#include <netdb.h>
#include <unistd.h>

#include "atomicio.h"
#include "bsd-poll.h"
#include "bsd-getifaddrs.h"
#include "fake-rfc2553.h"

char *_compat_get_progname(char *);

#ifndef HAVE_ARC4RANDOM
void seed_rng(void);
unsigned int arc4random(void);
void arc4random_stir(void);
#endif /* !HAVE_ARC4RANDOM */

#ifndef HAVE_STRLCPY
size_t strlcpy(char *dst, const char *src, size_t siz);
#endif

#ifndef HAVE_DAEMON
int daemon(int nochdir, int noclose);
#endif

#ifndef HAVE_ASPRINTF
int      asprintf(char **, const char *, ...)
                __attribute__((__format__ (printf, 2, 3)));
#endif

#ifndef HAVE_INET_PTON
int inet_pton(int, const char *, void *);
#endif

#if !defined(HAVE_SETEUID) && defined(HAVE_SETREUID)
int seteuid(uid_t);
#endif /* !defined(HAVE_SETEUID) && defined(HAVE_SETREUID) */

#if !defined(HAVE_SETEGID) && defined(HAVE_SETRESGID)
int setegid(uid_t);
#endif /* !defined(HAVE_SETEGID) && defined(HAVE_SETRESGID) */

#ifndef HAVE_VSYSLOG
void vsyslog(int, const char *, va_list);
#endif

#ifndef HAVE_SNPRINTF
int snprintf(char *, size_t, const char *, ...);
#endif

#ifndef HAVE_VSNPRINTF
int vsnprintf(char *, size_t, const char *, va_list);
#endif

#ifndef HAVE_CLOCK_GETRES
# ifndef CLOCK_REALTIME
#  define CLOCK_REALTIME	1
# endif
int clock_getres(int, struct timespec *);
#endif

#ifndef HAVE_ERRX
__dead void     errx(int, const char *, ...)
                        __attribute__((__format__ (printf, 2, 3)));
#endif
#ifndef HAVE_VERRX
__dead void     verrx(int, const char *, va_list)
                        __attribute__((__format__ (printf, 2, 0)));
#endif

#if !defined(HAVE_SETRESUID)
int setresuid(uid_t, uid_t, uid_t);
#endif

#if !defined(HAVE_SETRESGID)
int setresgid(gid_t, gid_t, gid_t);
#endif

#ifndef HAVE_STRSIGNAL
char *strsignal(int);
#endif

int permanently_set_uid(struct passwd *);
