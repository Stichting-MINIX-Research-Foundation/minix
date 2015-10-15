/*	$NetBSD: files.c,v 1.42 2015/08/05 07:10:03 mrg Exp $	*/

/*-
 * Copyright (c) 2000-2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Ben Harris and Jaromir Dolecek.
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

/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Peter McIlroy.
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

#include "sort.h"
#include "fsort.h"

__RCSID("$NetBSD: files.c,v 1.42 2015/08/05 07:10:03 mrg Exp $");

#include <string.h>

/* Align records in temporary files to avoid misaligned copies */
#define REC_ROUNDUP(n) (((n) + sizeof (long) - 1) & ~(sizeof (long) - 1))

static ssize_t	seq(FILE *, u_char **);

/*
 * this is called when there is no special key. It's only called
 * in the first fsort pass.
 */

static u_char *opos;
static size_t osz;

void
makeline_copydown(RECHEADER *recbuf)
{
	memmove(recbuf->data, opos, osz);
}

int
makeline(FILE *fp, RECHEADER *recbuf, u_char *bufend, struct field *dummy2)
{
	u_char *pos;
	int c;

	pos = recbuf->data;
	if (osz != 0) {
		/*
		 * Buffer shortage is solved by either of two ways:
		 * o flush previous buffered data and start using the
		 *   buffer from start.
		 *   makeline_copydown() above must be called.
		 * o realloc buffer
		 * 
		 * This code has relied on realloc changing 'bufend',
		 * but that isn't necessarily true.
		 */
		pos += osz;
		osz = 0;
	}

	while (pos < bufend) {
		c = getc(fp);
		if (c == EOF) {
			if (pos == recbuf->data) {
				FCLOSE(fp);
				return EOF;
			}
			/* Add terminator to partial line */
			c = REC_D;
		}
		*pos++ = c;
		if (c == REC_D) {
			recbuf->offset = 0;
			recbuf->length = pos - recbuf->data;
			recbuf->keylen = recbuf->length - 1;
			return (0);
		}
	}

	/* Ran out of buffer space... */
	if (recbuf->data < bufend) {
		/* Remember where the partial record is */
		osz = pos - recbuf->data;
		opos = recbuf->data;
	}
	return (BUFFEND);
}

/*
 * This generates keys. It's only called in the first fsort pass
 */
int
makekey(FILE *fp, RECHEADER *recbuf, u_char *bufend, struct field *ftbl)
{
	static u_char *line_data;
	static ssize_t line_size;
	static int overflow = 0;

	/* We get re-entered after returning BUFFEND - save old data */
	if (overflow) {
		overflow = enterkey(recbuf, bufend, line_data, line_size, ftbl);
		return overflow ? BUFFEND : 0;
	}

	line_size = seq(fp, &line_data);
	if (line_size == 0) {
		FCLOSE(fp);
		return EOF;
	}

	if (line_size > bufend - recbuf->data) {
		overflow = 1;
	} else {
		overflow = enterkey(recbuf, bufend, line_data, line_size, ftbl);
	}
	return overflow ? BUFFEND : 0;
}

/*
 * get a line of input from fp
 */
static ssize_t
seq(FILE *fp, u_char **line)
{
	static u_char *buf;
	static size_t buf_size = DEFLLEN;
	u_char *end, *pos;
	int c;
	u_char *new_buf;

	if (!buf) {
		/* one-time initialization */
		buf = malloc(buf_size);
		if (!buf)
		    err(2, "malloc of linebuf for %zu bytes failed",
			    buf_size);
	}

	end = buf + buf_size;
	pos = buf;
	while ((c = getc(fp)) != EOF) {
		*pos++ = c;
		if (c == REC_D) {
			*line = buf;
			return pos - buf;
		}
		if (pos == end) {
			/* Long line - double size of buffer */
			/* XXX: Check here for stupidly long lines */
			buf_size *= 2;
			new_buf = realloc(buf, buf_size);
			if (!new_buf)
				err(2, "realloc of linebuf to %zu bytes failed",
					buf_size);
		
			end = new_buf + buf_size;
			pos = new_buf + (pos - buf);
			buf = new_buf;
		}
	}

	if (pos != buf) {
		/* EOF part way through line - add line terminator */
		*pos++ = REC_D;
		*line = buf;
		return pos - buf;
	}

	return 0;
}

/*
 * write a key/line pair to a temporary file
 */
void
putrec(const RECHEADER *rec, FILE *fp)
{
	EWRITE(rec, 1, REC_ROUNDUP(offsetof(RECHEADER, data) + rec->length), fp,
	       "failed to write temp file");
}

/*
 * write a line to output
 */
void
putline(const RECHEADER *rec, FILE *fp)
{
	EWRITE(rec->data+rec->offset, 1, rec->length - rec->offset, fp,
	       "failed to write");
}

/*
 * write dump of key to output (for -Dk)
 */
void
putkeydump(const RECHEADER *rec, FILE *fp)
{
	EWRITE(rec, 1, REC_ROUNDUP(offsetof(RECHEADER, data) + rec->offset), fp,
	       "failed to write debug key");
}

/*
 * get a record from a temporary file. (Used by merge sort.)
 */
int
geteasy(FILE *fp, RECHEADER *rec, u_char *end, struct field *dummy2)
{
	length_t file_len;
	int i;

	(void)sizeof (char[offsetof(RECHEADER, length) == 0 ? 1 : -1]);

	if ((u_char *)(rec + 1) > end)
		return (BUFFEND);
	if (!fread(&rec->length, 1, sizeof rec->length, fp)) {
		fclose(fp);
		return (EOF);
	}
	file_len = REC_ROUNDUP(offsetof(RECHEADER, data) + rec->length);
	if (end - rec->data < (ptrdiff_t)file_len) {
		for (i = sizeof rec->length - 1; i >= 0;  i--)
			ungetc(*((char *) rec + i), fp);
		return (BUFFEND);
	}

	fread(&rec->length + 1, file_len - sizeof rec->length, 1, fp);
	return (0);
}
