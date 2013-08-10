/*	$NetBSD: display.c,v 1.21 2009/01/18 21:34:32 apb Exp $	*/

/*
 * Copyright (c) 1989, 1993
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

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <sys/cdefs.h>
#if !defined(lint)
#if 0
static char sccsid[] = "@(#)display.c	8.1 (Berkeley) 6/6/93";
#else
__RCSID("$NetBSD: display.c,v 1.21 2009/01/18 21:34:32 apb Exp $");
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <util.h>

#include "hexdump.h"

enum _vflag vflag = FIRST;

static off_t address;			/* address/offset in stream */
static off_t eaddress;			/* end address */

static inline void print(PR *, u_char *);

void
display(void)
{
	FS *fs;
	FU *fu;
	PR *pr;
	int cnt;
	u_char *bp;
	off_t saveaddress;
	u_char savech, *savebp;

	savech = 0;
	while ((bp = get()) != NULL)
	    for (fs = fshead, savebp = bp, saveaddress = address; fs;
		fs = fs->nextfs, bp = savebp, address = saveaddress)
		    for (fu = fs->nextfu; fu; fu = fu->nextfu) {
			if (fu->flags&F_IGNORE)
				break;
			for (cnt = fu->reps; cnt; --cnt)
			    for (pr = fu->nextpr; pr; address += pr->bcnt,
				bp += pr->bcnt, pr = pr->nextpr) {
				    if (eaddress && address >= eaddress &&
					!(pr->flags & (F_TEXT|F_BPAD)))
					    bpad(pr);
				    if (cnt == 1 && pr->nospace) {
					savech = *pr->nospace;
					*pr->nospace = '\0';
				    }
				    print(pr, bp);
				    if (cnt == 1 && pr->nospace)
					*pr->nospace = savech;
			    }
		    }
	if (endfu) {
		/*
		 * If eaddress not set, error or file size was multiple of
		 * blocksize, and no partial block ever found.
		 */
		if (!eaddress) {
			if (!address)
				return;
			eaddress = address;
		}
		for (pr = endfu->nextpr; pr; pr = pr->nextpr)
			switch(pr->flags) {
			case F_ADDRESS:
				(void)printf(pr->fmt, (int64_t)eaddress);
				break;
			case F_TEXT:
				(void)printf("%s", pr->fmt);
				break;
			}
	}
}

static inline void
print(PR *pr, u_char *bp)
{
	   double f8;
	    float f4;
	  int16_t s2;
	  int32_t s4;
	  int64_t s8;
	 uint16_t u2;
	 uint32_t u4;
	 uint64_t u8;

	switch(pr->flags) {
	case F_ADDRESS:
		(void)printf(pr->fmt, (int64_t)address);
		break;
	case F_BPAD:
		(void)printf(pr->fmt, "");
		break;
	case F_C:
		conv_c(pr, bp);
		break;
	case F_CHAR:
		(void)printf(pr->fmt, *bp);
		break;
	case F_DBL:
		switch(pr->bcnt) {
		case 4:
			memmove(&f4, bp, sizeof(f4));
			(void)printf(pr->fmt, f4);
			break;
		case 8:
			memmove(&f8, bp, sizeof(f8));
			(void)printf(pr->fmt, f8);
			break;
		}
		break;
	case F_INT:
		switch(pr->bcnt) {
		case 1:
			(void)printf(pr->fmt, (int64_t)*bp);
			break;
		case 2:
			memmove(&s2, bp, sizeof(s2));
			(void)printf(pr->fmt, (int64_t)s2);
			break;
		case 4:
			memmove(&s4, bp, sizeof(s4));
			(void)printf(pr->fmt, (int64_t)s4);
			break;
		case 8:
			memmove(&s8, bp, sizeof(s8));
			(void)printf(pr->fmt, (int64_t)s8);
			break;
		}
		break;
	case F_P:
		(void)printf(pr->fmt, isprint(*bp) ? *bp : '.');
		break;
	case F_STR:
		(void)printf(pr->fmt, (char *)bp);
		break;
	case F_TEXT:
		(void)printf("%s", pr->fmt);
		break;
	case F_U:
		conv_u(pr, bp);
		break;
	case F_UINT:
		switch(pr->bcnt) {
		case 1:
			(void)printf(pr->fmt, (uint64_t)*bp);
			break;
		case 2:
			memmove(&u2, bp, sizeof(u2));
			(void)printf(pr->fmt, (uint64_t)u2);
			break;
		case 4:
			memmove(&u4, bp, sizeof(u4));
			(void)printf(pr->fmt, (uint64_t)u4);
			break;
		case 8:
			memmove(&u8, bp, sizeof(u8));
			(void)printf(pr->fmt, (uint64_t)u8);
			break;
		}
		break;
	}
}

