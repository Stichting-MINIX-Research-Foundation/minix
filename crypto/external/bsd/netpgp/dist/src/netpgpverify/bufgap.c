/* $NetBSD: bufgap.c,v 1.1 2014/03/09 00:15:45 agc Exp $ */

/*-
 * Copyright (c) 1996-2009 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Alistair Crooks (agc@NetBSD.org)
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
#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "bufgap.h"
#include "defs.h"

/* macros to get subscripts in buffer */
#define AFTSUB(bp, n)	((bp)->buf[(int)n])
#define BEFSUB(bp, n)	((bp)->buf[(int)((bp)->size - (n) - 1)])

/* initial allocation size */
#ifndef CHUNKSIZE
#define CHUNKSIZE	256
#endif

#ifndef KiB
#define KiB(x)	((x) * 1024)
#endif

#define BGCHUNKSIZE	KiB(4)

#ifndef __UNCONST
#define __UNCONST(a)       ((void *)(unsigned long)(const void *)(a))
#endif

#ifndef USE_UTF
#define USE_UTF	0
#endif

#if !USE_UTF
#define Rune		char
#define	utfbytes(x)	strlen(x)
#define	utfrune(a, b)	strchr(a, b)
#define	utfnlen(a, b)	bounded_strlen(a, b)

static size_t
bounded_strlen(const char *s, size_t maxlen)
{
	size_t	n;

	for (n = 0 ; n < maxlen && s[n] != 0x0 ; n++) {
	}
	return n;
}

static int
chartorune(Rune *rp, char *s)
{
	*rp = s[0];
	return 1;
}

static int
priorrune(Rune *rp, char *s)
{
	*rp = s[0];
	return 1;
}
#else
#include "ure.h"
#endif

/* save `n' chars of `s' in malloc'd memory */
static char *
strnsave(char *s, int n)
{
	char	*cp;

	if (n < 0) {
		n = (int)strlen(s);
	}
	NEWARRAY(char, cp, n + 1, "strnsave", return NULL);
	(void) memcpy(cp, s, (size_t)n);
	cp[n] = 0x0;
	return cp;
}

/* open a file in a buffer gap structure */
int
bufgap_open(bufgap_t *bp, const char *f)
{
	struct stat	 s;
	int64_t		 cc;
	FILE		*filep;
	char		*cp;

	(void) memset(bp, 0x0, sizeof(*bp));
	filep = NULL;
	if (f != NULL && (filep = fopen(f, "r")) == NULL) {
		return 0;
	}
	if (f == NULL) {
		bp->size = BGCHUNKSIZE;
		NEWARRAY(char, bp->buf, bp->size, "f_open", return 0);
	} else {
		(void) fstat(fileno(filep), &s);
		bp->size = (int) ((s.st_size / BGCHUNKSIZE) + 1) * BGCHUNKSIZE;
		NEWARRAY(char, bp->buf, bp->size, "f_open", return 0);
		cc = fread(&BEFSUB(bp, s.st_size), sizeof(char),
						(size_t)s.st_size, filep);
		(void) fclose(filep);
		if (cc != s.st_size) {
			FREE(bp->buf);
			FREE(bp);
			return 0;
		}
		bp->name = strnsave(__UNCONST(f), (int)utfbytes(__UNCONST(f)));
		bp->bbc = s.st_size;
		cp = &BEFSUB(bp, cc);
		for (;;) {
			if ((cp = utfrune(cp, '\n')) == NULL) {
				break;
			}
			bp->blc++;
			cp++;
		}
		bp->bcc = utfnlen(&BEFSUB(bp, cc), (size_t)cc);
	}
	return 1;
}

/* close a buffer gapped file */
void
bufgap_close(bufgap_t *bp)
{
	FREE(bp->buf);
}

/* move forwards `n' chars/bytes in a buffer gap */
int
bufgap_forwards(bufgap_t *bp, uint64_t n, int type)
{
	Rune	r;
	int	rlen;

	switch(type) {
	case BGChar:
		if (bp->bcc >= n) {
			while (n-- > 0) {
				rlen = chartorune(&r, &BEFSUB(bp, bp->bbc));
				if (rlen == 1) {
					AFTSUB(bp, bp->abc) = BEFSUB(bp, bp->bbc);
				} else {
					(void) memmove(&AFTSUB(bp, bp->abc),
							&BEFSUB(bp, bp->bbc),
							(size_t)rlen);
				}
				bp->acc++;
				bp->bcc--;
				bp->abc += rlen;
				bp->bbc -= rlen;
				if (r == '\n') {
					bp->alc++;
					bp->blc--;
				}
			}
			return 1;
		}
		break;
	case BGByte:
		if (bp->bbc >= n) {
			for ( ; n > 0 ; n -= rlen) {
				rlen = chartorune(&r, &BEFSUB(bp, bp->bbc));
				if (rlen == 1) {
					AFTSUB(bp, bp->abc) = BEFSUB(bp, bp->bbc);
				} else {
					(void) memmove(&AFTSUB(bp, bp->abc),
							&BEFSUB(bp, bp->bbc),
							(size_t)rlen);
				}
				bp->acc++;
				bp->bcc--;
				bp->abc += rlen;
				bp->bbc -= rlen;
				if (r == '\n') {
					bp->alc++;
					bp->blc--;
				}
			}
			return 1;
		}
	}
	return 0;
}

