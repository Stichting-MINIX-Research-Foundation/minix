#ifndef lint
static char *rcsid = "$Id: race.c,v 1.1.1.1 2003-06-04 00:26:07 marka Exp $";
#endif

/*
 * Copyright (c) 2000,2001,2002 Japan Network Information Center.
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

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <idn/result.h>
#include <idn/assert.h>
#include <idn/logmacro.h>
#include <idn/converter.h>
#include <idn/ucs4.h>
#include <idn/debug.h>
#include <idn/race.h>
#include <idn/util.h>

#ifndef IDN_RACE_PREFIX
#define IDN_RACE_PREFIX		"bq--"
#endif
#define RACE_2OCTET_MODE	0xd8
#define RACE_ESCAPE		0xff
#define RACE_ESCAPE_2ND		0x99

#define RACE_BUF_SIZE		128		/* more than enough */

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
 * Compression type.
 */
enum {
	compress_one,	/* all characters are in a single row */
	compress_two,	/* row 0 and another row */
	compress_none	/* nope */
};

static idn_result_t	race_decode_decompress(const char *from,
					       unsigned short *buf,
					       size_t buflen);
static idn_result_t	race_compress_encode(const unsigned short *p,
					     int compress_mode,
					     char *to, size_t tolen);
static int		get_compress_mode(unsigned short *p);

idn_result_t
idn__race_decode(idn_converter_t ctx, void *privdata, 
		 const char *from, unsigned long *to, size_t tolen) {
	unsigned short *buf = NULL;
	size_t prefixlen = strlen(IDN_RACE_PREFIX);
	size_t fromlen;
	size_t buflen;
	idn_result_t r;

	assert(ctx != NULL);

	TRACE(("idn__race_decode(from=\"%s\", tolen=%d)\n",
	       idn__debug_xstring(from, 50), (int)tolen));

	if (!idn__util_asciihaveaceprefix(from, IDN_RACE_PREFIX)) {
		if (*from == '\0') {
			r = idn_ucs4_utf8toucs4(from, to, tolen);
			goto ret;
		}
		r = idn_invalid_encoding;
		goto ret;
	}
	from += prefixlen;
	fromlen = strlen(from);

	/*
	 * Allocate sufficient buffer.
	 */
	buflen = fromlen + 1;
	buf = malloc(sizeof(*buf) * buflen);
	if (buf == NULL) {
		r = idn_nomemory;
		goto ret;
	}

	/*
	 * Decode base32 and decompress.
	 */
	r = race_decode_decompress(from, buf, buflen);
	if (r != idn_success)
		goto ret;

	/*
	 * Now 'buf' points the decompressed string, which must contain
	 * UTF-16 characters.
	 */

	/*
	 * Convert to UCS4.
	 */
	r = idn_ucs4_utf16toucs4(buf, to, tolen);
	if (r != idn_success)
		goto ret;

ret:
	free(buf);
	if (r == idn_success) {
		TRACE(("idn__race_decode(): succcess (to=\"%s\")\n",
		       idn__debug_ucs4xstring(to, 50)));
	} else {
		TRACE(("idn__race_decode(): %s\n", idn_result_tostring(r)));
	}
	return (r);
}

static idn_result_t
race_decode_decompress(const char *from, unsigned short *buf, size_t buflen)
{
	unsigned short *p = buf;
	unsigned int bitbuf = 0;
	int bitlen = 0;
	int i, j;
	size_t len;

	while (*from != '\0') {
		int c = *from++;
		int x;

		if ('a' <= c && c <= 'z')
			x = c - 'a';
		else if ('A' <= c && c <= 'Z')
			x = c - 'A';
		else if ('2' <= c && c <= '7')
			x = c - '2' + 26;
		else
			return (idn_invalid_encoding);

		bitbuf = (bitbuf << 5) + x;
		bitlen += 5;
		if (bitlen >= 8) {
			*p++ = (bitbuf >> (bitlen - 8)) & 0xff;
			bitlen -= 8;
		}
	}
	len = p - buf;

	/*
	 * Now 'buf' holds the decoded string.
	 */

	/*
	 * Decompress.
	 */
	if (buf[0] == RACE_2OCTET_MODE) {
		if ((len - 1) % 2 != 0)
			return (idn_invalid_encoding);
		for (i = 1, j = 0; i < len; i += 2, j++)
			buf[j] = (buf[i] << 8) + buf[i + 1];
		len = j;
	} else {
		unsigned short c = buf[0] << 8;	/* higher octet */

		for (i = 1, j = 0; i < len; j++) {
			if (buf[i] == RACE_ESCAPE) {
				if (i + 1 >= len)
					return (idn_invalid_encoding);
				else if (buf[i + 1] == RACE_ESCAPE_2ND)
					buf[j] = c | 0xff;
				else
					buf[j] = buf[i + 1];
				i += 2;

			} else if (buf[i] == 0x99 && c == 0x00) {
				/*
				 * The RACE specification says this is error.
				 */
				return (idn_invalid_encoding);
				 
			} else {
				buf[j] = c | buf[i++];
			}
		}
		len = j;
	}
	buf[len] = '\0';

	return (idn_success);
}

