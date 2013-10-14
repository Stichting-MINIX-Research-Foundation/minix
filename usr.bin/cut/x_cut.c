/*	$NetBSD: x_cut.c,v 1.2 2007/07/02 18:41:04 christos Exp $	*/

/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Adam S. Moskowitz of Menlo Consulting and Marciano Pitargue.
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

/*
 *  This file is #include'd twice from cut.c, to generate both
 *  single- and multibyte versions of the same code.
 *
 *  In cut.c #define:
 *   CUT_BYTE=0 to define b_cut (singlebyte), and
 *   CUT_BYTE=1 to define c_cut (multibyte).
 *
 */

#if (CUT_BYTE == 1)
#  define CUT_FN 		b_cut
#  define CUT_CH_T 		int
#  define CUT_GETC 		getc
#  define CUT_EOF 		EOF
#  define CUT_PUTCHAR		putchar
#else
#  define CUT_FN 		c_cut
#  define CUT_CH_T 		wint_t
#  define CUT_GETC 		getwc
#  define CUT_EOF		WEOF
#  define CUT_PUTCHAR		putwchar
#endif


/* ARGSUSED */
void
CUT_FN(FILE *fp, const char *fname __unused)
{
	CUT_CH_T ch;
	int col;
	char *pos;

	ch = 0;
	for (;;) {
		pos = positions + 1;
		for (col = maxval; col; --col) {
			if ((ch = CUT_GETC(fp)) == EOF)
				return;
			if (ch == '\n')
				break;
			if (*pos++)
				(void)CUT_PUTCHAR(ch);
		}
		if (ch != '\n') {
			if (autostop)
				while ((ch = CUT_GETC(fp)) != CUT_EOF && ch != '\n')
					(void)CUT_PUTCHAR(ch);
			else
				while ((ch = CUT_GETC(fp)) != CUT_EOF && ch != '\n');
		}
		(void)CUT_PUTCHAR('\n');
	}
}

#undef CUT_FN
#undef CUT_CH_T
#undef CUT_GETC
#undef CUT_EOF
#undef CUT_PUTCHAR

