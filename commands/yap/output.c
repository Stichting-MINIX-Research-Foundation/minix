/* Copyright (c) 1985 Ceriel J.H. Jacobs */

/*
 * Handle output to screen
 */

# ifndef lint
static char rcsid[] = "$Header$";
# endif

# define _OUTPUT_

# include "in_all.h"
# include "output.h"
# include "main.h"

# define OBUFSIZ 64*128

static char _outbuf[OBUFSIZ];

VOID
flush() {			/* Flush output buffer, by writing it */
	register char *p = _outbuf;

	_ocnt = OBUFSIZ;
	if (_optr) (VOID) write(1, p, _optr - p);
	_optr = p;
}

VOID
nflush() {			/* Flush output buffer, ignoring it */

	_ocnt = OBUFSIZ;
	_optr = _outbuf;
}

int
fputch(ch) char ch; {		/* print a character */
	putch(ch);
}

VOID
putline(s) register char *s; {	/* Print string s */

	if (!s) return;
	while (*s) {
		putch(*s++);
	}
}

/*
 * A safe version of putline. All control characters are echoed as ^X
 */

VOID
cputline(s) char *s; {
	register c;

	while (c = *s++) {
		if ((unsigned) c > 0177) c &= 0177;
		if (c < ' ' || c == 0177) {
			putch('^');
			c ^= 0100;
		}
		putch(c);
	}
}

/*
 * Simple minded routine to print a number
 */

VOID
prnum(n) long n; {

	putline(getnum(n));
}

static char *
fillnum(n, p)
	long n;
	char *p;
{
	if (n >= 10) {
		p = fillnum(n / 10, p);
	}
	*p++ = (int) (n % 10) + '0';
	*p = '\0';
	return p;
}

char *
getnum(n)
	long n;
{
	static char buf[20];

	fillnum(n, buf);
	return buf;
}
