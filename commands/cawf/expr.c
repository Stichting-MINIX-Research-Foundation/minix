/*
 *	expr.c - expression support functions for cawf(1)
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
 * Asmcode(s, c) - assemble number/name code following backslash-character
 *		   definition  - e. .g, "\\nPO"
 */

unsigned char *
Asmcode(s, c)
	unsigned char **s;		/* pointer to character after '\\' */
	unsigned char *c;		/* code destination (c[3]) */
{
	unsigned char *s1;

	s1 = *s + 1;
	c[0] = c[1] = c[2] = '\0';
	if ((c[0] = *s1) == '(') {
		s1++;
		if ((c[0] = *s1) != '\0') {
			s1++;
			c[1] = *s1;
		}
	}
	return(s1);
}


/*
 * Delnum(nx) - delete number
 */

void
Delnum(nx)
	int nx;				/* number index */
{
	unsigned char buf[MAXLINE];	/* message buffer */

	if (nx >= Nnr) {
		(void) sprintf((char *)buf, " bad Delnum(%d) index", nx);
		Error(FATAL, LINE, (char *)buf, NULL);
	}
	while (nx < (Nnr - 1)) {
		Numb[nx] = Numb[nx + 1];
		nx++;
	}
	Nnr--;
}


/*
 * Findnum(n, v, e) - find or optionally enter number value
 */

Findnum(n, v, e)
	unsigned char *n;		/* register name */
	int v;				/* value */
	int e;				/* 0 = find, don't enter
					 * 1 = enter, don't find */
{
	int cmp, low, hi, mid;		/* binary search controls */
	unsigned char c[3];		/* name buffer */

	c[0] = n[0];
	c[1] = (n[1] == ' ' || n[1] == '\t') ? '\0' : n[1];
	c[2] = '\0';
	low = mid = 0;
	hi = Nnr - 1;
	while (low <= hi) {
		mid = (low + hi) / 2;
		if ((cmp = strncmp((char *)c, (char *)Numb[mid].nm, 2)) < 0)
			hi = mid - 1;
		else if (cmp > 0)
			low = mid + 1;
		else {
			if (e)
				Numb[mid].val = v;
			return(mid);
		}
	}
	if ( ! e)
		return(-1);
	if (Nnr >= MAXNR)
		Error(FATAL, LINE, " out of number registers at ", (char *)c);
	if (Nnr) {
		if (cmp > 0)
			mid++;
		for (hi = Nnr - 1; hi >= mid; hi--)
			Numb[hi+1] = Numb[hi];
	}
	Nnr++;
	Numb[mid].nm[0] = c[0];
	Numb[mid].nm[1] = c[1];
	Numb[mid].val = v;
	return(mid);
}


/*
 * Findparms(n) - find parameter registers
 */

Findparms(n)
	unsigned char *n;		/* parameter name */
{
	unsigned char c[3];		/* character buffer */
	int i;				/* temporary index */

	c[0] = n[0];
	c[1] = (n[1] == ' ' || n[1] == '\t') ? '\0' : n[1];
	c[2] = '\0';
	for (i = 0; Parms[i].nm[0]; i++) {
		if (c[0] == Parms[i].nm[0] && c[1] == Parms[i].nm[1])
			return(i);
	}
	return(-1);
}


/*
 * Findscale(n, v, e) - find and optionally enter scaling factor value
 */

Findscale(n, v, e)
	int n;				/* scaling factor name */
	double v;			/* value */
	int e;				/* 0 = find, don't enter
					 * 1 = enter, don't find */
{
	int i;
	double *pval;

	for (i = 0; Scale[i].nm; i++) {
		if ((unsigned char )n == Scale[i].nm)
			break;
	}
	if (Scale[i].nm) {
		if (e) {
			pval = &Scale[i].val;
			*pval = v;
		}
		return(i);
	}
	return(-1);
}
