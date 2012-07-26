#ifndef lint
static char *rcsid = "$Id: utf8.c,v 1.1.1.1 2003-06-04 00:26:44 marka Exp $";
#endif

/*
 * Copyright (c) 2000 Japan Network Information Center.  All rights reserved.
 *  
 * By using this file, you agree to the terms and conditions set forth bellow.
 * 
 * 			LICENSE TERMS AND CONDITIONS 
 * 
 * The following License Terms and Conditions apply, unless a different
 * license is obtained from Japan Network Information Center ("JPNIC"),
 * a Japanese association, Kokusai-Kougyou-Kanda Bldg 6F, 2-3-4 Uchi-Kanda,
 * Chiyoda-ku, Tokyo 101-0047, Japan.
 * 
 * 1. Use, Modification and Redistribution (including distribution of any
 *    modified or derived work) in source and/or binary forms is permitted
 *    under this License Terms and Conditions.
 * 
 * 2. Redistribution of source code must retain the copyright notices as they
 *    appear in each source code file, this License Terms and Conditions.
 * 
 * 3. Redistribution in binary form must reproduce the Copyright Notice,
 *    this License Terms and Conditions, in the documentation and/or other
 *    materials provided with the distribution.  For the purposes of binary
 *    distribution the "Copyright Notice" refers to the following language:
 *    "Copyright (c) 2000-2002 Japan Network Information Center.  All rights reserved."
 * 
 * 4. The name of JPNIC may not be used to endorse or promote products
 *    derived from this Software without specific prior written approval of
 *    JPNIC.
 * 
 * 5. Disclaimer/Limitation of Liability: THIS SOFTWARE IS PROVIDED BY JPNIC
 *    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 *    PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL JPNIC BE LIABLE
 *    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 *    BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 *    WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 *    OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 *    ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
 */

#include <config.h>

#include <stddef.h>

#include <idn/assert.h>
#include <idn/logmacro.h>
#include <idn/utf8.h>
#include <idn/debug.h>

#define UTF8_WIDTH(c) \
	(((c) < 0x80) ? 1 : \
	 ((c) < 0xc0) ? 0 : \
	 ((c) < 0xe0) ? 2 : \
	 ((c) < 0xf0) ? 3 : \
	 ((c) < 0xf8) ? 4 : \
	 ((c) < 0xfc) ? 5 : \
	 ((c) < 0xfe) ? 6 : 0)

#define VALID_CONT_BYTE(c)	(0x80 <= (c) && (c) < 0xc0)

int
idn_utf8_mblen(const char *s) {
	int c = *(unsigned char *)s;

	assert(s != NULL);

#if 0
	TRACE(("idn_utf8_mblen(s=<%s>)\n", idn__debug_hexstring(s, 6)));
#endif

	return UTF8_WIDTH(c);
}

int
idn_utf8_getmb(const char *s, size_t len, char *buf) {
	/* buf must be at least 7-bytes long */
	const unsigned char *p = (const unsigned char *)s;
	unsigned char *q = (unsigned char *)buf;
	int width = UTF8_WIDTH(*p);
	int w;

	assert(s != NULL);

#if 0
	TRACE(("idn_utf8_getmb(s=<%s>,len=%d)\n",
	      idn__debug_hexstring(s, 6), len));
#endif

	if (width == 0 || len < width)
		return (0);

	/* Copy the first byte. */
	*q++ = *p++;

	/* .. and the rest. */
	w = width;
	while (--w > 0) {
		if (!VALID_CONT_BYTE(*p))
			return (0);
		*q++ = *p++;
	}
	return (width);
}

int
idn_utf8_getwc(const char *s, size_t len, unsigned long *vp) {
	unsigned long v;
	unsigned long min;
	const unsigned char *p = (const unsigned char *)s;
	int c;
	int width;
	int rest;

	assert(s != NULL);

#if 0
	TRACE(("idn_utf8_getwc(s=<%s>,len=%d)\n",
	      idn__debug_hexstring(s, 10), len));
#endif

	c = *p++;
	width = UTF8_WIDTH(c);

	switch (width) {
	case 0:
		return (0);
	case 1:
		v = c;
		min = 0;
		break;
	case 2:
		v = c & 0x1f;
		min = 0x80;
		break;
	case 3:
		v = c & 0xf;
		min = 0x800;
		break;
	case 4:
		v = c & 0x7;
		min = 0x10000;
		break;
	case 5:
		v = c & 3;
		min = 0x200000;
		break;
	case 6:
		v = c & 1;
		min = 0x4000000;
		break;
	default:
		FATAL(("idn_utf8_getint: internal error\n"));
		return (0);
	}

	if (len < width)
		return (0);
		
	rest = width - 1;
	while (rest-- > 0) {
		if (!VALID_CONT_BYTE(*p))
			return (0);
		v = (v << 6) | (*p & 0x3f);
		p++;
	}

	if (v < min)
		return (0);

	*vp = v;
	return (width);
}

int
idn_utf8_putwc(char *s, size_t len, unsigned long v) {
	unsigned char *p = (unsigned char *)s;
	int mask;
	int off;
	int l;

	assert(s != NULL);

#if 0
	TRACE(("idn_utf8_putwc(v=%lx)\n", v));
#endif

	if (v < 0x80) {
		mask = 0;
		l = 1;
	} else if (v < 0x800) {
		mask = 0xc0;
		l = 2;
	} else if (v < 0x10000) {
		mask = 0xe0;
		l = 3;
	} else if (v < 0x200000) {
		mask = 0xf0;
		l = 4;
	} else if (v < 0x4000000) {
		mask = 0xf8;
		l = 5;
	} else if (v < 0x80000000) {
		mask = 0xfc;
		l = 6;
	} else {
		return (0);
	}

	if (len < l)
		return (0);

	off = 6 * (l - 1);
	*p++ = (v >> off) | mask;
	mask = 0x80;
	while (off > 0) {
		off -= 6;
		*p++ = ((v >> off) & 0x3f) | mask;
	}
	return l;
}

int
idn_utf8_isvalidchar(const char *s) {
	unsigned long dummy;

	TRACE(("idn_utf8_isvalidchar(s=<%s>)\n",
	      idn__debug_hexstring(s, 6)));

	return (idn_utf8_getwc(s, 6, &dummy) > 0);
}

int
idn_utf8_isvalidstring(const char *s) {
	unsigned long dummy;
	int width;

	assert(s != NULL);

	TRACE(("idn_utf8_isvalidstring(s=<%s>)\n",
	      idn__debug_hexstring(s, 20)));

	while (*s != '\0') {
		width = idn_utf8_getwc(s, 6, &dummy);
		if (width == 0)
			return (0);
		s += width;
	}
	return (1);
}

char *
idn_utf8_findfirstbyte(const char *s, const char *known_top) {
	const unsigned char *p = (const unsigned char *)s;
	const unsigned char *t = (const unsigned char *)known_top;

	assert(s != NULL && known_top != NULL && known_top <= s);

	TRACE(("idn_utf8_findfirstbyte(s=<%s>)\n",
	      idn__debug_hexstring(s, 8)));

	while (p >= t) {
		if (!VALID_CONT_BYTE(*p))
		    break;
		p--;
	}
	if (p < t || UTF8_WIDTH(*p) == 0)
		return (NULL);

	return ((char *)p);
}