/* move backwards `n' chars in a buffer gap */
int
bufgap_backwards(bufgap_t *bp, uint64_t n, int type)
{
	Rune	r;
	int	rlen;

	switch(type) {
	case BGChar:
		if (bp->acc >= n) {
			while (n-- > 0) {
				rlen = priorrune(&r, &AFTSUB(bp, bp->abc));
				bp->bcc++;
				bp->acc--;
				bp->bbc += rlen;
				bp->abc -= rlen;
				if (rlen == 1) {
					BEFSUB(bp, bp->bbc) = AFTSUB(bp, bp->abc);
				} else {
					(void) memmove(&BEFSUB(bp, bp->bbc),
							&AFTSUB(bp, bp->abc),
							(size_t)rlen);
				}
				if (r == '\n') {
					bp->blc++;
					bp->alc--;
				}
			}
			return 1;
		}
		break;
	case BGByte:
		if (bp->acc >= n) {
			for ( ; n > 0 ; n -= rlen) {
				rlen = priorrune(&r, &AFTSUB(bp, bp->abc));
				bp->bcc++;
				bp->acc--;
				bp->bbc += rlen;
				bp->abc -= rlen;
				if (rlen == 1) {
					BEFSUB(bp, bp->bbc) = AFTSUB(bp, bp->abc);
				} else {
					(void) memmove(&BEFSUB(bp, bp->bbc),
							&AFTSUB(bp, bp->abc),
							(size_t)rlen);
				}
				if (r == '\n') {
					bp->blc++;
					bp->alc--;
				}
			}
			return 1;
		}
	}
	return 0;
}

/* move within a buffer gap */
int
bufgap_seek(bufgap_t *bp, int64_t off, int whence, int type)
{
	switch(type) {
	case BGLine:
		switch(whence) {
		case BGFromBOF:
			if (off < 0 || off > (int64_t)(bp->alc + bp->blc)) {
				return 0;
			}
			if (off < (int64_t)bp->alc) {
				while (off <= (int64_t)bp->alc && bufgap_backwards(bp, 1, BGChar)) {
				}
				if (off > 0) {
					(void) bufgap_forwards(bp, 1, BGChar);
				}
			} else if (off > (int64_t)bp->alc) {
				while (off > (int64_t)bp->alc && bufgap_forwards(bp, 1, BGChar)) {
				}
			}
			return 1;
		case BGFromHere:
			return bufgap_seek(bp, (int64_t)(bp->alc + off), BGFromBOF, BGLine);
		case BGFromEOF:
			return bufgap_seek(bp, (int64_t)(bp->alc + bp->blc + off), BGFromBOF, BGLine);
		}
		break;
	case BGChar:
		switch(whence) {
		case BGFromBOF:
			if (off < 0 || off > (int64_t)(bp->acc + bp->bcc)) {
				return 0;
			}
			if (off < (int64_t)bp->acc) {
				return bufgap_backwards(bp, bp->acc - off, BGChar);
			} else if (off > (int64_t)bp->acc) {
				return bufgap_forwards(bp, off - bp->acc, BGChar);
			}
			return 1;
		case BGFromHere:
			return bufgap_seek(bp, (int64_t)(bp->acc + off), BGFromBOF, BGChar);
		case BGFromEOF:
			return bufgap_seek(bp, (int64_t)(bp->acc + bp->bcc + off), BGFromBOF, BGChar);
		}
		break;
	case BGByte:
		switch(whence) {
		case BGFromBOF:
			if (off < 0 || off > (int64_t)(bp->abc + bp->bbc)) {
				return 0;
			}
			if (off < (int64_t)bp->abc) {
				return bufgap_backwards(bp, bp->abc - off, BGByte);
			} else if (off > (int64_t)bp->abc) {
				return bufgap_forwards(bp, off - bp->abc, BGByte);
			}
			return 1;
		case BGFromHere:
			return bufgap_seek(bp, (int64_t)(bp->abc + off), BGFromBOF, BGByte);
		case BGFromEOF:
			return bufgap_seek(bp, (int64_t)(bp->abc + bp->bbc + off), BGFromBOF, BGByte);
		}
		break;
	}
	return 0;
}

/* return a pointer to the text in the buffer gap */
char *
bufgap_getstr(bufgap_t *bp)
{
	return &BEFSUB(bp, bp->bbc);
}