void
bpad(PR *pr)
{
	static const char *spec = " -0+#";
	char *p1, *p2;

	/*
	 * Remove all conversion flags; '-' is the only one valid
	 * with %s, and it's not useful here.
	 */
	pr->flags = F_BPAD;
	pr->cchar[0] = 's';
	pr->cchar[1] = '\0';
	for (p1 = pr->fmt; *p1 != '%'; ++p1);
	for (p2 = ++p1; *p1 && strchr(spec, *p1); ++p1);
	while ((*p2++ = *p1++) != '\0');
}

static char **_argv;

u_char *
get(void)
{
	static int ateof = 1;
	static u_char *curp, *savp;
	int n;
	int need, nread;
	u_char *tmpp;

	if (!curp) {
		curp = ecalloc(blocksize, 1);
		savp = ecalloc(blocksize, 1);
	} else {
		tmpp = curp;
		curp = savp;
		savp = tmpp;
		address += blocksize;
	}
	for (need = blocksize, nread = 0;;) {
		/*
		 * if read the right number of bytes, or at EOF for one file,
		 * and no other files are available, zero-pad the rest of the
		 * block and set the end flag.
		 */
		if (!length || (ateof && !next(NULL))) {
			if (need == blocksize)
				return(NULL);
			if (!need && vflag != ALL &&
			    !memcmp(curp, savp, nread)) {
				if (vflag != DUP)
					(void)printf("*\n");
				return(NULL);
			}
			memset((char *)curp + nread, 0, need);
			eaddress = address + nread;
			return(curp);
		}
		n = fread((char *)curp + nread, sizeof(u_char),
		    length == -1 ? need : MIN(length, need), stdin);
		if (!n) {
			if (ferror(stdin))
				warn("%s", _argv[-1]);
			ateof = 1;
			continue;
		}
		ateof = 0;
		if (length != -1)
			length -= n;
		if (!(need -= n)) {
			if (vflag == ALL || vflag == FIRST ||
			    memcmp(curp, savp, blocksize)) {
				if (vflag == DUP || vflag == FIRST)
					vflag = WAIT;
				return(curp);
			}
			if (vflag == WAIT)
				(void)printf("*\n");
			vflag = DUP;
			address += blocksize;
			need = blocksize;
			nread = 0;
		}
		else
			nread += n;
	}
}

int
next(char **argv)
{
	static int done;
	int statok;

	if (argv) {
		_argv = argv;
		return(1);
	}
	for (;;) {
		if (*_argv) {
			if (!(freopen(*_argv, "r", stdin))) {
				warn("%s", *_argv);
				exitval = 1;
				++_argv;
				continue;
			}
			statok = done = 1;
		} else {
			if (done++)
				return(0);
			statok = 0;
		}
		if (skip)
			doskip(statok ? *_argv : "stdin", statok);
		if (*_argv)
			++_argv;
		if (!skip)
			return(1);
	}
	/* NOTREACHED */
}

void
doskip(const char *fname, int statok)
{
	int cnt;
	struct stat sb;

	if (statok) {
		if (fstat(fileno(stdin), &sb))
			err(1, "fstat %s", fname);
		if (S_ISREG(sb.st_mode) && skip >= sb.st_size) {
			address += sb.st_size;
			skip -= sb.st_size;
			return;
		}
	}
	if (S_ISREG(sb.st_mode)) {
		if (fseek(stdin, skip, SEEK_SET))
			err(1, "fseek %s", fname);
		address += skip;
		skip = 0;
	} else {
		for (cnt = 0; cnt < skip; ++cnt)
			if (getchar() == EOF)
				break;
		address += cnt;
		skip -= cnt;
	}
}
