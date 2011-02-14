/*	$NetBSD: compat_getpwent.c,v 1.3 2009/06/01 06:04:37 yamt Exp $	*/

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
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: compat_getpwent.c,v 1.3 2009/06/01 06:04:37 yamt Exp $");
#endif /* LIBC_SCCS and not lint */

#define __LIBC12_SOURCE__

#include "namespace.h"
#include <stdint.h>
#include <stdlib.h>
#include <pwd.h>
#include <compat/include/pwd.h>

__warn_references(getpwuid,
    "warning: reference to compatibility getpwuid(); include <pwd.h> to generate correct reference")
__warn_references(getpwnam,
    "warning: reference to compatibility getpwnam(); include <pwd.h> to generate correct reference")
__warn_references(getpwnam_r,
    "warning: reference to compatibility getpwnam_r(); include <pwd.h> to generate correct reference")
__warn_references(getpwuid_r,
    "warning: reference to compatibility getpwuid_r(); include <pwd.h> to generate correct reference")
__warn_references(getpwent,
    "warning: reference to compatibility getpwent(); include <pwd.h> to generate correct reference")
#ifdef notdef /* for libutil */
__warn_references(pw_scan,
    "warning: reference to compatibility pw_scan(); include <pwd.h> to generate correct reference")
#endif
__warn_references(getpwent_r,
    "warning: reference to compatibility getpwent_r(); include <pwd.h> to generate correct reference")
__warn_references(pwcache_userdb,
    "warning: reference to compatibility pwcache_userdb(); include <pwd.h> to generate correct reference")

#ifdef __weak_alias
__weak_alias(getpwent, _getpwent)
__weak_alias(getpwent_r, _getpwent_r)
__weak_alias(getpwuid, _getpwuid)
__weak_alias(getpwuid_r, _getpwuid_r)
__weak_alias(getpwnam, _getpwnam)
__weak_alias(getpwnam_r, _getpwnam_r)
__weak_alias(pwcache_userdb, _pwcache_userdb)
#endif

static struct passwd50 *
cvt(struct passwd *p)
{
	struct passwd50 *q = (void *)p;

	if (q == NULL) {
		return NULL;
	}
	q->pw_change = (int32_t)p->pw_change;
	q->pw_class = p->pw_class;
	q->pw_gecos = p->pw_gecos;
	q->pw_dir = p->pw_dir;
	q->pw_shell = p->pw_shell;
	q->pw_expire = (int32_t)p->pw_expire;
	return q;
}

struct passwd50	*
getpwuid(uid_t uid)
{
	return cvt(__getpwuid50(uid));

}

struct passwd50	*
getpwnam(const char *name)
{
	return cvt(__getpwnam50(name));
}

int
getpwnam_r(const char *name , struct passwd50 *p, char *buf, size_t len,
    struct passwd50 **q)
{
	struct passwd px, *qx;
	int rv = __getpwnam_r50(name, &px, buf, len, &qx);
	*q = p;
	passwd_to_passwd50(&px, p);
	return rv;
}

int
getpwuid_r(uid_t uid, struct passwd50 *p, char *buf, size_t len,
    struct passwd50 **q)
{
	struct passwd px, *qx;
	int rv = __getpwuid_r50(uid, &px, buf, len, &qx);
	*q = p;
	passwd_to_passwd50(&px, p);
	return rv;
}

struct passwd50	*
getpwent(void)
{
	return cvt(__getpwent50());
}

#ifdef notdef /* for libutil */
int
pw_scan(char *buf, struct passwd50 *p, int *flags)
{
	struct passwd px;
	int rv = __pw_scan50(buf, &px, flags);
	passwd_to_passwd50(&px, p);
	return rv;
}
#endif

int
getpwent_r(struct passwd50 *p, char *buf, size_t len, struct passwd50 **q)
{
	struct passwd px, *qx;
	int rv = __getpwent_r50(&px, buf, len, &qx);
	*q = p;
	passwd_to_passwd50(&px, p);
	return rv;
}

static struct passwd50 * (*__getpwnamf)(const char *);
static struct passwd50 * (*__getpwuidf)(uid_t);
static struct passwd pw;

static struct passwd *
internal_getpwnam(const char *name)
{
	struct passwd50 *p = (*__getpwnamf)(name);
	passwd50_to_passwd(p, &pw);
	return &pw;
}

static struct passwd *
internal_getpwuid(uid_t uid)
{
	struct passwd50 *p = (*__getpwuidf)(uid);
	passwd50_to_passwd(p, &pw);
	return &pw;
}

int pwcache_userdb(int (*setpassentf)(int), void (*endpwentf)(void),
    struct passwd50 * (*getpwnamf)(const char *),
    struct passwd50 * (*getpwuidf)(uid_t))
{
	__getpwnamf = getpwnamf;
	__getpwuidf = getpwuidf;
	return __pwcache_userdb50(setpassentf, endpwentf, internal_getpwnam,
	    internal_getpwuid);
}
