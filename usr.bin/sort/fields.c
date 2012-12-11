/*	$NetBSD: fields.c,v 1.32 2010/12/18 23:09:48 christos Exp $	*/

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

/* Subroutines to generate sort keys. */

#include "sort.h"

__RCSID("$NetBSD: fields.c,v 1.32 2010/12/18 23:09:48 christos Exp $");

#define SKIP_BLANKS(ptr) {					\
	if (BLANK & d_mask[*(ptr)])				\
		while (BLANK & d_mask[*(++(ptr))]);		\
}

#define NEXTCOL(pos) {						\
	if (!SEP_FLAG)						\
		while (BLANK & l_d_mask[*(++pos)]);		\
	while ((*(pos+1) != '\0') && !((FLD_D | REC_D_F) & l_d_mask[*++pos]));\
}
		
static u_char *enterfield(u_char *, const u_char *, struct field *, int);
static u_char *number(u_char *, const u_char *, u_char *, u_char *, int);
static u_char *length(u_char *, const u_char *, u_char *, u_char *, int);

#ifdef __minix
static u_char *numhex(u_char *, const u_char *, u_char *, u_char *, int);
#endif

#define DECIMAL_POINT '.'

/*
 * constructs sort key with leading recheader, followed by the key,
 * followed by the original line.
 */
length_t
enterkey(RECHEADER *keybuf, const u_char *keybuf_end, u_char *line_data,
    size_t line_size, struct field fieldtable[])
	/* keybuf:	 pointer to start of key */
{
	int i;
	u_char *l_d_mask;
	u_char *lineend, *pos;
	const u_char *endkey;
	u_char *keypos;
	struct coldesc *clpos;
	int col = 1;
	struct field *ftpos;

	l_d_mask = d_mask;
	pos = line_data - 1;
	lineend = line_data + line_size-1;
				/* don't include rec_delimiter */

	for (i = 0; i < ncols; i++) {
		clpos = clist + i;
		for (; (col < clpos->num) && (pos < lineend); col++) {
			NEXTCOL(pos);
		}
		if (pos >= lineend)
			break;
		clpos->start = SEP_FLAG ? pos + 1 : pos;
		NEXTCOL(pos);
		clpos->end = pos;
		col++;
		if (pos >= lineend) {
			clpos->end = lineend;
			i++;
			break;
		}
	}
	for (; i <= ncols; i++)
		clist[i].start = clist[i].end = lineend;
	if (clist[0].start < line_data)
		clist[0].start++;

	/*
	 * We write the sort keys (concatenated) followed by the
	 * original line data (for output) as the 'keybuf' data.
	 * keybuf->length is the number of key bytes + data bytes.
	 * keybuf->offset is the number of key bytes.
	 * We add a record separator weight after the key in case
	 * (as is usual) we need to preserve the order of equal lines,
	 * and for 'sort -u'.
	 * The key itself will have had the correct weight applied.
	 */
	keypos = keybuf->data;
	endkey = keybuf_end - line_size - 1;
	if (endkey <= keypos)
		/* No room for any key bytes */
		return 1;

	for (ftpos = fieldtable + 1; ftpos->icol.num; ftpos++) {
		if ((keypos = enterfield(keypos, endkey, ftpos,
		    fieldtable->flags)) == NULL)
			return (1);
	}

	keybuf->offset = keypos - keybuf->data;
	keybuf->length = keybuf->offset + line_size;

	/*
	 * Posix requires that equal keys be further sorted by the
	 * entire original record.
	 * NetBSD has (at least for some time) kept equal keys in
	 * their original order.
	 * For 'sort -u' posix_sort is unset.
	 */
	keybuf->keylen = posix_sort ? keybuf->length : keybuf->offset;

	memcpy(keypos, line_data, line_size);
	return (0);
}

/*
 * constructs a field (as defined by -k) within a key
 */
static u_char *
enterfield(u_char *tablepos, const u_char *endkey, struct field *cur_fld,
    int gflags)
{
	u_char *start, *end, *lineend, *mask, *lweight;
	struct column icol, tcol;
	u_int flags;

	icol = cur_fld->icol;
	tcol = cur_fld->tcol;
	flags = cur_fld->flags;
	start = icol.p->start;
	lineend = clist[ncols].end;
	if (flags & BI)
		SKIP_BLANKS(start);
	start += icol.indent;
	start = min(start, lineend);

	if (!tcol.num)
		end = lineend;
	else {
		if (tcol.indent) {
			end = tcol.p->start;
			if (flags & BT)
				SKIP_BLANKS(end);
			end += tcol.indent;
			end = min(end, lineend);
		} else
			end = tcol.p->end;
	}

	if (flags & L)
		return length(tablepos, endkey, start, end, flags);
	if (flags & N)
		return number(tablepos, endkey, start, end, flags);
#ifdef __minix
	if (flags & X)
		return numhex(tablepos, endkey, start, end, flags);
#endif

	/* Bound check space - assuming nothing is skipped */
	if (tablepos + (end - start) + 1 >= endkey)
		return NULL;

	mask = cur_fld->mask;
	lweight = cur_fld->weights;	
	for (; start < end; start++) {
		if (!mask || mask[*start]) {
			*tablepos++ = lweight[*start];
		}
	}
	/* Add extra byte (absent from lweight) to sort short keys correctly */
	*tablepos++ = lweight[REC_D];
	return tablepos;
}

