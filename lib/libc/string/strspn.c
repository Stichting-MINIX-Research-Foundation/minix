/*	$NetBSD: strspn.c,v 1.17 2009/07/30 21:42:06 dsl Exp $	*/

/*-
 * Copyright (c) 2008 Joerg Sonnenberger
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__RCSID("$NetBSD: strspn.c,v 1.17 2009/07/30 21:42:06 dsl Exp $");

#include <assert.h>
#include <inttypes.h>
#include <limits.h>
#include <string.h>

#if ULONG_MAX != 0xffffffffffffffffull

size_t
strspn(const char *s, const char *charset)
{
	static const size_t idx[8] = { 1, 2, 4, 8, 16, 32, 64, 128 };
	uint8_t set[32];
	const char *t;
#define UC(a) ((unsigned int)(unsigned char)(a))

	_DIAGASSERT(s != NULL);
	_DIAGASSERT(charset != NULL);

	if (charset[0] == '\0')
		return 0;
	if (charset[1] == '\0') {
		for (t = s; *t != '\0'; ++t) {
			if (*t != *charset)
				break;
		}
		return t - s;
	}

	(void)memset(set, 0, sizeof(set));

	for (; *charset != '\0'; ++charset)
		set[UC(*charset) >> 3] |= idx[UC(*charset) & 7];

	for (t = s; *t != '\0'; ++t)
		if ((set[UC(*t) >> 3] & idx[UC(*t) & 7]) == 0)
			break;
	return t - s;
}

#else

/* 64 bit system, use four 64 bits registers for bitmask */

static size_t
strspn_x(const char *s_s, const char *charset_s, unsigned long invert)
{
	const unsigned char *s = (const unsigned char *)s_s;
	const unsigned char *charset = (const unsigned char *)charset_s;
	unsigned long m_0, m_4, m_8, m_c;
	unsigned char ch, next_ch;
	unsigned long bit;
	unsigned long check;
	size_t count;

	/* Four 64bit registers have one bit for each character value */
	m_0 = 0;
	m_4 = 0;
	m_8 = 0;
	m_c = 0;

	for (ch = *charset; ch != 0; ch = next_ch) {
		next_ch = *++charset;
		bit = 1ul << (ch & 0x3f);
		if (__predict_true(ch < 0x80)) {
			if (ch < 0x40)
				m_0 |= bit;
			else
				m_4 |= bit;
		} else {
			if (ch < 0xc0)
				m_8 |= bit;
			else
				m_c |= bit;
		}
	}

	/* For strcspn() we just invert the validity set */
	m_0 ^= invert;
	m_4 ^= invert;
	m_8 ^= invert;
	m_c ^= invert;

	/*
	 * We could do remove the lsb from m_0 to terminate at the
	 * end of the input string.
	 * However prefetching the next char is benifitial and we must
	 * not read the byte after the \0 - as it might fault!
	 * So we take the 'hit' of the compare against 0.
	 */

	ch = *s++;
	for (count = 0; ch != 0; ch = next_ch) {
		next_ch = s[count];
		if (__predict_true(ch < 0x80)) {
			check = m_0;
			if (ch >= 0x40)
				check = m_4;
		} else {
			check = m_8;
			if (ch >= 0xc0)
				check = m_c;
		}
		if (!((check >> (ch & 0x3f)) & 1))
			break;
		count++;
	}
	return count;
}

size_t
strspn(const char *s, const char *charset)
{
	return strspn_x(s, charset, 0);
}

size_t
strcspn(const char *s, const char *charset)
{
	return strspn_x(s, charset, ~0ul);
}
#endif
