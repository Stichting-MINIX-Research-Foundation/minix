#ifndef lint
static char *rcsid = "$Id: ucs4.c,v 1.1.1.1 2003-06-04 00:26:14 marka Exp $";
#endif

/*
 * Copyright (c) 2001 Japan Network Information Center.  All rights reserved.
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
#include <stdlib.h>
#include <string.h>

#include <idn/assert.h>
#include <idn/result.h>
#include <idn/logmacro.h>
#include <idn/util.h>
#include <idn/ucs4.h>
#include <idn/debug.h>

/*
 * Unicode surrogate pair.
 */
#define IS_SURROGATE_HIGH(v)	(0xd800 <= (v) && (v) <= 0xdbff)
#define IS_SURROGATE_LOW(v)	(0xdc00 <= (v) && (v) <= 0xdfff)
#define SURROGATE_HIGH(v)	(SURROGATE_H_OFF + (((v) - 0x10000) >> 10))
#define SURROGATE_LOW(v)	(SURROGATE_L_OFF + ((v) & 0x3ff))
#define SURROGATE_BASE		0x10000
#define SURROGATE_H_OFF		0xd800
#define SURROGATE_L_OFF		0xdc00
#define COMBINE_SURROGATE(h, l) \
	(SURROGATE_BASE + (((h)-SURROGATE_H_OFF)<<10) + ((l)-SURROGATE_L_OFF))

/*
 * ASCII ctype macros.
 * Note that these macros evaluate the argument multiple times.  Be careful.
 */
#define ASCII_TOUPPER(c) \
	(('a' <= (c) && (c) <= 'z') ? ((c) - 'a' + 'A') : (c))
#define ASCII_TOLOWER(c) \
	(('A' <= (c) && (c) <= 'Z') ? ((c) - 'A' + 'a') : (c))

idn_result_t
idn_ucs4_ucs4toutf16(const unsigned long *ucs4, unsigned short *utf16,
		     size_t tolen) {
	unsigned short *utf16p = utf16;
	unsigned long v;
	idn_result_t r;

	TRACE(("idn_ucs4_ucs4toutf16(ucs4=\"%s\", tolen=%d)\n",
	       idn__debug_ucs4xstring(ucs4, 50), (int)tolen));

	while (*ucs4 != '\0') {
		v = *ucs4++;

		if (IS_SURROGATE_LOW(v) || IS_SURROGATE_HIGH(v)) {
			WARNING(("idn_ucs4_ucs4toutf16: UCS4 string contains "
				 "surrogate pair\n"));
			r = idn_invalid_encoding;
			goto ret;
		} else if (v > 0xffff) {
			/* Convert to surrogate pair */
			if (v >= 0x110000) {
				r = idn_invalid_encoding;
				goto ret;
			}
			if (tolen < 2) {
				r = idn_buffer_overflow;
				goto ret;
			}
			*utf16p++ = SURROGATE_HIGH(v);
			*utf16p++ = SURROGATE_LOW(v);
			tolen -= 2;
		} else {
			if (tolen < 1) {
				r = idn_buffer_overflow;
				goto ret;
			}
			*utf16p++ = v;
			tolen--;
		}
	}

	if (tolen < 1) {
		r = idn_buffer_overflow;
		goto ret;
	}
	*utf16p = '\0';

	r = idn_success;
ret:
	if (r == idn_success) {
		TRACE(("idn_ucs4_ucs4toutf16(): success (utf16=\"%s\")\n",
		       idn__debug_utf16xstring(utf16, 50)));
	} else {
		TRACE(("idn_ucs4_ucs4toutf16(): %s\n",
		       idn_result_tostring(r)));
	}
	return (r);
}

