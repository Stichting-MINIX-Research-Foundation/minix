/*
 *	bsfilt.c - a colcrt-like processor for cawf(1)
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

#include <stdio.h>

#ifdef UNIX
# ifdef USG
#include <string.h>
# else	/* not USG */
#include <strings.h>
# endif	/* USG */
#else	/* not UNIX */
#include <string.h>
#endif	/* UNIX */

#include <sys/types.h>

#include "ansi.h"

#define MAXLL	2048			/* ridiculous maximum line length */

int Dash = 1;				/* underline with dashes */
int Dp = 0;				/* dash pending */
int Lc = 0;				/* line count */
char *Pname;				/* program name */
unsigned char Ulb[MAXLL];		/* underline buffer */
int Ulx = 0;				/* underline buffer index */

_PROTOTYPE(void Putchar,(int ch));

main(argc, argv)
	int argc;
	char *argv[];
{
	int ax = 1;			/* argument index */
	unsigned char c;		/* character buffer */
	FILE *fs;			/* file stream */
	int nf = 0;			/* number of files processed */
	unsigned char pc;		/* previous character */
	int under = 0;                  /* underline */
/*
 * Save program name.
 */
	if ((Pname = strrchr(argv[0], '/')) != NULL)
		Pname++;
	else if ((Pname = strrchr(argv[0], '\\')) != NULL)
		Pname++;
	else
		Pname = argv[0];
/*
 * Process options.
 */
	if (argc > 1 && argv[1][0] == '-') {
		switch (argv[1][1]) {
	/*
	 * "-U" - underline with dashes.
	 */
		case 'U':
			Dash = 0;
			under = 1;
			break;
	/*
	 * "-" - do no  underlining at all.
	 */
		case '\0':
			Dash = under = 0;
			break;
		default:
			(void) fprintf(stderr,
				"%s usage: [-] [-U] [file]\n", Pname);
			exit(1);
		}
		ax++;
	}
/*
 * Process files.  Read standard input if no files names.
 */

	while (ax < argc || nf == 0) {
		if (ax >= argc)
			fs = stdin;
		else {
#ifdef	UNIX
			if ((fs = fopen(argv[ax], "r")) == NULL)
#else
			if ((fs = fopen(argv[ax], "rt")) == NULL)
#endif
			{
				(void) fprintf(stderr, "%s: can't open %s\n",
					Pname, argv[ax]);
				exit(1);
			}
			ax++;
		}
		nf++;
	/*
	 * Read input a character at a time.
	 */
		for (pc = '\0';;) {
			c = (unsigned char)fgetc(fs);
			if (feof(fs))
				break;
			switch(c) {

			case '\n':
				if (pc)
					Putchar((int)pc);
				Putchar('\n');
				pc = '\0';
				break;

			case '\b':
				if (pc == '_') {
					if (under) {
						putchar(pc);
						putchar('\b');
					} else if (Dash)
						Dp = 1;
				}
				pc = '\0';
				break;

			default:
				if (pc)
					Putchar((int)pc);
				pc = c;
			}
		}
		if (pc) {
			Putchar((int)pc);
			Putchar((int)'\n');
		}
	}
	exit(0);
}


/*
 * Putchar(ch) - put a character with possible underlining
 */

void
Putchar(ch)
	int ch;
{
	int i;					/* temporary index */

	if ((unsigned char)ch == '\n') {
/*
 * Handle end of line.
 */
		putchar('\n');
		if (Ulx) {
			while (Ulx && Ulb[Ulx-1] == ' ')
				Ulx--;
			if (Ulx) {
				for (i = 0; i < Ulx; i++)
					putchar(Ulb[i]);
				putchar('\n');
			}
		}
		Dp = Ulx = 0;
		Lc++;
		return;
	}
/*
 * Put "normal" character.
 */
	putchar((unsigned char)ch);
	if (Dash) {

	/*
	 * Handle dash-type underlining.
	 */
		if (Ulx >= MAXLL) {
			(void) fprintf(stderr,
				"%s: underline for line %d > %d characters\n",
				Pname, Lc, MAXLL);
			exit(1);
		}
		Ulb[Ulx++] = Dp ? '-' : ' ';
		Dp = 0;
	}
}
