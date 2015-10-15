/*	$NetBSD: gettemp.c,v 1.20 2015/02/05 16:05:20 christos Exp $	*/

/*
 * Copyright (c) 1987, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "gettemp.h"

#if !HAVE_NBTOOL_CONFIG_H || !HAVE_MKSTEMP || !HAVE_MKDTEMP

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char sccsid[] = "@(#)mktemp.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: gettemp.c,v 1.20 2015/02/05 16:05:20 christos Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include <sys/param.h>
#include <fcntl.h>
#include <string.h>

static const unsigned char padchar[] =
"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

int
GETTEMP(char *path, int *doopen, int domkdir, int slen, int oflags)
{
	char *start, *trv, *suffp, *carryp;
	char *pad;
	struct stat sbuf;
	int rval;
	uint32_t r;
	char carrybuf[MAXPATHLEN];

	_DIAGASSERT(path != NULL);
	/* doopen may be NULL */
	if ((doopen != NULL && domkdir) || slen < 0 ||
	    (oflags & ~(O_APPEND | O_DIRECT | O_SHLOCK | O_EXLOCK | O_SYNC |
	    O_CLOEXEC)) != 0) {
		errno = EINVAL;
		return 0;
	}

	for (trv = path; *trv != '\0'; ++trv)
		continue;

	if (trv - path >= MAXPATHLEN) {
		errno = ENAMETOOLONG;
		return 0;
	}
	trv -= slen;
	suffp = trv;
	--trv;
	if (trv < path || NULL != strchr(suffp, '/')) {
		errno = EINVAL;
		return 0;
	}

	/* Fill space with random characters */
	while (trv >= path && *trv == 'X') {
		r = arc4random_uniform((unsigned int)(sizeof(padchar) - 1));
		*trv-- = padchar[r];
	}
	start = trv + 1;

	/* save first combination of random characters */
	memcpy(carrybuf, start, (size_t)(suffp - start));

	/*
	 * check the target directory.
	 */
	if (doopen != NULL || domkdir) {
		for (; trv > path; --trv) {
			if (*trv == '/') {
				*trv = '\0';
				rval = stat(path, &sbuf);
				*trv = '/';
				if (rval != 0)
					return 0;
				if (!S_ISDIR(sbuf.st_mode)) {
					errno = ENOTDIR;
					return 0;
				}
				break;
			}
		}
	}

	for (;;) {
		if (doopen) {
			if ((*doopen = open(path, O_CREAT|O_EXCL|O_RDWR|oflags,
			    0600)) != -1)
				return 1;
			if (errno != EEXIST)
				return 0;
		} else if (domkdir) {
			if (mkdir(path, 0700) != -1)
				return 1;
			if (errno != EEXIST)
				return 0;
		} else if (lstat(path, &sbuf))
			return errno == ENOENT;

		/*
		 * If we have a collision,
		 * cycle through the space of filenames
		 */
		for (trv = start, carryp = carrybuf;;) {
			/* have we tried all possible permutations? */
			if (trv == suffp)
				return 0; /* yes - exit with EEXIST */
			pad = strchr((const char *)padchar, *trv);
			if (pad == NULL) {
				/* this should never happen */
				errno = EIO;
				return 0;
			}
			/* increment character */
			*trv = (*++pad == '\0') ? padchar[0] : *pad;
			/* carry to next position? */
			if (*trv == *carryp) {
				/* increment position and loop */
				++trv;
				++carryp;
			} else {
				/* try with new name */
				break;
			}
		}
	}
	/*NOTREACHED*/
}

#endif /* !HAVE_NBTOOL_CONFIG_H || !HAVE_MKSTEMP || !HAVE_MKDTEMP */