idn_result_t
idn_ucs4_utf16toucs4(const unsigned short *utf16, unsigned long *ucs4,
		     size_t tolen) {
	unsigned long *ucs4p = ucs4;
	unsigned short v0, v1;
	idn_result_t r;

	TRACE(("idn_ucs4_utf16toucs4(utf16=\"%s\", tolen=%d)\n",
	       idn__debug_utf16xstring(utf16, 50), (int)tolen));

	while (*utf16 != '\0') {
		v0 = *utf16;

		if (tolen < 1) {
			r = idn_buffer_overflow;
			goto ret;
		}

		if (IS_SURROGATE_HIGH(v0)) {
			v1 = *(utf16 + 1);
			if (!IS_SURROGATE_LOW(v1)) {
				WARNING(("idn_ucs4_utf16toucs4: "
					 "corrupted surrogate pair\n"));
				r = idn_invalid_encoding;
				goto ret;
			}
			*ucs4p++ = COMBINE_SURROGATE(v0, v1);
			tolen--;
			utf16 += 2;

		} else {
			*ucs4p++ = v0;
			tolen--;
			utf16++;
			
		}
	}

	if (tolen < 1) {
		r = idn_buffer_overflow;
		goto ret;
	}
	*ucs4p = '\0';

	r = idn_success;
ret:
	if (r == idn_success) {
		TRACE(("idn_ucs4_utf16toucs4(): success (ucs4=\"%s\")\n",
		       idn__debug_ucs4xstring(ucs4, 50)));
	} else {
		TRACE(("idn_ucs4_utf16toucs4(): %s\n",
		       idn_result_tostring(r)));
	}
	return (r);
}

idn_result_t
idn_ucs4_utf8toucs4(const char *utf8, unsigned long *ucs4, size_t tolen) {
	const unsigned char *utf8p = (const unsigned char *)utf8;
	unsigned long *ucs4p = ucs4;
	unsigned long v, min;
	unsigned char c;
	int width;
	int i;
	idn_result_t r;

	TRACE(("idn_ucs4_utf8toucs4(utf8=\"%s\", tolen=%d)\n",
	       idn__debug_xstring(utf8, 50), (int)tolen));

	while(*utf8p != '\0') {
		c = *utf8p++;
		if (c < 0x80) {
			v = c;
			min = 0;
			width = 1;
		} else if (c < 0xc0) {
			WARNING(("idn_ucs4_utf8toucs4: invalid character\n"));
			r = idn_invalid_encoding;
			goto ret;
		} else if (c < 0xe0) {
			v = c & 0x1f;
			min = 0x80;
			width = 2;
		} else if (c < 0xf0) {
			v = c & 0x0f;
			min = 0x800;
			width = 3;
		} else if (c < 0xf8) {
			v = c & 0x07;
			min = 0x10000;
			width = 4;
		} else if (c < 0xfc) {
			v = c & 0x03;
			min = 0x200000;
			width = 5;
		} else if (c < 0xfe) {
			v = c & 0x01;
			min = 0x4000000;
			width = 6;
		} else {
			WARNING(("idn_ucs4_utf8toucs4: invalid character\n"));
			r = idn_invalid_encoding;
			goto ret;
		}

		for (i = width - 1; i > 0; i--) {
			c = *utf8p++;
			if (c < 0x80 || 0xc0 <= c) {
				WARNING(("idn_ucs4_utf8toucs4: "
					 "invalid character\n"));
				r = idn_invalid_encoding;
				goto ret;
			}
			v = (v << 6) | (c & 0x3f);
		}

	        if (v < min) {
			WARNING(("idn_ucs4_utf8toucs4: invalid character\n"));
			r = idn_invalid_encoding;
			goto ret;
		}
		if (IS_SURROGATE_LOW(v) || IS_SURROGATE_HIGH(v)) {
			WARNING(("idn_ucs4_utf8toucs4: UTF-8 string contains "
				 "surrogate pair\n"));
			r = idn_invalid_encoding;
			goto ret;
		}
		if (tolen < 1) {
			r = idn_buffer_overflow;
			goto ret;
		}
		tolen--;
		*ucs4p++ = v;
	}

	if (tolen < 1) {
		r = idn_buffer_overflow;
		goto ret;
	}
	*ucs4p = '\0';

	r = idn_success;
ret:
	if (r == idn_success) {
		TRACE(("idn_ucs4_utf8toucs4(): success (ucs4=\"%s\")\n",
		       idn__debug_ucs4xstring(ucs4, 50)));
	} else {
		TRACE(("idn_ucs4_utf8toucs4(): %s\n",
		       idn_result_tostring(r)));
	}
	return (r);
}

