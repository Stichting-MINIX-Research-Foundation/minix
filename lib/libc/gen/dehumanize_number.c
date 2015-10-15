/*	$NetBSD: dehumanize_number.c,v 1.7 2014/10/01 13:53:04 apb Exp $	*/

/*
 * Copyright (c) 2005, 2006 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Julio M. Merino Vidal, developed as part of Google's Summer of Code
 * 2005 program.
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

#ifdef HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif /* HAVE_NBTOOL_CONFIG_H */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: dehumanize_number.c,v 1.7 2014/10/01 13:53:04 apb Exp $");
#endif /* LIBC_SCCS and not lint */

#include "namespace.h"
#include <assert.h>
#include <inttypes.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

/*
 * Converts the number given in 'str', which may be given in a humanized
 * form (as described in humanize_number(3), but with some limitations),
 * to an int64_t without units.
 * In case of success, 0 is returned and *size holds the value.
 * Otherwise, -1 is returned and *size is untouched.
 *
 * TODO: Internationalization, SI units.
 */
int
dehumanize_number(const char *str, int64_t *size)
{
	char *ep, unit;
	const char *delimit;
	long multiplier;
	long long tmp, tmp2;
	size_t len;

	len = strlen(str);
	if (len == 0) {
		errno = EINVAL;
		return -1;
	}

	multiplier = 1;

	unit = str[len - 1];
	if (isalpha((unsigned char)unit)) {
		switch (tolower((unsigned char)unit)) {
		case 'b':
			multiplier = 1;
			break;

		case 'k':
			multiplier = 1024;
			break;

		case 'm':
			multiplier = 1024 * 1024;
			break;

		case 'g':
			multiplier = 1024 * 1024 * 1024;
			break;

		default:
			errno = EINVAL;
			return -1; /* Invalid suffix. */
		}

		delimit = &str[len - 1];
	} else
		delimit = NULL;

	errno = 0;
	tmp = strtoll(str, &ep, 10);
	if (str[0] == '\0' || (ep != delimit && *ep != '\0'))
		return -1; /* Not a number. */
	else if (errno == ERANGE && (tmp == LLONG_MAX || tmp == LLONG_MIN))
		return -1; /* Out of range. */

	tmp2 = tmp * multiplier;
	tmp2 = tmp2 / multiplier;
	if (tmp != tmp2) {
		errno = ERANGE;
		return -1; /* Out of range. */
	}
	tmp *= multiplier;
#ifdef _DIAGASSERT
	_DIAGASSERT(__type_fit(int64_t, tmp));
#endif
	*size = (int64_t)tmp;

	return 0;
}
