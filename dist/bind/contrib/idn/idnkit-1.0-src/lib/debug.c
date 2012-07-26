#ifndef lint
static char *rcsid = "$Id: debug.c,v 1.1.1.1 2003-06-04 00:25:51 marka Exp $";
#endif

/*
 * Copyright (c) 2000,2002 Japan Network Information Center.
 * All rights reserved.
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

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include <idn/debug.h>

static char *hex = "0123456789abcdef";

#define STRING_MAXBYTES	200
#define STRING_NBUFS	4
static char bufs[STRING_NBUFS][STRING_MAXBYTES + 16];	/* +16 for margin */
static int bufno = 0;

char *
idn__debug_hexstring(const char *s, int maxbytes) {
	char *buf = bufs[bufno];
	char *p;
	int i;

	if (maxbytes > STRING_MAXBYTES)
		maxbytes = STRING_MAXBYTES;

	for (i = 0, p = buf; i < maxbytes; i += 3, s++) {
		int c = *(unsigned char *)s;

		if (c == '\0')
			break;
		*p++ = hex[c >> 4];
		*p++ = hex[c & 15];
		*p++ = ' ';
	}

	if (i >= maxbytes)
		strcpy(p, "...");
	else
		*p = '\0';

	bufno = (bufno + 1) % STRING_NBUFS;
	return (buf);
}

char *
idn__debug_xstring(const char *s, int maxbytes) {
	char *buf = bufs[bufno];
	char *p;
	int i;

	if (maxbytes > STRING_MAXBYTES)
		maxbytes = STRING_MAXBYTES;

	i = 0;
	p = buf;
	while (i < maxbytes) {
		int c = *(unsigned char *)s;

		if (c == '\0') {
			break;
		} else if (0x20 <= c && c <= 0x7e) {
			*p++ = c;
			i++;
		} else {
			*p++ = '\\';
			*p++ = 'x';
			*p++ = hex[c >> 4];
			*p++ = hex[c & 15];
			i += 4;
		}
		s++;
	}

	if (i >= maxbytes)
		strcpy(p, "...");
	else
		*p = '\0';

	bufno = (bufno + 1) % STRING_NBUFS;
	return (buf);
}

char *
idn__debug_ucs4xstring(const unsigned long *s, int maxbytes) {
	char *buf = bufs[bufno];
	char *p;
	int i;

	if (maxbytes > STRING_MAXBYTES)
		maxbytes = STRING_MAXBYTES;

	i = 0;
	p = buf;
	while (i < maxbytes) {
		if (*s == '\0') {
			break;
		} else if (0x20 <= *s && *s <= 0x7e) {
			*p++ = *s;
			i++;
		} else {
			*p++ = '\\';
			*p++ = 'x';
			i += 2;
			if (*s >= 0x1000000UL) {
				*p++ = hex[(*s >> 28) & 0x0f];
				*p++ = hex[(*s >> 24) & 0x0f];
				i += 2;
			}
			if (*s >= 0x10000UL) {
				*p++ = hex[(*s >> 20) & 0x0f];
				*p++ = hex[(*s >> 16) & 0x0f];
				i += 2;
		    	}
			if (*s >= 0x100UL) {
				*p++ = hex[(*s >> 12) & 0x0f];
				*p++ = hex[(*s >>  8) & 0x0f];
				i += 2;
			}
			*p++ = hex[(*s >> 4) & 0x0f];
			*p++ = hex[ *s       & 0x0f];
			i += 2;
		}
		s++;
	}

	if (i >= maxbytes)
		strcpy(p, "...");
	else
		*p = '\0';

	bufno = (bufno + 1) % STRING_NBUFS;
	return (buf);
}

char *
idn__debug_utf16xstring(const unsigned short *s, int maxbytes) {
	char *buf = bufs[bufno];
	char *p;
	int i;

	if (maxbytes > STRING_MAXBYTES)
		maxbytes = STRING_MAXBYTES;

	i = 0;
	p = buf;
	while (i < maxbytes) {
		if (*s == '\0') {
			break;
		} else if (0x20 <= *s && *s <= 0x7e) {
			*p++ = *s;
			i++;
		} else {
			*p++ = '\\';
			*p++ = 'x';
			*p++ = hex[(*s >> 12) & 0x0f];
			*p++ = hex[(*s >>  8) & 0x0f];
			*p++ = hex[(*s >> 4)  & 0x0f];
			*p++ = hex[ *s        & 0x0f];
			i += 6;
		}
		s++;
	}

	if (i >= maxbytes)
		strcpy(p, "...");
	else
		*p = '\0';

	bufno = (bufno + 1) % STRING_NBUFS;
	return (buf);
}

char *
idn__debug_hexdata(const char *s, int length, int maxbytes) {
	char *buf = bufs[bufno];
	char *p;
	int i;

	if (maxbytes > STRING_MAXBYTES)
		maxbytes = STRING_MAXBYTES;

	i = 0;
	p = buf;
	while (length > 0 && i < maxbytes) {
		int c = *(const unsigned char *)s;

		*p++ = hex[c >> 4];
		*p++ = hex[c & 15];
		*p++ = ' ';
		i += 3;
		length--;
		s++;
	}

	if (i >= maxbytes)
		strcpy(p, "...");
	else
		*p = '\0';

	bufno = (bufno + 1) % STRING_NBUFS;
	return (buf);
}

void
idn__debug_hexdump(const char *s, int length) {
	int i;
	const unsigned char *p = (const unsigned char *)s;

	i = 0;
	while (length-- > 0) {
		if (i % 16 == 0) {
			if (i > 0)
				fprintf(stderr, "\n");
			fprintf(stderr, "%4x:", i);
		}
		fprintf(stderr, " %02x", p[i]);
		i++;
	}
	fprintf(stderr, "\n");
}
