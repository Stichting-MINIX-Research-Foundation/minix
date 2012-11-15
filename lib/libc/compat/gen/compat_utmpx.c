/*	$NetBSD: compat_utmpx.c,v 1.4 2011/07/01 01:08:59 joerg Exp $	 */

/*-
 * Copyright (c) 2008 The NetBSD Foundation, Inc.
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
#include <sys/cdefs.h>

#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: compat_utmpx.c,v 1.4 2011/07/01 01:08:59 joerg Exp $");
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#include <sys/types.h>
#include <sys/param.h>

#define __LIBC12_SOURCE__
#include <assert.h>
#include <sys/time.h>
#include <string.h>
#include <compat/sys/time.h>
#include <utmpx.h>
#include <compat/include/utmpx.h>

__warn_references(getutxent,
    "warning: reference to compatibility getutxent(); include <utmpx.h> for correct reference")
__warn_references(getutxid,
    "warning: reference to compatibility getutxid(); include <utmpx.h> for correct reference")
__warn_references(getutxline,
    "warning: reference to compatibility getutxline(); include <utmpx.h> for correct reference")
__warn_references(pututxline,
    "warning: reference to compatibility pututxline(); include <utmpx.h> for correct reference")
__warn_references(updwtmpx,
    "warning: reference to compatibility updwtmpx(); include <utmpx.h> for correct reference")
__warn_references(getlastlogx,
    "warning: reference to compatibility getlastlogx(); include <utmpx.h> for correct reference")
__warn_references(updlastlogx,
    "warning: reference to compatibility updlastlogx(); include <utmpx.h> for correct reference")
__warn_references(getutmp,
    "warning: reference to compatibility getutmp(); include <utmpx.h> for correct reference")
__warn_references(getutmpx,
    "warning: reference to compatibility getutmpx(); include <utmpx.h> for correct reference")

static struct utmpx50 *
cvt(struct utmpx *ut)
{
	if (ut == NULL)
		return NULL;
	timeval_to_timeval50(&ut->ut_tv, (void *)&ut->ut_tv);
	return (void *)ut;
}

static void
lastlogx50_to_lastlogx(const struct lastlogx50 *ll50, struct lastlogx *ll)
{
	(void)memcpy(ll->ll_line, ll50->ll_line, sizeof(ll->ll_line));
	(void)memcpy(ll->ll_host, ll50->ll_host, sizeof(ll->ll_host));
	(void)memcpy(&ll->ll_ss, &ll50->ll_ss, sizeof(ll->ll_ss));
	timeval50_to_timeval(&ll50->ll_tv, &ll->ll_tv);
}

static void
lastlogx_to_lastlogx50(const struct lastlogx *ll, struct lastlogx50 *ll50)
{
	(void)memcpy(ll50->ll_line, ll->ll_line, sizeof(ll50->ll_line));
	(void)memcpy(ll50->ll_host, ll->ll_host, sizeof(ll50->ll_host));
	(void)memcpy(&ll50->ll_ss, &ll->ll_ss, sizeof(ll50->ll_ss));
	timeval_to_timeval50(&ll->ll_tv, &ll50->ll_tv);
}

struct utmpx50 *
getutxent(void)
{
	return cvt(__getutxent50());
}

struct utmpx50 *
getutxid(const struct utmpx50 *ut50)
{
	struct utmpx ut;
	utmpx50_to_utmpx(ut50, &ut);
	return cvt(__getutxid50(&ut));
}

struct utmpx50 *
getutxline(const struct utmpx50 *ut50)
{
	struct utmpx ut;
	utmpx50_to_utmpx(ut50, &ut);
	return cvt(__getutxline50(&ut));
}

struct utmpx50 *
pututxline(const struct utmpx50 *ut50)
{
	struct utmpx ut;
	utmpx50_to_utmpx(ut50, &ut);
	return cvt(__pututxline50(&ut));
}

int
updwtmpx(const char *fname, const struct utmpx50 *ut50)
{
	struct utmpx ut;
	utmpx50_to_utmpx(ut50, &ut);
	return __updwtmpx50(fname, &ut);
}

struct lastlogx50 *
__getlastlogx13(const char *fname, uid_t uid, struct lastlogx50 *ll50)
{
	struct lastlogx ll;
	if (__getlastlogx50(fname, uid, &ll) == NULL)
		return NULL;
	lastlogx_to_lastlogx50(&ll, ll50);
	return ll50;
}

static char llfile[MAXPATHLEN] = _PATH_LASTLOGX;

int
lastlogxname(const char *fname)
{
	size_t len;

	_DIAGASSERT(fname != NULL);

	len = strlen(fname);

	if (len >= sizeof(llfile))
		return 0;

	/* must end in x! */
	if (fname[len - 1] != 'x')
		return 0;

	(void)strlcpy(llfile, fname, sizeof(llfile));
	return 1;
}

struct lastlogx50 *
getlastlogx(uid_t uid, struct lastlogx50 *ll)
{

	return __getlastlogx13(llfile, uid, ll);
}

int
updlastlogx(const char *fname, uid_t uid, struct lastlogx50 *ll50)
{
	struct lastlogx ll;
	lastlogx50_to_lastlogx(ll50, &ll);
	return __updlastlogx50(fname, uid, &ll);
}

void
getutmp(const struct utmpx50 *utx50, struct utmp *ut)
{
	struct utmpx utx;
	utmpx50_to_utmpx(utx50, &utx);
	__getutmp50(&utx, ut);
}

void
getutmpx(const struct utmp *ut, struct utmpx50 *utx50)
{
	struct utmpx utx;
	__getutmpx50(ut, &utx);
	utmpx_to_utmpx50(&utx, utx50);
}
