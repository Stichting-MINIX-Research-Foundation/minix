/*	$NetBSD: utmp.c,v 1.1 2011/09/17 01:50:08 christos Exp $	*/

/*-
 * Copyright (c) 2011 The NetBSD Foundation, Inc.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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
#include <sys/cdefs.h>
__RCSID("$NetBSD: utmp.c,v 1.1 2011/09/17 01:50:08 christos Exp $");

#include <sys/types.h>
#include <sys/param.h>
#include <syslog.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <util.h>
#include <paths.h>

#ifdef SUPPORT_UTMP
#include <utmp.h>
#endif
#ifdef SUPPORT_UTMPX
#include <utmpx.h>
#endif

#include "tmux.h"

struct window_utmp {
#ifdef SUPPORT_UTMP
	struct utmp ut;
#endif
#ifdef SUPPORT_UTMPX
	struct utmpx utx;
#endif
};

#ifdef SUPPORT_UTMPX
static void
login_utmpx(struct utmpx *utmpx, const char *username, const char *hostname,
	const char *tty, const struct timeval *now)
{
	const char *t;

	(void)memset(utmpx, 0, sizeof(*utmpx));
	utmpx->ut_tv = *now;
	(void)strncpy(utmpx->ut_name, username, sizeof(utmpx->ut_name));
	if (hostname)
		(void)strncpy(utmpx->ut_host, hostname, sizeof(utmpx->ut_host));
	(void)strncpy(utmpx->ut_line, tty, sizeof(utmpx->ut_line));
	utmpx->ut_type = USER_PROCESS;
	utmpx->ut_pid = getpid();
	t = tty + strlen(tty);
	if ((size_t)(t - tty) >= sizeof(utmpx->ut_id)) {
	    (void)strncpy(utmpx->ut_id, t - sizeof(utmpx->ut_id),
		sizeof(utmpx->ut_id));
	} else {
	    (void)strncpy(utmpx->ut_id, tty, sizeof(utmpx->ut_id));
	}
	(void)pututxline(utmpx);
	endutxent();
}

static void
logout_utmpx(struct utmpx *utmpx, const struct timeval *now)
{
	utmpx->ut_type = DEAD_PROCESS;
	utmpx->ut_tv = *now;
	utmpx->ut_pid = 0;
	(void)pututxline(utmpx);
	endutxent();
}
#endif

#ifdef SUPPORT_UTMP
static void
login_utmp(struct utmp *utmp, const char *username, const char *hostname,
    const char *tty, const struct timeval *now)
{
	(void)memset(utmp, 0, sizeof(*utmp));
	utmp->ut_time = now->tv_sec;
	(void)strncpy(utmp->ut_name, username, sizeof(utmp->ut_name));
	if (hostname)
		(void)strncpy(utmp->ut_host, hostname, sizeof(utmp->ut_host));
	(void)strncpy(utmp->ut_line, tty, sizeof(utmp->ut_line));
	login(utmp);
}

static void
logout_utmp(struct utmp *utmp, const struct timeval *now)
{
	logout(utmp->ut_line);
}
#endif

struct window_utmp *
utmp_create(const char *tty)
{
	struct window_utmp *wu;
	struct timeval tv;
	char username[LOGIN_NAME_MAX];

	if (getlogin_r(username, sizeof(username)) == -1)
		return NULL;

	if ((wu = malloc(sizeof(*wu))) == NULL)
		return NULL;

	tty += sizeof(_PATH_DEV) - 1;

	(void)gettimeofday(&tv, NULL);
#ifdef SUPPORT_UTMPX
	login_utmpx(&wu->utx, username, "tmux", tty, &tv);
#endif
#ifdef SUPPORT_UTMP
	login_utmp(&wu->ut, username, "tmux", tty, &tv);
#endif
	return wu;
}

void
utmp_destroy(struct window_utmp *wu)
{
	struct timeval tv;

	if (wu == NULL)
		return;

	(void)gettimeofday(&tv, NULL);
#ifdef SUPPORT_UTMPX
	logout_utmpx(&wu->utx, &tv);
#endif
#ifdef SUPPORT_UTMP
	logout_utmp(&wu->ut, &tv);
#endif
}
