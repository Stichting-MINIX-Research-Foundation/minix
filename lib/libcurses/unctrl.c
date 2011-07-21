/*	$NetBSD: unctrl.c,v 1.11 2007/05/28 15:01:58 blymn Exp $	*/

/*
 * Copyright (c) 1981, 1993
 *	The Regents of the University of California.  All rights reserved.
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

#include <sys/cdefs.h>
#ifndef lint
#if 0
static char sccsid[] = "@(#)unctrl.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: unctrl.c,v 1.11 2007/05/28 15:01:58 blymn Exp $");
#endif
#endif /* not lint */

#include <unctrl.h>

const char   * const __unctrl[256] = {
	"^@", "^A", "^B", "^C", "^D", "^E", "^F", "^G",
	"^H", "^I", "^J", "^K", "^L", "^M", "^N", "^O",
	"^P", "^Q", "^R", "^S", "^T", "^U", "^V", "^W",
	"^X", "^Y", "^Z", "^[", "^\\", "^]", "^~", "^_",
	" ", "!", "\"", "#", "$", "%", "&", "'",
	"(", ")", "*", "+", ",", "-", ".", "/",
	"0", "1", "2", "3", "4", "5", "6", "7",
	"8", "9", ":", ";", "<", "=", ">", "?",
	"@", "A", "B", "C", "D", "E", "F", "G",
	"H", "I", "J", "K", "L", "M", "N", "O",
	"P", "Q", "R", "S", "T", "U", "V", "W",
	"X", "Y", "Z", "[", "\\", "]", "^", "_",
	"`", "a", "b", "c", "d", "e", "f", "g",
	"h", "i", "j", "k", "l", "m", "n", "o",
	"p", "q", "r", "s", "t", "u", "v", "w",
	"x", "y", "z", "{", "|", "}", "~", "^?",

	"0x80", "0x81", "0x82", "0x83", "0x84", "0x85", "0x86", "0x87",
	"0x88", "0x89", "0x8a", "0x8b", "0x8c", "0x8d", "0x8e", "0x8f",
	"0x90", "0x91", "0x92", "0x93", "0x94", "0x95", "0x96", "0x97",
	"0x98", "0x99", "0x9a", "0x9b", "0x9c", "0x9d", "0x9e", "0x9f",
	"0xa0", "0xa1", "0xa2", "0xa3", "0xa4", "0xa5", "0xa6", "0xa7",
	"0xa8", "0xa9", "0xaa", "0xab", "0xac", "0xad", "0xae", "0xaf",
	"0xb0", "0xb1", "0xb2", "0xb3", "0xb4", "0xb5", "0xb6", "0xb7",
	"0xb8", "0xb9", "0xba", "0xbb", "0xbc", "0xbd", "0xbe", "0xbf",
	"0xc0", "0xc1", "0xc2", "0xc3", "0xc4", "0xc5", "0xc6", "0xc7",
	"0xc8", "0xc9", "0xca", "0xcb", "0xcc", "0xcd", "0xce", "0xcf",
	"0xd0", "0xd1", "0xd2", "0xd3", "0xd4", "0xd5", "0xd6", "0xd7",
	"0xd8", "0xd9", "0xda", "0xdb", "0xdc", "0xdd", "0xde", "0xdf",
	"0xe0", "0xe1", "0xe2", "0xe3", "0xe4", "0xe5", "0xe6", "0xe7",
	"0xe8", "0xe9", "0xea", "0xeb", "0xec", "0xed", "0xee", "0xef",
	"0xf0", "0xf1", "0xf2", "0xf3", "0xf4", "0xf5", "0xf6", "0xf7",
	"0xf8", "0xf9", "0xfa", "0xfb", "0xfc", "0xfd", "0xfe", "0xff",
};

const unsigned char    __unctrllen[256] = {
	2, 2, 2, 2, 2, 2, 2, 2,
	2, 2, 2, 2, 2, 2, 2, 2,
	2, 2, 2, 2, 2, 2, 2, 2,
	2, 2, 2, 2, 2, 2, 2, 2,
	1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 2,
	4, 4, 4, 4, 4, 4, 4, 4,
	4, 4, 4, 4, 4, 4, 4, 4,
	4, 4, 4, 4, 4, 4, 4, 4,
	4, 4, 4, 4, 4, 4, 4, 4,
	4, 4, 4, 4, 4, 4, 4, 4,
	4, 4, 4, 4, 4, 4, 4, 4,
	4, 4, 4, 4, 4, 4, 4, 4,
	4, 4, 4, 4, 4, 4, 4, 4,
	4, 4, 4, 4, 4, 4, 4, 4,
	4, 4, 4, 4, 4, 4, 4, 4,
	4, 4, 4, 4, 4, 4, 4, 4,
	4, 4, 4, 4, 4, 4, 4, 4,
	4, 4, 4, 4, 4, 4, 4, 4,
	4, 4, 4, 4, 4, 4, 4, 4,
	4, 4, 4, 4, 4, 4, 4, 4,
	4, 4, 4, 4, 4, 4, 4, 4,
};

