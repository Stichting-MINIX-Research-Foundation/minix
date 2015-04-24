/* $NetBSD: unicode.h,v 1.1 2013/08/05 14:11:30 reinoud Exp $ */

/*-
 * Copyright (c) 2001, 2004 The NetBSD Foundation, Inc.
 * All rights reserved.
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
 * Paul Borman at Krystal Technologies.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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

/*
 * Routines for handling Unicode encoded in UTF-8 form, code derived from
 * src/lib/libc/locale/utf2.c.
 */
static u_int16_t wget_utf8(const char **, size_t *) __unused;
static int wput_utf8(char *, size_t, u_int16_t) __unused;

/*
 * Read one UTF8-encoded character off the string, shift the string pointer
 * and return the character.
 */
static u_int16_t
wget_utf8(const char **str, size_t *sz)
{
	unsigned int c;
	u_int16_t rune = 0;
	const char *s = *str;
	static const int _utf_count[16] = {
		1, 1, 1, 1, 1, 1, 1, 1,
		0, 0, 0, 0, 2, 2, 3, 0,
	};

	/* must be called with at least one byte remaining */
	assert(*sz > 0);

	c = _utf_count[(s[0] & 0xf0) >> 4];
	if (c == 0 || c > *sz) {
    decoding_error:
		/*
		 * The first character is in range 128-255 and doesn't
		 * mark valid a valid UTF-8 sequence. There is not much
		 * we can do with this, so handle by returning
		 * the first character as if it would be a correctly
		 * encoded ISO-8859-1 character.
		 */
		c = 1;
	}

	switch (c) {
	case 1:
		rune = s[0] & 0xff;
		break;
	case 2:
		if ((s[1] & 0xc0) != 0x80)
			goto decoding_error;
		rune = ((s[0] & 0x1F) << 6) | (s[1] & 0x3F);
		break;
	case 3:
		if ((s[1] & 0xC0) != 0x80 || (s[2] & 0xC0) != 0x80)
			goto decoding_error;
		rune = ((s[0] & 0x0F) << 12) | ((s[1] & 0x3F) << 6)
		    | (s[2] & 0x3F);
		break;
	}

	*str += c;
	*sz -= c;
	return rune;
}

/*
 * Encode wide character and write it to the string. 'n' specifies
 * how much buffer space remains in 's'. Returns number of bytes written
 * to the target string 's'.
 */
static int
wput_utf8(char *s, size_t n, u_int16_t wc)
{
	if (wc & 0xf800) {
		if (n < 3) {
			/* bound check failure */
			return 0;
		}

		s[0] = 0xE0 | (wc >> 12);
		s[1] = 0x80 | ((wc >> 6) & 0x3F);
		s[2] = 0x80 | ((wc) & 0x3F);
		return 3;
	} else if (wc & 0x0780) {
		if (n < 2) {
			/* bound check failure */
			return 0;
		}

		s[0] = 0xC0 | (wc >> 6);
		s[1] = 0x80 | ((wc) & 0x3F);
		return 2;
	} else {
		if (n < 1) {
			/* bound check failure */
			return 0;
		}

		s[0] = wc;
		return 1;
	}
}
