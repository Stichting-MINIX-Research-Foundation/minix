/*
 * Copyright (c) 1999-2004 Damien Miller <djm@mindrot.org>
 * Copyright (c) 2004 Darren Tucker <dtucker at zip com au>
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

#include <errno.h>

#include "includes.h"

#ifdef HAVE_SYS_TIMEX_H
/*
 * We can't put this in includes.h because of conflicting definitions of
 * ntp_adjtime.
 */
# include <sys/timex.h>
#endif

#ifndef HAVE___PROGNAME
char *__progname;
#endif

static char *
xstrdup(const char *s)
{
	char *c = strdup(s);

	if (c == NULL) {
		fprintf(stderr, "%s failed: %s", __func__, strerror(errno));
		exit(1);
	}
	return c;
}

/*
 * NB. duplicate __progname in case it is an alias for argv[0]
 * Otherwise it may get clobbered by setproctitle()
 */
char *
_compat_get_progname(char *argv0)
{
#ifdef HAVE___PROGNAME
	extern char *__progname;

	return xstrdup(__progname);
#else
	char *p;

	if (argv0 == NULL)
		return ("unknown");	/* XXX */
	p = strrchr(argv0, '/');
	if (p == NULL)
		p = argv0;
	else
		p++;

	return (xstrdup(p));
#endif
}

#if !defined(HAVE_SETEUID) && defined(HAVE_SETREUID)
int seteuid(uid_t euid)
{
	return (setreuid(-1, euid));
}
#endif /* !defined(HAVE_SETEUID) && defined(HAVE_SETREUID) */

#if !defined(HAVE_SETEGID) && defined(HAVE_SETRESGID)
int setegid(uid_t egid)
{
	return(setresgid(-1, egid, -1));
}
#endif /* !defined(HAVE_SETEGID) && defined(HAVE_SETRESGID) */

#ifndef HAVE_VSYSLOG
void
vsyslog(int priority, const char *message, va_list args)
{
	char buf[2048];

	vsnprintf(buf, sizeof(buf), message, args); /* possible truncation */
	syslog(priority, "%s", buf);
}
#endif /* HAVE_VSYSLOG */

#ifndef HAVE_CLOCK_GETRES
int
clock_getres(int clock_id, struct timespec *tp)
{
# ifdef HAVE_ADJTIMEX
	struct timex tmx;
# endif

	if (clock_id != CLOCK_REALTIME)
		return -1;	/* not implemented */
	tp->tv_sec = 0;

# ifdef HAVE_ADJTIMEX
	tmx.modes = 0;
	if (adjtimex(&tmx) == -1)
		return -1;
	else
		tp->tv_nsec = tmx.precision * 1000;	/* usec -> nsec */
# else
	/* assume default 10ms tick */
	tp->tv_nsec = 10000000;
# endif
	return 0;
}
#endif /* HAVE_CLOCK_GETRES */

#ifndef HAVE_SETGROUPS
int
setgroups(size_t size, const gid_t *list)
{
	return 0;
}
#endif /* HAVE_SETGROUPS */

#ifndef HAVE_STRSIGNAL
char *
strsignal(int sig)
{
	return NULL;
}
#endif
