/* regsub
 *
 *	Copyright (c) 1986 by University of Toronto.
 *	Written by Henry Spencer.  Not derived from licensed software.
 *
 *	Permission is granted to anyone to use this software for any
 *	purpose on any computer system, and to redistribute it freely,
 *	subject to the following restrictions:
 *
 *	1. The author is not responsible for the consequences of use of
 *		this software, no matter how awful, even if they arise
 *		from defects in it.
 *
 *	2. The origin of this software must not be misrepresented, either
 *		by explicit claim or by omission.
 *
 *	3. Altered versions must be plainly marked as such, and must not
 *		be misrepresented as being the original software.
 */

#include <string.h>
#include <stdio.h>
#define const		/* avoid "const poisoning" */
#include <regexp.h>
#undef const

/* The first byte of the regexp internal "program" is actually this magic
 * number; the start node begins in the second byte.
 */
#define	MAGIC	0234

#define CHARBITS 0377
#ifndef CHARBITS
#define	UCHARAT(p)	((int)*(unsigned char *)(p))
#else
#define	UCHARAT(p)	((int)*(p)&CHARBITS)
#endif

/*
 - regsub - perform substitutions after a regexp match
 */
void regsub(prog, source, dest)
regexp *prog;
char *source;
char *dest;
{
  register char *src;
  register char *dst;
  register char c;
  register int no;
  register int len;

  if (prog == (regexp *)NULL || source == (char *)NULL || dest == (char *)NULL) {
	regerror("NULL parm to regsub");
	return;
  }
  if (UCHARAT(prog->program) != MAGIC) {
	regerror("damaged regexp fed to regsub");
	return;
  }
  src = source;
  dst = dest;
  while ((c = *src++) != '\0') {
	if (c == '&')
		no = 0;
	else if (c == '\\' && '0' <= *src && *src <= '9')
		no = *src++ - '0';
	else
		no = -1;

	if (no < 0) {		/* Ordinary character. */
		if (c == '\\' && (*src == '\\' || *src == '&')) c = *src++;
		*dst++ = c;
	} else
	if (prog->startp[no] != (char *)NULL && prog->endp[no] != (char *)NULL) {
		len = (int) (prog->endp[no] - prog->startp[no]);
		strncpy(dst, prog->startp[no], len);
		dst += len;
		if (len != 0 && *(dst - 1) == '\0') {	/* strncpy hit NUL. */
			regerror("damaged match string");
			return;
		}
	}
  }
  *dst++ = '\0';
}

/*
 * $PchId: regsub.c,v 1.3 1995/11/27 20:18:16 philip Exp $
 */
