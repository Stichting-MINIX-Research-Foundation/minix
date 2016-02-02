/*	$NetBSD: expand.c,v 1.6 2013/05/06 08:02:20 skrll Exp $	*/

/*-
 * Copyright (c) 2007 The NetBSD Foundation, Inc.
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
#ifndef lint
__RCSID("$NetBSD: expand.c,v 1.6 2013/05/06 08:02:20 skrll Exp $");
#endif /* not lint */

#include <ctype.h>
#include <string.h>
#include <sys/sysctl.h>

#ifdef DEBUG_EXPAND
#include <stdio.h>
#include <err.h>
#define xwarn warn
#define xerr err
size_t _rtld_expand_path(char *, size_t, const char *, const char *,
    const char *);
#else
#include <sys/stat.h>
#include "rtld.h"
#endif

static const struct {
	const char *name;
	size_t namelen;
} bltn[] = {
#define ADD(a)	{ #a, sizeof(#a) - 1 },
	ADD(HWCAP)	/* SSE, MMX, etc */
	ADD(ISALIST)	/* XXX */
	ADD(ORIGIN) 	/* dirname argv[0] */
	ADD(OSNAME)	/* uname -s */
	ADD(OSREL)	/* uname -r */
	ADD(PLATFORM)	/* uname -p */
};

static int mib[3][2] = {
	{ CTL_KERN, KERN_OSTYPE },
	{ CTL_KERN, KERN_OSRELEASE },
	{ CTL_HW, HW_MACHINE_ARCH },
};

static size_t
expand(char *buf, const char *execname, int what, size_t bl)
{
	const char *p, *ep;
	char *bp = buf;
	size_t len;
	char name[32];

	switch (what) {
	case 0:	/* HWCAP XXX: Not yet */
	case 1:	/* ISALIST XXX: Not yet */
		return 0;

	case 2:	/* ORIGIN */
		if (execname == NULL)
			xerr(1, "execname not specified in AUX vector");
		if ((ep = strrchr(p = execname, '/')) == NULL)
			xerr(1, "bad execname `%s' in AUX vector", execname);
		break;

	case 3:	/* OSNAME */
	case 4:	/* OSREL */
	case 5:	/* PLATFORM */
		len = sizeof(name);
		if (sysctl(mib[what - 3], 2, name, &len, NULL, 0) == -1) {
			xwarn("sysctl");
			return 0;
		}
		ep = (p = name) + len - 1;
		break;
	default:
		return 0;
	}

	while (p != ep && bl)
		*bp++ = *p++, bl--;

	return bp - buf;
}


size_t
_rtld_expand_path(char *buf, size_t bufsize, const char *execname,
    const char *bp, const char *ep)
{
	size_t i, ds = bufsize;
	char *dp = buf;
	const char *p;
	int br;

	for (p = bp; p < ep;) {
		if (*p == '$') {
			br = *++p == '{';

			if (br)
				p++;

			for (i = 0; i < sizeof(bltn) / sizeof(bltn[0]); i++) {
				size_t s = bltn[i].namelen;
				const char *es = p + s;

				if ((br && *es != '}') ||
				    (!br && (es != ep &&
					isalpha((unsigned char)*es))))
					continue;

				if (strncmp(bltn[i].name, p, s) == 0) {
					size_t ls = expand(dp, execname, i, ds);
					if (ls >= ds)
						return bufsize;
					ds -= ls;
					dp += ls;
					p = es + br;
					goto done;
				}
			}
			p -= br + 1;

		}
		*dp++ = *p++;
		ds--;
done:;
	}
	*dp = '\0';
	return dp - buf;
}

#ifdef DEBUG_EXPAND
int
main(int argc, char *argv[])
{
	char buf[1024];
	size_t i;

	for (i = 1; i < argc; i++) {
		char *p = argv[i], *ep = argv[i] + strlen(p);
		size_t n = _rtld_expand_path(buf, sizeof(buf), argv[0], p, ep);
		printf("%s\n", buf);
	}
	return 0;
}
#endif