idn_result_t
idn__race_encode(idn_converter_t ctx, void *privdata, 
		 const unsigned long *from, char *to, size_t tolen) {
	char *to_org = to;
	unsigned short *p, *buf = NULL;
	size_t prefixlen = strlen(IDN_RACE_PREFIX);
	size_t buflen;
	size_t fromlen;
	idn_result_t r;
	int compress_mode;

	assert(ctx != NULL);

	TRACE(("idn__race_encode(from=\"%s\", tolen=%d)\n",
	       idn__debug_ucs4xstring(from, 50), (int)tolen));

	if (*from == '\0') {
		r = idn_ucs4_ucs4toutf8(from, to, tolen);
		goto ret;
	} else if (idn__util_ucs4haveaceprefix(from, IDN_RACE_PREFIX)) {
		r = idn_prohibited;
		goto ret;
	}

	if (tolen < prefixlen) {
		r  = idn_buffer_overflow;
		goto ret;
	}
	memcpy(to, IDN_RACE_PREFIX, prefixlen);
	to += prefixlen;
	tolen -= prefixlen;

	fromlen = idn_ucs4_strlen(from);
	buflen = fromlen * 2 + 2;

	/*
	 * Convert to UTF-16.
	 * Preserve space for a character at the top of the buffer.
	 */
	for (;;) {
		unsigned short *new_buf;

		new_buf = realloc(buf, sizeof(*buf) * buflen);
		if (new_buf == NULL) {
			r = idn_nomemory;
			goto ret;
		}
		buf = new_buf;

		r = idn_ucs4_ucs4toutf16(from, buf + 1, buflen - 1);
		if (r == idn_success)
			break;
		else if (r != idn_buffer_overflow)
			goto ret;

		buflen = fromlen * 2 + 2;
	}
	p = buf + 1;

	/*
	 * Now 'p' contains UTF-16 encoded string.
	 */

	/*
	 * Check U+0099. 
	 * RACE doesn't permit U+0099 in an input string.
	 */
	for (p = buf + 1; *p != '\0'; p++) {
		if (*p == 0x0099) {
			r = idn_invalid_encoding;
			goto ret;
		}
	}

	/*
	 * Compress, encode in base-32 and output.
	 */
	compress_mode = get_compress_mode(buf + 1);
	r = race_compress_encode(buf, compress_mode, to, tolen);

ret:
	free(buf);
	if (r == idn_success) {
		TRACE(("idn__race_encode(): succcess (to=\"%s\")\n",
		       idn__debug_xstring(to_org, 50)));
	} else {
		TRACE(("idn__race_encode(): %s\n", idn_result_tostring(r)));
	}
	return (r);
}

static idn_result_t
race_compress_encode(const unsigned short *p, int compress_mode,
		     char *to, size_t tolen)
{
	unsigned long bitbuf = *p++;	/* bit stream buffer */
	int bitlen = 8;			/* # of bits in 'bitbuf' */

	while (*p != '\0' || bitlen > 0) {
		unsigned int c = *p;

		if (c == '\0') {
			/* End of data.  Flush. */
			bitbuf <<= (5 - bitlen);
			bitlen = 5;
		} else if (compress_mode == compress_none) {
			/* Push 16 bit data. */
			bitbuf = (bitbuf << 16) | c;
			bitlen += 16;
			p++;
		} else {/* compress_mode == compress_one/compress_two */
			/* Push 8 or 16 bit data. */
			if (compress_mode == compress_two &&
			    (c & 0xff00) == 0) {
				/* Upper octet is zero (and not U1). */
				bitbuf = (bitbuf << 16) | 0xff00 | c;
				bitlen += 16;
			} else if ((c & 0xff) == 0xff) {
				/* Lower octet is 0xff. */
				bitbuf = (bitbuf << 16) |
					(RACE_ESCAPE << 8) | RACE_ESCAPE_2ND;
				bitlen += 16;
			} else {
				/* Just output lower octet. */
				bitbuf = (bitbuf << 8) | (c & 0xff);
				bitlen += 8;
			}
			p++;
		}

		/*
		 * Output bits in 'bitbuf' in 5-bit unit.
		 */
		while (bitlen >= 5) {
			int x;

			/* Get top 5 bits. */
			x = (bitbuf >> (bitlen - 5)) & 0x1f;
			bitlen -= 5;

			/* Encode. */
			if (x < 26)
				x += 'a';
			else
				x = (x - 26) + '2';

			if (tolen < 1)
				return (idn_buffer_overflow);

			*to++ = x;
			tolen--;
		}
	}

	if (tolen <= 0)
		return (idn_buffer_overflow);

	*to = '\0';
	return (idn_success);
}

static int
get_compress_mode(unsigned short *p) {
	int zero = 0;
	unsigned int upper = 0;
	unsigned short *modepos = p - 1;

	while (*p != '\0') {
		unsigned int hi = *p++ & 0xff00;

		if (hi == 0) {
			zero++;
		} else if (hi == upper) {
			;
		} else if (upper == 0) {
			upper = hi;
		} else {
			*modepos = RACE_2OCTET_MODE;
			return (compress_none);
		}
	}
	*modepos = upper >> 8;
	if (upper > 0 && zero > 0)
		return (compress_two);
	else
		return (compress_one);
}
