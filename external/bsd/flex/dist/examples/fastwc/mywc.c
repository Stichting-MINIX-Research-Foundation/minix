/*	$NetBSD: mywc.c,v 1.1.1.1 2009/10/26 00:28:34 christos Exp $	*/

/* A simple but fairly efficient C version of the Unix "wc" tool */

#include <stdio.h>
#include <ctype.h>

main()
{
	register int c, cc = 0, wc = 0, lc = 0;
	FILE *f = stdin;

	while ((c = getc(f)) != EOF) {
		++cc;
		if (isgraph(c)) {
			++wc;
			do {
				c = getc(f);
				if (c == EOF)
					goto done;
				++cc;
			} while (isgraph(c));
		}
		if (c == '\n')
			++lc;
	}
done:	printf( "%8d%8d%8d\n", lc, wc, cc );
}