/* return the binary text in the buffer gap */
int
bufgap_getbin(bufgap_t *bp, void *dst, size_t len)
{
	int	cc;

	cc = (bp->bcc < len) ? (int)bp->bcc : (int)len;
	(void) memcpy(dst, &BEFSUB(bp, bp->bbc), len);
	return cc;
}

/* return offset (from beginning/end) in a buffer gap */
int64_t
bufgap_tell(bufgap_t *bp, int whence, int type)
{
	switch(whence) {
	case BGFromBOF:
		return (type == BGLine) ? bp->alc :
			(type == BGByte) ? bp->abc : bp->acc;
	case BGFromEOF:
		return (type == BGLine) ? bp->blc :
			(type == BGByte) ? bp->bbc : bp->bcc;
	default:
		(void) fprintf(stderr, "weird whence in bufgap_tell\n");
		break;
	}
	return (int64_t)0;
}

/* return size of buffer gap */
int64_t
bufgap_size(bufgap_t *bp, int type)
{
	return (type == BGLine) ? bp->alc + bp->blc :
		(type == BGChar) ? bp->acc + bp->bcc :
			bp->abc + bp->bbc;
}

/* insert `n' chars of `s' in a buffer gap */
int
bufgap_insert(bufgap_t *bp, const char *s, int n)
{
	int64_t	off;
	Rune	r;
	int	rlen;
	int	i;

	if (n < 0) {
		n = (int)strlen(s);
	}
	for (i = 0 ; i < n ; i += rlen) {
		if (bp->bbc + bp->abc == bp->size) {
			off = bufgap_tell(bp, BGFromBOF, BGChar);
			(void) bufgap_seek(bp, 0, BGFromEOF, BGChar);
			bp->size *= 2;
			RENEW(char, bp->buf, bp->size, "bufgap_insert", return 0);
			(void) bufgap_seek(bp, off, BGFromBOF, BGChar);
		}
		if ((rlen = chartorune(&r, __UNCONST(s))) == 1) {
			AFTSUB(bp, bp->abc) = *s;
		} else {
			(void) memmove(&AFTSUB(bp, bp->abc), s, (size_t)rlen);
		}
		if (r == '\n') {
			bp->alc++;
		}
		bp->modified = 1;
		bp->abc += rlen;
		bp->acc++;
		s += rlen;
	}
	return 1;
}

/* delete `n' bytes from the buffer gap */
int
bufgap_delete(bufgap_t *bp, uint64_t n)
{
	uint64_t	i;
	Rune		r;
	int		rlen;

	if (n <= bp->bbc) {
		for (i = 0 ; i < n ; i += rlen) {
			rlen = chartorune(&r, &BEFSUB(bp, bp->bbc));
			if (r == '\n') {
				bp->blc--;
			}
			bp->bbc -= rlen;
			bp->bcc--;
			bp->modified = 1;
		}
		return 1;
	}
	return 0;
}

/* look at a character in a buffer gap `delta' UTF chars away */
int
bufgap_peek(bufgap_t *bp, int64_t delta)
{
	int	ch;

	if (delta != 0) {
		if (!bufgap_seek(bp, delta, BGFromHere, BGChar)) {
			return -1;
		}
	}
	ch = BEFSUB(bp, bp->bbc);
	if (delta != 0) {
		(void) bufgap_seek(bp, -delta, BGFromHere, BGChar);
	}
	return ch;
}

/* return, in malloc'd storage, text from the buffer gap */
char *
bufgap_gettext(bufgap_t *bp, int64_t from, int64_t to)
{
	int64_t	 off;
	int64_t	 n;
	char	*text;

	off = bufgap_tell(bp, BGFromBOF, BGChar);
	NEWARRAY(char, text, (to - from + 1), "bufgap_gettext", return NULL);
	(void) bufgap_seek(bp, from, BGFromBOF, BGChar);
	for (n = 0 ; n < to - from ; n++) {
		text[(int)n] = BEFSUB(bp, bp->bbc - n);
	}
	text[(int)n] = 0x0;
	(void) bufgap_seek(bp, off, BGFromBOF, BGChar);
	return text;
}

/* return 1 if we wrote the file correctly */
int
bufgap_write(bufgap_t *bp, FILE *filep)
{
	if (fwrite(bp->buf, sizeof(char), (size_t)bp->abc, filep) != (size_t)bp->abc) {
		return 0;
	}
	if (fwrite(&BEFSUB(bp, bp->bbc), sizeof(char), (size_t)bp->bbc, filep) != (size_t)bp->bbc) {
		return 0;
	}
	return 1;
}

/* tell if the buffer gap is dirty - has been modified */
int
bufgap_dirty(bufgap_t *bp)
{
	return (int)bp->modified;
}
