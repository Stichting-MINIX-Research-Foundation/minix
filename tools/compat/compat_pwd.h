/*	$NetBSD: compat_pwd.h,v 1.6 2009/01/18 01:44:09 christos Exp $	*/

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Todd Vierling.
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

#ifndef _COMPAT_PWD_H_
#define	_COMPAT_PWD_H_

/* A very special version of <pwd.h> for pwd_mkdb(8) and __nbcompat_pwscan(3). */

#include "../../include/pwd.h"

#define passwd __nbcompat_passwd
#define pw_scan __nbcompat_pw_scan
#ifdef LOGIN_NAME_MAX
#undef LOGIN_NAME_MAX
#endif
/* Taken from syslimits.h. Need the NetBSD def, not the local system def */
#define LOGIN_NAME_MAX	17

/* All elements exactly sized: */
struct passwd {
	char	*pw_name;
	char	*pw_passwd;
	int32_t	pw_uid;
	int32_t	pw_gid;
	int64_t	pw_change;
	char	*pw_class;
	char	*pw_gecos;
	char	*pw_dir;
	char	*pw_shell;
	int64_t	pw_expire;
};

int pw_scan(char *, struct passwd *, int *);

#endif /* !_PWD_H_ */
