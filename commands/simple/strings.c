/* strings - print ASCII strings in a file	Author: Peter S. Housel */

/*
  This is a version of the BSD "strings" program for Minix. It is used
  to search a file for printable strings. To install,

  cc -o strings strings.c
  chmem =8000 strings

Command:  strings - search file for printable strings
Syntax:	  strings [-] [-o] [-len] file ...
Flags:	  -	Search the entire file. If this option is not given, only
	 	the initialized data segment of files that appear to be
		"a.out" format is searched.
    	  -o	Print the offset (in octal) with each string.
          -len	Use "len" as the minimum string length. The default is 4.

Examples: strings core
	  strings -o a.out > str

Strings searches the specified file(s) for printable ASCII strings (four
or more printable characters followed by a newline or a null) and writes
them to the standard output. This can be used to find out, for example, to
find out what program a "core" file came from, what kinds of error messages
are in an executable, or to see ASCII data hidden in a "binary" data file.

P.S. The program doesn't use the "a.out.h" file posted last week by
Dick van Veen, both because it was written before then, and because
not everybody has a.out.h yet. Future revisions probably ought to, though.

*/



#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Minix (8086 version) dependant definitions */
#define SMALLMAGIC	0x04100301L	/* small model a.out magic number */
#define SEPARATEMAGIC	0x04200301L	/* separate instruction/data a.out */

#define HDR_MAGIC	0	/* 0'th long  magic number */
#define HDR_HSIZE	1	/* 1'st long  size of header */
#define HDR_TSIZE	2	/* 2'nd long  size of text */
#define HDR_DSIZE	3	/* 3'rd long  size of init'ed data */
#define HDR_BSIZE	4	/* 4'th long  size of bss */
#define HDR_TOTMEM	6	/* 6'th long  total memory */

#define HDR_LEN		8	/* total length of header */

/* Miscellaneous definitions */
#define	STRLEN		4	/* default minimum string length */
#define STRBUF		512	/* buffer length for strings */

_PROTOTYPE(int main, (int argc, char **argv));
_PROTOTYPE(void strings, (char *filename));
_PROTOTYPE(void usage, (void));

int strmin = STRLEN;		/* minimum string length */
int printoff = 0;		/* print octal offset of each str */
int objall = 0;			/* search entire a.out file, not */

/* Just initialized data segment */

int main(argc, argv)
int argc;
char *argv[];
{
  while ((++argv, --argc) && '-' == (*argv)[0]) {
	if (!strcmp(*argv, "-"))
		++objall;
	else if (!strcmp(*argv, "-o"))
		++printoff;
	else if (isdigit((*argv)[1]))
		strmin = atoi(&(*argv)[1]);
	else
		usage();
  }

  if (0 == argc) usage();
  while (argc--) strings(*argv++);
  return(0);
}

void strings(filename)
char *filename;
{
  char buf[STRBUF];		/* the strings buffer */
  char *bufptr;			/* pointer into the strings buffer */
  FILE *input;			/* input file */
  long header[HDR_LEN];		/* buffer for reading the header */
  long offset;			/* file offset */
  long limit;			/* limit, if doing data segment only */
  int c;			/* input character */

  if (NULL == (input = fopen(filename, "r"))) {
	fprintf(stderr, "strings: ");
	perror(filename);
	exit(1);
  }
  if (HDR_LEN == fread(header, sizeof(long), (size_t)HDR_LEN, input)
      && (SMALLMAGIC == header[HDR_MAGIC]
	||SEPARATEMAGIC == header[HDR_MAGIC]) && !objall) {
	offset = header[HDR_HSIZE] + header[HDR_TSIZE];	/* object file */
	limit = offset + header[HDR_DSIZE];
  } else {
	offset = 0L;
	limit = 0L;
  }

  fseek(input, offset, 0);
  bufptr = buf;

  while (!limit || offset < limit) {
	if (EOF == (c = getc(input))) break;
	if ((('\0' == c || '\n' == c) && bufptr - buf >= strmin)
	    || (bufptr - buf == STRBUF - 1)) {
		*bufptr = '\0';
		if (printoff) printf("%lo:", offset - (bufptr - buf));
		puts(buf);
		bufptr = buf;
	} else if ((' ' <= c && c < 0177) || '\t' == c)
		*bufptr++ = c;
	else
		bufptr = buf;

	++offset;
  }

  fclose(input);
}

void usage()
{
  fprintf(stderr, "usage: strings [-] [-o] [-num] file ...\n");
  exit(1);
}
