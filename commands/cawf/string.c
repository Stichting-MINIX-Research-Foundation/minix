/*
 *	string.c - string support functions for cawf(1)
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
#include <ctype.h>

_PROTOTYPE(static void Setroman,());


/*
 * Asmname(s, c) - assemble name
 */

Asmname(s, c)
	unsigned char *s;		/* pointer to name */
	unsigned char *c;		/* code destination (c[3]) */
{

	c[1] = c[2] = '\0';
	while (*s && *s == ' ')
		s++;
	if ((c[0] = *s) == '\0')
		return(0);
	return(((c[1] = s[1]) == '\0') ? 1 : 2);
}


/*
 * Delstr(sx) - delete string
 */

void
Delstr(sx)
	int sx;				/* string index */
{
	char buf[MAXLINE];		/* message buffer */

	if (sx >= Nstr) {
		(void) sprintf(buf, " bad Delstr(%d) index", sx);
		Error(FATAL, LINE, buf, NULL);
	}
	Free(&Str[sx].str);
	while (sx < (Nstr - 1)) {
		Str[sx] = Str[sx + 1];
		sx++;
	}
	Nstr--;
}


/*
 * Endword() - end a word
 */

void
Endword()
{
	if (Fontstat != 'R')
		Setroman();
	Word[Wordx] = '\0';
}


/*
 * Findchar(nm, l, s, e) - find special character definition and
 *			   optionally enter it
 */

Findchar(nm, l, s, e)
	unsigned char *nm;		/* character name */
	int l;				/* effective length */
	unsigned char *s;		/* value string */
	int e;				/* 0 = find, don't enter
					 * 1 = don't find, enter */
{
	int cmp, hi, low, mid;
	unsigned char c[3];

	c[0] = nm[0];
	c[1] = (nm[1] == ' ' || nm[1] == '\t') ? '\0' : nm[1];
	c[2] = '\0';
	low = mid = 0;
	hi = Nsch - 1;
	while (low <= hi) {
		mid = (low + hi) / 2;
		if ((cmp = strncmp((char *)c, (char *)Schar[mid].nm, 2)) < 0)
			hi = mid - 1;
		else if (cmp > 0)
			low = mid + 1;
		else {
			if ( ! e)
				return(mid);
			Free(&Schar[mid].str);
			goto new_char;
		}
	}
	if ( ! e)
		return(-1);
	if (Nsch >= MAXSCH)
		Error(FATAL, LINE, " at character table limit", NULL);
	if (Nsch) {
		if (cmp > 0)
			mid++;
		for (hi = Nsch - 1; hi >= mid; hi--)
			Schar[hi+1] = Schar[hi];
	}
	Nsch++;
	Schar[mid].nm[0] = c[0];
	Schar[mid].nm[1] = c[1];

new_char:

	Schar[mid].str = Newstr(s);
	Schar[mid].len = l;
	return(mid);
}


/*
 * Findhy(s, l, e) - find and optionally enter hyphen
 */

Findhy(s, l, e)
	unsigned char *s;		/* value string */
	int l;				/* equivalent length */
	int e;				/* 0 = find, don't enter
					 * 1 = enter, don't find */
{
	int i;

	for (i = 0; i < Nhy; i++) {
		if (Font[0] == Hychar[i].font)
			break;
	}
	if (i >= Nhy) {
		if ( ! e)
			return(-1);
		if (Nhy >= MAXHYCH)
			Error(FATAL, LINE, " at hyphen limit for font ",
				(char *)Font);
		Hychar[i].font = Font[0];
		Nhy++;
	} else {
		if ( ! e)
			return(i);
		Error(WARN, LINE, " duplicate hyphen for font ", (char *)Font);
		Free(&Hychar[i].str);
	}
	Hychar[i].str = Newstr(s);
	Hychar[i].len = l;
	return(i);
}


/*
 * Findstr(nm, s, e) - find and  optionally enter string in Str[]
 */

