/*
 *	output-c - output support functions for cawf(1)
 */

/*
 *	Copyright (c) 1991 Purdue University Research Foundation,
 *	West Lafayette, Indiana 47907.  All rights reserved.
 *
 *	Written by Victor A. Abell <abe@mace.cc.purdue.edu>,  Purdue
 *	University Computing Center.  Not derived from licensed software;
 *	derived from awf(1) by Henry Spencer of the University of Toronto.
 *
 *	Permission is granted to anyone to use this software for any
 *	purpose on any computer system, and to alter it and redistribute
 *	it freely, subject to the following restrictions:
 *
 *	1. The author is not responsible for any consequences of use of
 *	   this software, even if they arise from flaws in it.
 *
 *	2. The origin of this software must not be misrepresented, either
 *	   by explicit claim or by omission.  Credits must appear in the
 *	   documentation.
 *
 *	3. Altered versions must be plainly marked as such, and must not
 *	   be misrepresented as being the original software.  Credits must
 *	   appear in the documentation.
 *
 *	4. This notice may not be removed or altered.
 */

#include "cawf.h"


/*
 * LenprtHF(s, p, t) - get length of print header or footer with page number
 *		       interpolation
 */

int LenprtHF(unsigned char *s, int p, int t) {
/* header/footer string s
 * page number p
 * type t: 0 = get interpolated length
 *	 1 = print
 */
	unsigned char buf[10];		/* buffer for page number */
	int len;			/* line length */
	unsigned char *s1;		/* temporary string pointer */
	
	if (s == NULL)
		return(0);
	for (len = 0; *s && *s != '%'; s++) {
		len++;
		if (t)
			Charput((int)*s);
	}
	if (*s) {
		(void) sprintf((char *)buf, "%d", p);
		for (s1 = buf; *s1; s1++) {
			len++;
			if (t)
				Charput((int)*s1);
		}
		for (s++; *s; s++) {
			len++;
			if (t)
				Charput((int)*s);
		}
	}
	return(len);
}


/*
 * Charput(s) - put a character to output, subject to diversion
 */

void Charput(int c) {
/* character to put c */
	if (Divert == 0)
		putchar((unsigned char)c);
}


/*
 * Stringput(s) - put a string to output, subject to diversion
 */

void Stringput(unsigned char *s) {
/* string to put s */
	if (Divert == 0)
		fputs((char *)s, stdout);
}