/*
 * Numbers are converted to a floating point format (exponent & mantissa)
 * so that they compare correctly as sequence of unsigned bytes.
 * Bytes 0x00 and 0xff are used to terminate positive and negative numbers
 * to ensure that 0.123 sorts after 0.12 and -0.123 sorts before -0.12.
 *
 * The first byte contain the overall sign, exponent sign and some of the
 * exponent. These have to be ordered (-ve value, decreasing exponent),
 * zero, (+ve value, increasing exponent).
 *
 * The first byte is 0x80 for zero, 0xc0 for +ve with exponent 0.
 * -ve values are the 1's compliments (so 0x7f isn't used!).
 *
 * This only leaves 63 byte values for +ve exponents - which isn't enough.
 * The largest 4 exponent values are used to hold a byte count of the
 * number of following bytes that contain 8 exponent bits per byte,
 * This lets us sort exponents from -2^31 to +2^31.
 *
 * The mantissa is stored 2 digits per byte offset by 0x40, for negative
 * numbers the order must be reversed (they are bit inverted).
 *
 * Reverse sorts are done by inverting the sign of the number.
 */
#define MAX_EXP_ENC  ((int)sizeof(int))

static u_char *
number(u_char *pos, const u_char *bufend, u_char *line, u_char *lineend,
    int reverse)
{
	int exponent = -1;
	int had_dp = 0;
	u_char *tline;
	char ch;
	unsigned int val;
	u_char *last_nz_pos;
	u_char negate;

	if (reverse & R)
		negate = 0xff;
	else
		negate = 0;

	/* Give ourselves space for the key terminator */
	bufend--;

	/* Ensure we have enough space for the exponent */
	if (pos + 1 + MAX_EXP_ENC > bufend)
		return (NULL);

	SKIP_BLANKS(line);
	if (*line == '-') {	/* set the sign */
		negate ^= 0xff;
		line++;
	}
	/* eat initial zeroes */
	for (; *line == '0' && line < lineend; line++)
		continue;

	/* calculate exponents */
	if (*line == DECIMAL_POINT) {
		/* Decimal fraction */
		had_dp = 1;
		while (*++line == '0' && line < lineend)
			exponent--;
	} else {
		/* Large (absolute) value, count digits */
		for (tline = line; *tline >= '0' && 
		    *tline <= '9' && tline < lineend; tline++)
			exponent++;
	}

	/* If the first/next character isn't a digit, value is zero */
	if (*line < '1' || *line > '9' || line >= lineend) {
		/* This may be "0", "0.00", "000" or "fubar" but sorts as 0 */
		/* XXX what about NaN, NAN, inf and INF */
		*pos++ = 0x80;
		return pos;
	}

	/* Maybe here we should allow for e+12 (etc) */

	if (exponent < 0x40 - MAX_EXP_ENC && -exponent < 0x40 - MAX_EXP_ENC) {
		/* Value ok for simple encoding */
		/* exponent 0 is 0xc0 for +ve numbers and 0x40 for -ve ones */
		exponent += 0xc0;
		*pos++ = negate ^ exponent;
	} else {
		/* Out or range for a single byte */
		int c, t;
		t = exponent > 0 ? exponent : -exponent;
		/* Count how many 8-bit bytes are needed */
		for (c = 0; ; c++) {
			t >>= 8;
			if (t == 0)
				break;
		}
		/* 'c' better be 0..3 here - but probably 0..1 */
		/* Offset just outside valid range */
		t = c + 0x40 - MAX_EXP_ENC;
		if (exponent < 0)
			t = -t;
		*pos++ = negate ^ (t + 0xc0);
		/* now add each byte, most significant first */
		for (; c >= 0; c--)
			*pos++ = negate ^ (exponent >> (c * 8));
	}

	/* Finally add mantissa, 2 digits per byte */
	for (last_nz_pos = pos; line < lineend; ) {
		if (pos >= bufend)
			return NULL;
		ch = *line++;
		val = (ch - '0') * 10;
		if (val > 90) {
			if (ch == DECIMAL_POINT && !had_dp) {
				had_dp = 1;
				continue;
			}
			break;
		}
		while (line < lineend) {
			ch = *line++;
			if (ch == DECIMAL_POINT && !had_dp) {
				had_dp = 1;
				continue;
			}
			if (ch < '0' || ch > '9')
				line = lineend;
			else
				val += ch - '0';
			break;
		}
		*pos++ = negate ^ (val + 0x40);
		if (val != 0)
			last_nz_pos = pos;
	}

	/* Add key terminator, deleting any trailing "00" */
	*last_nz_pos++ = negate;

	return (last_nz_pos);
}

static u_char *
length(u_char *pos, const u_char *bufend, u_char *line, u_char *lineend,
    int flag)
{
	u_char buf[32];
	int l;
	SKIP_BLANKS(line);
	l = snprintf((char *)buf, sizeof(buf), "%td", lineend - line);
	return number(pos, bufend, buf, buf + l, flag);
}

#ifdef __minix
static u_char *
numhex(u_char *pos, const u_char *bufend, u_char *line, u_char *lineend,
    int flag)
{
	u_char buf[32];
	int64_t n = 0;
	int l;
	SKIP_BLANKS(line);
	sscanf((const char *) pos, "%lx", &n);
	l = snprintf((char *)buf, sizeof(buf), "%lld", n);
	return number(pos, bufend, buf, buf + l, flag);
}
#endif
