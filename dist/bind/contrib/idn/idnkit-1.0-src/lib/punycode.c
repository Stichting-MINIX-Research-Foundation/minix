#ifndef lint
static char *rcsid = "$Id: punycode.c,v 1.1.1.1 2003-06-04 00:26:06 marka Exp $";
#endif

/*
 * Copyright (c) 2001,2002 Japan Network Information Center.
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
#include <idn/punycode.h>
#include <idn/util.h>

/*
 * Although draft-ietf-idn-punycode-00.txt doesn't specify the ACE
 * signature, we have to choose one.  In order to prevent the converted
 * name from beginning with a hyphen, we should choose a prefix rather
 * than a suffix.
 */
#ifndef IDN_PUNYCODE_PREFIX
#define IDN_PUNYCODE_PREFIX	"xn--"
#endif

#define INVALID_UCS	0x80000000
#define MAX_UCS		0x10FFFF

/*
 * As the draft states, it is possible that `delta' may overflow during
 * the encoding.  The upper bound of 'delta' is:
 *   <# of chars. of input string> + <max. difference in code point> *
 *   <# of chars. of input string + 1>
 * For this value not to be greater than 0xffffffff (since the calculation
 * is done using unsigned long, which is at least 32bit long), the maxmum
 * input string size is about 3850 characters, which is long enough for
 * a domain label...
 */
#define PUNYCODE_MAXINPUT	3800

/*
 * Parameters.
 */
#define PUNYCODE_BASE		36
#define PUNYCODE_TMIN		1
#define PUNYCODE_TMAX		26
#define PUNYCODE_SKEW		38
#define PUNYCODE_DAMP		700
#define PUNYCODE_INITIAL_BIAS	72
#define PUNYCODE_INITIAL_N	0x80

static int		punycode_getwc(const char *s, size_t len,
				      int bias, unsigned long *vp);
static int		punycode_putwc(char *s, size_t len,
				      unsigned long delta, int bias);
static int		punycode_update_bias(unsigned long delta,
					    size_t npoints, int first);

