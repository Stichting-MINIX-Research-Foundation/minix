/*	$NetBSD: options.h,v 1.11 2007/04/23 18:40:22 christos Exp $	*/

/*-
 * Copyright (c) 1992 Keith Muller.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Keith Muller of the University of California, San Diego.
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
 *
 *	@(#)options.h	8.2 (Berkeley) 4/18/94
 */

/*
 * argv[0] names. Used for tar and cpio emulation
 */

#define NM_TAR  "tar"
#define NM_CPIO "cpio"
#define NM_PAX  "pax"

/* special value for -E */
#define none	"none"

/*
 * Constants used to specify the legal sets of flags in pax. For each major
 * operation mode of pax, a set of illegal flags is defined. If any one of
 * those illegal flags are found set, we scream and exit
 */

/*
 * flags (one for each option).
 */
#define AF	0x000000001ULL
#define BF	0x000000002ULL
#define CF	0x000000004ULL
#define DF	0x000000008ULL
#define FF	0x000000010ULL
#define IF	0x000000020ULL
#define KF	0x000000040ULL
#define LF	0x000000080ULL
#define NF	0x000000100ULL
#define OF	0x000000200ULL
#define PF	0x000000400ULL
#define RF	0x000000800ULL
#define SF	0x000001000ULL
#define TF	0x000002000ULL
#define UF	0x000004000ULL
#define VF	0x000008000ULL
#define WF	0x000010000ULL
#define XF	0x000020000ULL
#define CAF	0x000040000ULL	/* nonstandard extension */
#define CBF	0x000080000ULL	/* nonstandard extension */
#define CDF	0x000100000ULL	/* nonstandard extension */
#define CEF	0x000200000ULL	/* nonstandard extension */
#define CGF	0x000400000ULL	/* nonstandard extension */
#define CHF	0x000800000ULL	/* nonstandard extension */
#define CLF	0x001000000ULL	/* nonstandard extension */
#define CMF	0x002000000ULL	/* nonstandard extension */
#define CPF	0x004000000ULL	/* nonstandard extension */
#define CTF	0x008000000ULL	/* nonstandard extension */
#define CUF	0x010000000ULL	/* nonstandard extension */
#define VSF	0x020000000ULL	/* non-standard */
#define CXF	0x040000000ULL
#define CYF	0x080000000ULL	/* nonstandard extension */
#define CZF	0x100000000ULL	/* nonstandard extension */

/*
 * ascii string indexed by bit position above (alter the above and you must
 * alter this string) used to tell the user what flags caused us to complain
 */
#define FLGCH	"abcdfiklnoprstuvwxABDEGHLMPTUVXYZ"

/*
 * legal pax operation bit patterns
 */

#define ISLIST(x)	(((x) & (RF|WF)) == 0)
#define	ISEXTRACT(x)	(((x) & (RF|WF)) == RF)
#define ISARCHIVE(x)	(((x) & (AF|RF|WF)) == WF)
#define ISAPPND(x)	(((x) & (AF|RF|WF)) == (AF|WF))
#define	ISCOPY(x)	(((x) & (RF|WF)) == (RF|WF))
#define	ISWRITE(x)	(((x) & (RF|WF)) == WF)

/*
 * Illegal option flag subsets based on pax operation
 */

#define	BDEXTR	(AF|BF|LF|TF|WF|XF|CBF|CHF|CLF|CMF|CPF|CXF)
#define	BDARCH	(CF|KF|LF|NF|PF|RF|CDF|CEF|CYF|CZF)
#define	BDCOPY	(AF|BF|FF|OF|XF|CAF|CBF|CEF)
#define	BDLIST (AF|BF|IF|KF|LF|OF|PF|RF|TF|UF|WF|XF|CBF|CDF|CHF|CLF|CMF|CPF|CXF|CYF|CZF)
