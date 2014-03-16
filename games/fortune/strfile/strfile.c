/*	$NetBSD: strfile.c,v 1.38 2013/09/19 00:34:00 uwe Exp $	*/

/*-
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ken Arnold.
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

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#ifdef __NetBSD__
#include <sys/cdefs.h>
#ifndef lint
__COPYRIGHT("@(#) Copyright (c) 1989, 1993\
 The Regents of the University of California.  All rights reserved.");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)strfile.c	8.1 (Berkeley) 5/31/93";
#else
__RCSID("$NetBSD: strfile.c,v 1.38 2013/09/19 00:34:00 uwe Exp $");
#endif
#endif /* not lint */
#endif /* __NetBSD__ */

#include <sys/types.h>
#include <sys/param.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <inttypes.h>
#include <err.h>

#include "strfile.h"

#ifndef MAXPATHLEN
#define	MAXPATHLEN	1024
#endif	/* MAXPATHLEN */

/*
 *	This program takes a file composed of strings separated by
 * lines starting with two consecutive delimiting character (default
 * character is '%') and creates another file which consists of a table
 * describing the file (structure from "strfile.h"), a table of seek
 * pointers to the start of the strings, and the strings, each terminated
 * by a null byte.  Usage:
 *
 *	% strfile [-iorsx] [ -cC ] sourcefile [ datafile ]
 *
 *	c - Change delimiting character from '%' to 'C'
 *	s - Silent.  Give no summary of data processed at the end of
 *	    the run.
 *	o - order the strings in alphabetic order
 *	i - if ordering, ignore case 
 *	r - randomize the order of the strings
 *	x - set rotated bit
 *
 *		Ken Arnold	Sept. 7, 1978 --
 *
 *	Added ordering options.
 */

# define	STORING_PTRS	(Oflag || Rflag)
# define	CHUNKSIZE	512

# define	ALLOC(ptr,sz)	do { \
			if (ptr == NULL) \
				ptr = malloc(CHUNKSIZE * sizeof *ptr); \
			else if (((sz) + 1) % CHUNKSIZE == 0) \
				ptr = realloc(ptr, ((sz) + CHUNKSIZE) * sizeof *ptr); \
			if (ptr == NULL) \
				err(1, "out of space"); \
		} while (0)

typedef struct {
	char	first;
	off_t	pos;
} STR;

static char *Infile = NULL;		/* input file name */
static char Outfile[MAXPATHLEN] = "";	/* output file name */
static char Delimch = '%';		/* delimiting character */

static int Sflag	= 0;		/* silent run flag */
static int Oflag	= 0;		/* ordering flag */
static int Iflag	= 0;		/* ignore case flag */
static int Rflag	= 0;		/* randomize order flag */
static int Xflag	= 0;		/* set rotated bit */
static long Num_pts	= 0;		/* number of pointers/strings */

static off_t *Seekpts;

static FILE *Sort_1, *Sort_2;		/* pointers for sorting */

static STRFILE Tbl;			/* statistics table */

static STR *Firstch;			/* first chars of each string */


static uint32_t h2nl(uint32_t h);
static void getargs(int argc, char **argv);
static void usage(void) __dead;
static void add_offset(FILE *fp, off_t off);
static void do_order(void);
static int cmp_str(const void *vp1, const void *vp2);
static void randomize(void);
static void fwrite_be_offt(off_t off, FILE *f);


/*
 * main:
 *	Drive the sucker.  There are two main modes -- either we store
 *	the seek pointers, if the table is to be sorted or randomized,
 *	or we write the pointer directly to the file, if we are to stay
 *	in file order.  If the former, we allocate and re-allocate in
 *	CHUNKSIZE blocks; if the latter, we just write each pointer,
 *	and then seek back to the beginning to write in the table.
 */