unsigned char *
Findstr(nm, s, e)
	unsigned char *nm;		/* 2 character string name */
	unsigned char *s;		/* string value */
	int e;				/* 0 = find, don't enter
					 * 1 = enter, don't find */
{
	unsigned char c[3];		/* character buffer */
	int cmp, hi, low, mid;		/* binary search controls */
	int i;				/* temporary indexes */
	unsigned char *s1, *s2;		/* temporary string pointers */

	c[0] = nm[0];
	c[1] = (nm[1] == ' ' || nm[1] == '\t') ? '\0' : nm[1];
	c[2] = '\0';
	low = mid = 0;
	hi = Nstr - 1;
	Sx = -1;
	while (low <= hi) {
		mid = (low + hi) / 2;
		if ((cmp = strncmp((char *)c, (char *)Str[mid].nm, 2)) < 0)
			hi = mid - 1;
		else if (cmp > 0)
			low = mid + 1;
		else {
			Sx = mid;
			if ( ! e)
				return(Str[mid].str);
			Free(&Str[mid].str);
			goto new_string;
		}
	}
	if ( ! e)
		return((unsigned char *)"");
	if (Nstr >= MAXSTR)
		Error(FATAL, LINE, " out of space for string ", (char *)c);
	if (Nstr) {
		if (cmp > 0)
			mid++;
		for (hi = Nstr - 1; hi >= mid; hi--)
			Str[hi+1] = Str[hi];
	}
	Nstr++;
	Sx = mid;
	Str[mid].nm[0] = c[0];
	Str[mid].nm[1] = c[1];

new_string:

	if (s == NULL)
		return (Str[mid].str = Newstr((unsigned char *)""));
	i = (*s == '"') ? 1 : 0;
	s1 = Str[mid].str = Newstr(s + i);
	if (i) {
		s2 = s1 + strlen((char *)s1);
		if (s2 > s1 && *(s2-1) == '"')
			*(s2-1) = '\0';
	}
	return(s1);
}


/*
 * Setroman() - set Roman font
 */

static void
Setroman()
{
	int i;

	if ((Wordx + Fstr.rl) >= MAXLINE)
		Error(WARN, LINE, " word too long", NULL);
	else {
		if (Fstr.r) {
			for (i = 0; i < Fstr.rl; i++) {
				Word[Wordx++] = Fstr.r[i];
			}
	    	}
		Fontstat = 'R';
	}
}


/*
 * Str2word(s, len) - copy len characters from string to Word[]
 */

Str2word(s, len)
	unsigned char *s;
	int len;
{
	int i;

	for (; len > 0; len--, s++) {
		switch (Font[0]) {
		case 'B':
		case 'C':
			if (Fontctl == 0) {
				if ((Wordx + 5) >= MAXLINE) {
word_too_long:
					Error(WARN, LINE, " word too long",
						NULL);
					return(1);
				}
				Word[Wordx++] = Trtbl[(int)*s];
				Word[Wordx++] = '\b';
				Word[Wordx++] = Trtbl[(int)*s];
				Word[Wordx++] = '\b';
				Word[Wordx++] = Trtbl[(int)*s];
				break;
			}
			if (Fontstat != Font[0]) {
				if (Fontstat != 'R')
					Setroman();
				if ((Wordx + Fstr.bl) >= MAXLINE)
					goto word_too_long;
				if (Fstr.b) {
					for (i = 0; i < Fstr.bl; i++) {
						Word[Wordx++] = Fstr.b[i];
					}
				}
				Fontstat = Font[0];
			}
			if ((Wordx + 1) >= MAXLINE)
				goto word_too_long;
			Word[Wordx++] = Trtbl[(int)*s];
			break;
		case 'I':
			if (isalnum(*s)) {
				if (Fontctl == 0) {
					if ((Wordx + 3) >= MAXLINE)
						goto word_too_long;
					Word[Wordx++] = '_';
					Word[Wordx++] = '\b';
					Word[Wordx++] = Trtbl[(int)*s];
					break;
				}
				if (Fontstat != 'I') {
					if (Fontstat != 'R')
						Setroman();
					if ((Wordx + Fstr.itl) >= MAXLINE)
						goto word_too_long;
					if (Fstr.it) {
					    for (i = 0; i < Fstr.itl; i++) {
						Word[Wordx++] = Fstr.it[i];
					    }
					}
					Fontstat = 'I';
				}
				if ((Wordx + 1) >= MAXLINE)
					goto word_too_long;
				Word[Wordx++] = Trtbl[(int)*s];
				break;
			}
			/* else fall through */
		default:
			if (Fontstat != 'R')
				Setroman();
			if ((Wordx + 1) >= MAXLINE)
				goto word_too_long;
			Word[Wordx++] = Trtbl[(int)*s];
		}
	}
	return(0);
}
