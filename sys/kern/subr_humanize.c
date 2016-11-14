/*	$NetBSD: subr_humanize.c,v 1.1 2009/10/02 15:48:41 pooka Exp $	*/

/*-
 * Copyright (c) 1997, 1998, 1999, 2002, 2007, 2008 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Luke Mewburn.
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
__KERNEL_RCSID(0, "$NetBSD: subr_humanize.c,v 1.1 2009/10/02 15:48:41 pooka Exp $");

#include <sys/types.h>
#include <sys/systm.h>

/*
 * snprintf() `bytes' into `buf', reformatting it so that the number,
 * plus a possible `x' + suffix extension) fits into len bytes (including
 * the terminating NUL).
 * Returns the number of bytes stored in buf, or -1 if there was a problem.
 * E.g, given a len of 9 and a suffix of `B':
 *	bytes		result
 *	-----		------
 *	99999		`99999 B'
 *	100000		`97 kB'
 *	66715648	`65152 kB'
 *	252215296	`240 MB'
 */
int
humanize_number(char *buf, size_t len, uint64_t bytes, const char *suffix,
    int divisor)
{
       	/* prefixes are: (none), kilo, Mega, Giga, Tera, Peta, Exa */
	const char *prefixes;
	int		r;
	uint64_t	umax;
	size_t		i, suffixlen;

	if (buf == NULL || suffix == NULL)
		return (-1);
	if (len > 0)
		buf[0] = '\0';
	suffixlen = strlen(suffix);
	/* check if enough room for `x y' + suffix + `\0' */
	if (len < 4 + suffixlen)
		return (-1);

	if (divisor == 1024) {
		/*
		 * binary multiplies
		 * XXX IEC 60027-2 recommends Ki, Mi, Gi...
		 */
		prefixes = " KMGTPE";
	} else
		prefixes = " kMGTPE"; /* SI for decimal multiplies */

	umax = 1;
	for (i = 0; i < len - suffixlen - 3; i++) {
		umax *= 10;
		if (umax > bytes)
			break;
	}
	for (i = 0; bytes >= umax && prefixes[i + 1]; i++)
		bytes /= divisor;

	r = snprintf(buf, len, "%qu%s%c%s", (unsigned long long)bytes,
	    i == 0 ? "" : " ", prefixes[i], suffix);

	return (r);
}

int
format_bytes(char *buf, size_t len, uint64_t bytes)
{
	int	rv;
	size_t	nlen;

	rv = humanize_number(buf, len, bytes, "B", 1024);
	if (rv != -1) {
			/* nuke the trailing ` B' if it exists */
		nlen = strlen(buf) - 2;
		if (strcmp(&buf[nlen], " B") == 0)
			buf[nlen] = '\0';
	}
	return (rv);
}