int
main(int ac, char **av)
{
	char		*sp, dc;
	FILE		*inf, *outf;
	off_t		last_off, length, pos;
	int		first;
	char		*nsp;
	STR		*fp;
	static char	string[257];
	long		i;

	/* sanity test */
	if (sizeof(uint32_t) != 4)
		errx(1, "sizeof(uint32_t) != 4");

	getargs(ac, av);		/* evalute arguments */
	dc = Delimch;
	if ((inf = fopen(Infile, "r")) == NULL)
		err(1, "open `%s'", Infile);

	if ((outf = fopen(Outfile, "w")) == NULL)
		err(1, "open `%s'", Outfile);
	if (!STORING_PTRS)
		(void) fseek(outf, sizeof Tbl, SEEK_SET);

	/*
	 * Write the strings onto the file
	 */

	Tbl.str_longlen = 0;
	Tbl.str_shortlen = (unsigned int) 0x7fffffff;
	Tbl.str_delim = dc;
	Tbl.str_version = VERSION;
	first = Oflag;
	add_offset(outf, ftell(inf));
	last_off = 0;
	do {
		sp = fgets(string, 256, inf);
		if (sp == NULL || (sp[0] == dc && sp[1] == '\n')) {
			pos = ftell(inf);
			length = pos - last_off - (sp ? strlen(sp) : 0);
			last_off = pos;
			if (!length)
				continue;
			add_offset(outf, pos);
			if ((off_t)Tbl.str_longlen < length)
				Tbl.str_longlen = length;
			if ((off_t)Tbl.str_shortlen > length)
				Tbl.str_shortlen = length;
			first = Oflag;
		}
		else if (first) {
			for (nsp = sp; !isalnum((unsigned char)*nsp); nsp++)
				continue;
			ALLOC(Firstch, Num_pts);
			fp = &Firstch[Num_pts - 1];
			if (Iflag && isupper((unsigned char)*nsp))
				fp->first = tolower((unsigned char)*nsp);
			else
				fp->first = *nsp;
			fp->pos = Seekpts[Num_pts - 1];
			first = 0;
		}
	} while (sp != NULL);

	/*
	 * write the tables in
	 */

	(void) fclose(inf);

	if (Oflag)
		do_order();
	else if (Rflag)
		randomize();

	if (Xflag)
		Tbl.str_flags |= STR_ROTATED;

	if (!Sflag) {
		printf("\"%s\" created\n", Outfile);
		if (Num_pts == 2)
			puts("There was 1 string");
		else
			printf("There were %d strings\n", (int)(Num_pts - 1));
		printf("Longest string: %lu byte%s\n", (unsigned long)Tbl.str_longlen,
		       Tbl.str_longlen == 1 ? "" : "s");
		printf("Shortest string: %lu byte%s\n", (unsigned long)Tbl.str_shortlen,
		       Tbl.str_shortlen == 1 ? "" : "s");
	}

	(void) fseek(outf, (off_t) 0, SEEK_SET);
	Tbl.str_version = h2nl(Tbl.str_version);
	Tbl.str_numstr = h2nl(Num_pts - 1);
	Tbl.str_longlen = h2nl(Tbl.str_longlen);
	Tbl.str_shortlen = h2nl(Tbl.str_shortlen);
	Tbl.str_flags = h2nl(Tbl.str_flags);
	(void) fwrite((char *) &Tbl, sizeof Tbl, 1, outf);
	if (STORING_PTRS) {
		for (i = 0; i < Num_pts; i++)
			fwrite_be_offt(Seekpts[i], outf);
	}
	fflush(outf);
	if (ferror(outf))
		err(1, "fwrite %s", Outfile);
	(void) fclose(outf);
	exit(0);
}

/*
 *	This routine evaluates arguments from the command line
 */
static void
getargs(int argc, char **argv)
{
	int	ch;
	extern	int optind;
	extern	char *optarg;

	while ((ch = getopt(argc, argv, "c:iorsx")) != -1)
		switch(ch) {
		case 'c':			/* new delimiting char */
			Delimch = *optarg;
			if (!isascii(Delimch)) {
				printf("bad delimiting character: '\\%o\n'",
				       Delimch);
			}
			break;
		case 'i':			/* ignore case in ordering */
			Iflag++;
			break;
		case 'o':			/* order strings */
			Oflag++;
			break;
		case 'r':			/* randomize pointers */
			Rflag++;
			break;
		case 's':			/* silent */
			Sflag++;
			break;
		case 'x':			/* set the rotated bit */
			Xflag++;
			break;
		case '?':
		default:
			usage();
		}
	argv += optind;

	if (*argv) {
		Infile = *argv;
		if (*++argv)
			(void) strcpy(Outfile, *argv);
	}
	if (!Infile) {
		puts("No input file name");
		usage();
	}
	if (*Outfile == '\0') {
		(void) strcpy(Outfile, Infile);
		(void) strcat(Outfile, ".dat");
	}
}

