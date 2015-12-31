/*	$NetBSD: dump.c,v 1.4 2014/01/28 20:43:55 joerg Exp $ */
/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#if defined(__NetBSD__)
#include <sys/cdefs.h>
#if 0
#ifndef lint
static char copyright[] =
"%Z% Copyright (c) 1992, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */
#else
__RCSID("$NetBSD: dump.c,v 1.4 2014/01/28 20:43:55 joerg Exp $");
#endif
#endif

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

static void
parse(fp)
	FILE *fp;
{
	int ch, s1, s2, s3;

#define	TESTD(s) {							\
	if ((s = getc(fp)) == EOF)					\
		return;							\
	if (!isdigit(s))						\
		continue;						\
}
#define	TESTP {								\
	if ((ch = getc(fp)) == EOF)					\
		return;							\
	if (ch != '|')							\
		continue;						\
}
#define	MOVEC(t) {							\
	do {								\
		if ((ch = getc(fp)) == EOF)				\
			return;						\
	} while (ch != (t));						\
}
	for (;;) {
		MOVEC('"');
		TESTD(s1);
		TESTD(s2);
		TESTD(s3);
		TESTP;
		putchar('"');
		putchar(s1);
		putchar(s2);
		putchar(s3);
		putchar('|');
		for (;;) {		/* dump to end quote. */
			if ((ch = getc(fp)) == EOF)
				return;
			putchar(ch);
			if (ch == '"')
				break;
			if (ch == '\\') {
				if ((ch = getc(fp)) == EOF)
					return;
				putchar(ch);
			}
		}
		putchar('\n');
	}
}

int
main(argc, argv)
	int argc;
	char *argv[];
{
	FILE *fp;

	for (; *argv != NULL; ++argv) {
		if ((fp = fopen(*argv, "r")) == NULL) {
			perror(*argv);
			exit (1);
		}
		parse(fp);
		(void)fclose(fp);
	}
	exit (0);
}