idn_result_t
idn_ucs4_ucs4toutf8(const unsigned long *ucs4, char *utf8, size_t tolen) {
	unsigned char *utf8p = (unsigned char *)utf8;
	unsigned long v;
	int width;
	int mask;
	int offset;
	idn_result_t r;

	TRACE(("idn_ucs4_ucs4toutf8(ucs4=\"%s\", tolen=%d)\n",
	       idn__debug_ucs4xstring(ucs4, 50), (int)tolen));

	while (*ucs4 != '\0') {
		v = *ucs4++;
		if (IS_SURROGATE_LOW(v) || IS_SURROGATE_HIGH(v)) {
			WARNING(("idn_ucs4_ucs4toutf8: UCS4 string contains "
				 "surrogate pair\n"));
			r = idn_invalid_encoding;
			goto ret;
		}
		if (v < 0x80) {
			mask = 0;
			width = 1;
		} else if (v < 0x800) {
			mask = 0xc0;
			width = 2;
		} else if (v < 0x10000) {
			mask = 0xe0;
			width = 3;
		} else if (v < 0x200000) {
			mask = 0xf0;
			width = 4;
		} else if (v < 0x4000000) {
			mask = 0xf8;
			width = 5;
		} else if (v < 0x80000000) {
			mask = 0xfc;
			width = 6;
		} else {
			WARNING(("idn_ucs4_ucs4toutf8: invalid character\n"));
			r = idn_invalid_encoding;
			goto ret;
		}

		if (tolen < width) {
			r = idn_buffer_overflow;
			goto ret;
		}
		offset = 6 * (width - 1);
		*utf8p++ = (v >> offset) | mask;
		mask = 0x80;
		while (offset > 0) {
			offset -= 6;
			*utf8p++ = ((v >> offset) & 0x3f) | mask;
		}
		tolen -= width;
	}

	if (tolen < 1) {
		r = idn_buffer_overflow;
		goto ret;
	}
	*utf8p = '\0';

	r = idn_success;
ret:
	if (r == idn_success) {
		TRACE(("idn_ucs4_ucs4toutf8(): success (utf8=\"%s\")\n",
		       idn__debug_xstring(utf8, 50)));
	} else {
		TRACE(("idn_ucs4_ucs4toutf8(): %s\n",
		       idn_result_tostring(r)));
	}
	return (r);
}

size_t
idn_ucs4_strlen(const unsigned long *ucs4) {
	size_t len;

	for (len = 0; *ucs4 != '\0'; ucs4++, len++)
		/* nothing to do */ ;

	return (len);
}

unsigned long *
idn_ucs4_strcpy(unsigned long *to, const unsigned long *from) {
	unsigned long *result = to;

	while (*from != '\0')
		*to++ = *from++;
	*to = '\0';

	return (result);
}

unsigned long *
idn_ucs4_strcat(unsigned long *to, const unsigned long *from) {
	unsigned long *result = to;

	while (*to != '\0')
		to++;

	while (*from != '\0')
		*to++ = *from++;
	*to = '\0';

	return (result);
}

int
idn_ucs4_strcmp(const unsigned long *str1, const unsigned long *str2) {
	while (*str1 != '\0') {
		if (*str1 > *str2)
			return (1);
		else if (*str1 < *str2)
			return (-1);
		str1++;
		str2++;
	}

	if (*str1 > *str2)
		return (1);
	else if (*str1 < *str2)
		return (-1);

	return (0);
}

int
idn_ucs4_strcasecmp(const unsigned long *str1, const unsigned long *str2) {
	unsigned long c1, c2;

	while (*str1 != '\0') {
		c1 = ASCII_TOLOWER(*str1);
		c2 = ASCII_TOLOWER(*str2);
		if (c1 > c2)
			return (1);
		else if (c1 < c2)
			return (-1);
		str1++;
		str2++;
	}

	c1 = ASCII_TOLOWER(*str1);
	c2 = ASCII_TOLOWER(*str2);
	if (c1 > c2)
		return (1);
	else if (c1 < c2)
		return (-1);

	return (0);
}


unsigned long *
idn_ucs4_strdup(const unsigned long *str) {
	size_t length = idn_ucs4_strlen(str);
	unsigned long *dupstr;

	dupstr = (unsigned long *)malloc(sizeof(*str) * (length + 1));
	if (dupstr == NULL)
		return NULL;
	memcpy(dupstr, str, sizeof(*str) * (length + 1));

	return dupstr;
}