static void
usage(void)
{
	(void) fprintf(stderr,
	    "Usage: %s [-iorsx] [-c char] sourcefile [datafile]\n",
	    getprogname());
	exit(1);
}

/*
 * add_offset:
 *	Add an offset to the list, or write it out, as appropriate.
 */
static void
add_offset(FILE *fp, off_t off)
{

	if (!STORING_PTRS) {
		fwrite_be_offt(off, fp);
	} else {
		ALLOC(Seekpts, Num_pts + 1);
		Seekpts[Num_pts] = off;
	}
	Num_pts++;
}

/*
 * do_order:
 *	Order the strings alphabetically (possibly ignoring case).
 */
static void
do_order(void)
{
	int	i;
	off_t	*lp;
	STR	*fp;

	Sort_1 = fopen(Infile, "r");
	Sort_2 = fopen(Infile, "r");
	qsort((char *) Firstch, (int) Tbl.str_numstr, sizeof *Firstch, cmp_str);
	i = Tbl.str_numstr;
	lp = Seekpts;
	fp = Firstch;
	while (i--)
		*lp++ = fp++->pos;
	(void) fclose(Sort_1);
	(void) fclose(Sort_2);
	Tbl.str_flags |= STR_ORDERED;
}

static int
cmp_str(const void *vp1, const void *vp2)
{
	const STR	*p1, *p2;
	int	c1, c2;
	int	n1, n2;

	p1 = (const STR *)vp1;
	p2 = (const STR *)vp2;

# define	SET_N(nf,ch)	(nf = (ch == '\n'))
# define	IS_END(ch,nf)	(ch == Delimch && nf)

	c1 = p1->first;
	c2 = p2->first;
	if (c1 != c2)
		return c1 - c2;

	(void) fseek(Sort_1, p1->pos, SEEK_SET);
	(void) fseek(Sort_2, p2->pos, SEEK_SET);

	n1 = 0;
	n2 = 0;
	while (!isalnum(c1 = getc(Sort_1)) && c1 != '\0')
		SET_N(n1, c1);
	while (!isalnum(c2 = getc(Sort_2)) && c2 != '\0')
		SET_N(n2, c2);

	while (!IS_END(c1, n1) && !IS_END(c2, n2)) {
		if (Iflag) {
			if (isupper(c1))
				c1 = tolower(c1);
			if (isupper(c2))
				c2 = tolower(c2);
		}
		if (c1 != c2)
			return c1 - c2;
		SET_N(n1, c1);
		SET_N(n2, c2);
		c1 = getc(Sort_1);
		c2 = getc(Sort_2);
	}
	if (IS_END(c1, n1))
		c1 = 0;
	if (IS_END(c2, n2))
		c2 = 0;
	return c1 - c2;
}

/*
 * randomize:
 *	Randomize the order of the string table.  We must be careful
 *	not to randomize across delimiter boundaries.  All
 *	randomization is done within each block.
 */
static void
randomize(void)
{
	int	cnt, i;
	off_t	tmp;
	off_t	*sp;

	srandom((int)(time(NULL) + getpid()));

	Tbl.str_flags |= STR_RANDOM;
	cnt = Tbl.str_numstr;

	/*
	 * move things around randomly
	 */

	for (sp = Seekpts; cnt > 0; cnt--, sp++) {
		i = random() % cnt;
		tmp = sp[0];
		sp[0] = sp[i];
		sp[i] = tmp;
	}
}

/*
 * fwrite_be_offt:
 *	Write out the off paramater as a 64 bit big endian number
 */

static void
fwrite_be_offt(off_t off, FILE *f)
{
	int		i;
	unsigned char	c[8];

	for (i = 7; i >= 0; i--) {
		c[i] = off & 0xff;
		off >>= 8;
	}
	fwrite(c, sizeof(c), 1, f);
}

static uint32_t
h2nl(uint32_t h)
{
        unsigned char c[4];
        uint32_t rv;

        c[0] = (h >> 24) & 0xff;
        c[1] = (h >> 16) & 0xff;
        c[2] = (h >>  8) & 0xff;
        c[3] = (h >>  0) & 0xff;
        memcpy(&rv, c, sizeof rv);

        return (rv);
}