idn_result_t
idn__punycode_decode(idn_converter_t ctx, void *privdata,
		    const char *from, unsigned long *to, size_t tolen) {
	unsigned long *to_org = to;
	unsigned long c, idx;
	size_t prefixlen = strlen(IDN_PUNYCODE_PREFIX);
	size_t fromlen;
	size_t uidx, fidx, ucslen;
	int first, bias;
	idn_result_t r;

	assert(ctx != NULL);

	TRACE(("idn__punycode_decode(from=\"%s\", tolen=%d)\n",
	       idn__debug_xstring(from, 50), (int)tolen));

	if (!idn__util_asciihaveaceprefix(from, IDN_PUNYCODE_PREFIX)) {
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
	 * Find the last delimiter, and copy the characters
	 * before it verbatim.
	 */
	ucslen = 0;
	for (fidx = fromlen; fidx > 0; fidx--) {
		if (from[fidx - 1] == '-') {
			if (tolen < fidx) {
				r = idn_buffer_overflow;
				goto ret;
			}
			for (uidx = 0; uidx < fidx - 1; uidx++) {
				to[uidx] = from[uidx];
			}
			ucslen = uidx;
			break;
		}
	}

	first = 1;
	bias = PUNYCODE_INITIAL_BIAS;
	c = PUNYCODE_INITIAL_N;
	idx = 0;
	while (fidx < fromlen) {
		int len;
		unsigned long delta;
		int i;

		len = punycode_getwc(from + fidx, fromlen - fidx, bias, &delta);
		if (len == 0) {
			r = idn_invalid_encoding;
			goto ret;
		}
		fidx += len;

		bias = punycode_update_bias(delta, ucslen + 1, first);
		first = 0;
		idx += delta;
		c += idx / (ucslen + 1);
		uidx = idx % (ucslen + 1);

		/* Insert 'c' at uidx. */
		if (tolen-- <= 0) {
			r = idn_buffer_overflow;
			goto ret;
		}
		for (i = ucslen; i > uidx; i--)
			to[i] = to[i - 1];
		to[uidx] = c;

		ucslen++;
		idx = uidx + 1;
	}

	/* Terminate with NUL. */
	if (tolen <= 0) {
		r = idn_buffer_overflow;
		goto ret;
	}
	to[ucslen] = '\0';
	r = idn_success;

ret:
	if (r == idn_success) {
		TRACE(("idn__punycode_decode(): succcess (to=\"%s\")\n",
		       idn__debug_ucs4xstring(to_org, 50)));
	} else {
		TRACE(("idn__punycode_decode(): %s\n", idn_result_tostring(r)));
	}
	return (r);
}

idn_result_t
idn__punycode_encode(idn_converter_t ctx, void *privdata,
		    const unsigned long *from, char *to, size_t tolen) {
	char *to_org = to;
	unsigned long cur_code, next_code, delta;
	size_t prefixlen = strlen(IDN_PUNYCODE_PREFIX);
	size_t fromlen;
	size_t ucsdone;
	size_t toidx;
	int uidx, bias, first;
	idn_result_t r;

	assert(ctx != NULL);

	TRACE(("idn__punycode_encode(from=\"%s\", tolen=%d)\n",
	       idn__debug_ucs4xstring(from, 50), (int)tolen));

	if (*from == '\0') {
		r = idn_ucs4_ucs4toutf8(from, to, tolen);
		goto ret;
	} else if (idn__util_ucs4haveaceprefix(from, IDN_PUNYCODE_PREFIX)) {
		r = idn_prohibited;
		goto ret;
	}

	if (tolen < prefixlen) {
		r = idn_buffer_overflow;
		goto ret;
	}
	memcpy(to, IDN_PUNYCODE_PREFIX, prefixlen);
	to += prefixlen;
	tolen -= prefixlen;

	fromlen = idn_ucs4_strlen(from);

	/*
	 * If the input string is too long (actually too long to be sane),
	 * return failure in order to prevent possible overflow.
	 */
	if (fromlen > PUNYCODE_MAXINPUT) {
		ERROR(("idn__punycode_encode(): "
		       "the input string is too long to convert Punycode\n",
		       idn__debug_ucs4xstring(from, 50)));
		r = idn_failure;
		goto ret;
	}

	ucsdone = 0;	/* number of characters processed */
	toidx = 0;

	/*
	 * First, pick up basic code points and copy them to 'to'.
	 */
	for (uidx = 0; uidx < fromlen; uidx++) {
		if (from[uidx] < 0x80) {
			if (toidx >= tolen) {
				r = idn_buffer_overflow;
				goto ret;
			}
			to[toidx++] = from[uidx];
			ucsdone++;
		}
	}

	/*
	 * If there are any basic code points, output a delimiter
	 * (hyphen-minus).
	 */
	if (toidx > 0) {
		if (toidx >= tolen) {
			r = idn_buffer_overflow;
			goto ret;
		}
		to[toidx++] = '-';
		to += toidx;
		tolen -= toidx;
	}

	/*
	 * Then encode non-basic characters.
	 */
	first = 1;
	cur_code = PUNYCODE_INITIAL_N;
	bias = PUNYCODE_INITIAL_BIAS;
	delta = 0;
	while (ucsdone < fromlen) {
		int limit = -1, rest;

		/*
		 * Find the smallest code point equal to or greater
		 * than 'cur_code'.  Also remember the index of the
		 * last occurence of the code point.
		 */
		for (next_code = MAX_UCS, uidx = fromlen - 1;
		     uidx >= 0; uidx--) {
			if (from[uidx] >= cur_code && from[uidx] < next_code) {
				next_code = from[uidx];
				limit = uidx;
			}
		}
		/* There must be such code point. */
		assert(limit >= 0);

		delta += (next_code - cur_code) * (ucsdone + 1);
		cur_code = next_code;

		/*
		 * Scan the input string again, and encode characters
		 * whose code point is 'cur_code'.  Use 'limit' to avoid
		 * unnecessary scan.
		 */
		for (uidx = 0, rest = ucsdone; uidx <= limit; uidx++) {
			if (from[uidx] < cur_code) {
				delta++;
				rest--;
			} else if (from[uidx] == cur_code) {
				int sz = punycode_putwc(to, tolen, delta, bias);
				if (sz == 0) {
					r = idn_buffer_overflow;
					goto ret;
				}
				to += sz;
				tolen -= sz;
				ucsdone++;
				bias = punycode_update_bias(delta, ucsdone,
							   first);
				delta = 0;
				first = 0;
			}
		}
		delta += rest + 1;
		cur_code++;
	}

	/*
	 * Terminate with NUL.
	 */
	if (tolen <= 0) {
		r = idn_buffer_overflow;
		goto ret;
	}
	*to = '\0';
	r = idn_success;

ret:
	if (r == idn_success) {
		TRACE(("idn__punycode_encode(): succcess (to=\"%s\")\n",
		       idn__debug_xstring(to_org, 50)));
	} else {
		TRACE(("idn__punycode_encode(): %s\n", idn_result_tostring(r)));
	}
	return (r);
}

static int
punycode_getwc(const char *s, size_t len, int bias, unsigned long *vp) {
	size_t orglen = len;
	unsigned long v = 0, w = 1;
	int k;

	for (k = PUNYCODE_BASE - bias; len > 0; k += PUNYCODE_BASE) {
		int c = *s++;
		int t = (k < PUNYCODE_TMIN) ? PUNYCODE_TMIN :
			(k > PUNYCODE_TMAX) ? PUNYCODE_TMAX : k;

		len--;
		if ('a' <= c && c <= 'z')
			c = c - 'a';
		else if ('A' <= c && c <= 'Z')
			c = c - 'A';
		else if ('0' <= c && c <= '9')
			c = c - '0' + 26;
		else
			c = -1;

		if (c < 0)
			return (0);	/* invalid character */

		v += c * w;

		if (c < t) {
			*vp = v;
			return (orglen - len);
		}

		w *= (PUNYCODE_BASE - t);
	}

	return (0);	/* final character missing */
}

static int
punycode_putwc(char *s, size_t len, unsigned long delta, int bias) {
	const char *punycode_base36 = "abcdefghijklmnopqrstuvwxyz0123456789";
	int k;
	char *sorg = s;

	for (k = PUNYCODE_BASE - bias; 1; k += PUNYCODE_BASE) {
		int t = (k < PUNYCODE_TMIN) ? PUNYCODE_TMIN :
			(k > PUNYCODE_TMAX) ? PUNYCODE_TMAX : k;

		if (delta < t)
			break;
		if (len < 1)
			return (0);
		*s++ = punycode_base36[t + ((delta - t) % (PUNYCODE_BASE - t))];
		len--;
		delta = (delta - t) / (PUNYCODE_BASE - t);
	}
	if (len < 1)
		return (0);
	*s++ = punycode_base36[delta];
	return (s - sorg);
}

static int
punycode_update_bias(unsigned long delta, size_t npoints, int first) {
	int k = 0;

	delta /= first ? PUNYCODE_DAMP : 2;
	delta += delta / npoints;

	while (delta > ((PUNYCODE_BASE - PUNYCODE_TMIN) * PUNYCODE_TMAX) / 2) {
		delta /= PUNYCODE_BASE - PUNYCODE_TMIN;
		k++;
	}
	return (PUNYCODE_BASE * k +
		(((PUNYCODE_BASE - PUNYCODE_TMIN + 1) * delta) /
		 (delta + PUNYCODE_SKEW)));
}