#ifdef HAVE_WCHAR
const wchar_t   * const __wunctrl[256] = {
	L"^@", L"^A", L"^B", L"^C", L"^D", L"^E", L"^F", L"^G",
	L"^H", L"^I", L"^J", L"^K", L"^L", L"^M", L"^N", L"^O",
	L"^P", L"^Q", L"^R", L"^S", L"^T", L"^U", L"^V", L"^W",
	L"^X", L"^Y", L"^Z", L"^[", L"^\\", L"^]", L"^~", L"^_",
	L" ", L"!", L"\"", L"#", L"$", L"%", L"&", L"'",
	L"(", L")", L"*", L"+", L",", L"-", L".", L"/",
	L"0", L"1", L"2", L"3", L"4", L"5", L"6", L"7",
	L"8", L"9", L":", L";", L"<", L"=", L">", L"?",
	L"@", L"A", L"B", L"C", L"D", L"E", L"F", L"G",
	L"H", L"I", L"J", L"K", L"L", L"M", L"N", L"O",
	L"P", L"Q", L"R", L"S", L"T", L"U", L"V", L"W",
	L"X", L"Y", L"Z", L"[", L"\\", L"]", L"^", L"_",
	L"`", L"a", L"b", L"c", L"d", L"e", L"f", L"g",
	L"h", L"i", L"j", L"k", L"l", L"m", L"n", L"o",
	L"p", L"q", L"r", L"s", L"t", L"u", L"v", L"w",
	L"x", L"y", L"z", L"{", L"|", L"}", L"~", L"^?",

	L"0x80", L"0x81", L"0x82", L"0x83", L"0x84", L"0x85", L"0x86", L"0x87",
	L"0x88", L"0x89", L"0x8a", L"0x8b", L"0x8c", L"0x8d", L"0x8e", L"0x8f",
	L"0x90", L"0x91", L"0x92", L"0x93", L"0x94", L"0x95", L"0x96", L"0x97",
	L"0x98", L"0x99", L"0x9a", L"0x9b", L"0x9c", L"0x9d", L"0x9e", L"0x9f",
	L"0xa0", L"0xa1", L"0xa2", L"0xa3", L"0xa4", L"0xa5", L"0xa6", L"0xa7",
	L"0xa8", L"0xa9", L"0xaa", L"0xab", L"0xac", L"0xad", L"0xae", L"0xaf",
	L"0xb0", L"0xb1", L"0xb2", L"0xb3", L"0xb4", L"0xb5", L"0xb6", L"0xb7",
	L"0xb8", L"0xb9", L"0xba", L"0xbb", L"0xbc", L"0xbd", L"0xbe", L"0xbf",
	L"0xc0", L"0xc1", L"0xc2", L"0xc3", L"0xc4", L"0xc5", L"0xc6", L"0xc7",
	L"0xc8", L"0xc9", L"0xca", L"0xcb", L"0xcc", L"0xcd", L"0xce", L"0xcf",
	L"0xd0", L"0xd1", L"0xd2", L"0xd3", L"0xd4", L"0xd5", L"0xd6", L"0xd7",
	L"0xd8", L"0xd9", L"0xda", L"0xdb", L"0xdc", L"0xdd", L"0xde", L"0xdf",
	L"0xe0", L"0xe1", L"0xe2", L"0xe3", L"0xe4", L"0xe5", L"0xe6", L"0xe7",
	L"0xe8", L"0xe9", L"0xea", L"0xeb", L"0xec", L"0xed", L"0xee", L"0xef",
	L"0xf0", L"0xf1", L"0xf2", L"0xf3", L"0xf4", L"0xf5", L"0xf6", L"0xf7",
	L"0xf8", L"0xf9", L"0xfa", L"0xfb", L"0xfc", L"0xfd", L"0xfe", L"0xff",
};
#endif /* HAVE_WCHAR */
