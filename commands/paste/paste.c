/* paste - laminate files		Author: David Ihnat */

/* Paste - a recreation of the Unix(Tm) paste(1) command.
 *
 * syntax: paste file1 file2 ... paste -dLIST file1 file2 ... paste -s [-dLIST]
 * file1 file2 ...
 *
 * Copyright (C) 1984 by David M. Ihnat
 *
 * This program is a total rewrite of the Bell Laboratories Unix(Tm) command of
 * the same name, as of System V.  It contains no proprietary code, and
 * therefore may be used without violation of any proprietary agreements
 * whatsoever.  However, you will notice that the program is copyrighted by
 * me.  This is to assure the program does *not* fall into the public domain.
 * Thus, I may specify just what I am now: This program may be freely copied
 * and distributed, provided this notice remains; it may not be sold for
 * profit without express written consent of the author. Please note that I
 * recreated the behavior of the Unix(Tm) 'paste' command as faithfully as
 * possible, with minor exceptions (noted below); however, I haven't run a
 * full set of regression * tests.  Thus, the user of this program accepts
 * full responsibility for any effects or loss; in particular, the author is
 * not responsible for any losses, explicit or incidental, that may be
 * incurred through use of this program.
 *
 * The changes to the program, with one exception, are transparent to a user
 * familiar with the Unix command of the same name.  These changes are:
 *
 * 1) The '-s' option had a bug in the Unix version when used with multiple
 *    files.  (It would repeat each file in a list, i.e., for 
 *
 *		paste -s file1 file2 file3
 *
 * it would list
 *
 *	 <file1\n><file1\n><file2\n><file1\n><file2\n><file3\n>
 *
 * I fixed this, and reported the bug to the providers of the command in Unix.
 *
 * 2) The list of valid escape sequences has been expanded to include \b,\f, 
 *    and \r.  (Just because *I* can't imagine why you'd want to use them 
 *    doesn't mean I should keep them from you.)
 *
 * 3) There is no longer any restriction on line length.
 *
 * I ask that any bugs (and, if possible, fixes) be reported to me when
 * possible.  -David Ihnat (312) 784-4544 ihuxx!ignatz
 */

/* Modified to run under MINIX 1.1 by David O. Tinker  (416) 978-3636
 * (utgpu!dtinker) Sept. 19, 1987
 */

/* Modified to conform to POSIX 1003.2/Draft10 standard 23rd Sept. 1990
 * Changes:
 * 	- the arguments can be in any order
 *	- removed the ToUpper function
 * by Thomas Brupbacher (tobr@mw.lpc.ethz.ch)
 */

#include <errno.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* I'd love to use enums, but not everyone has them.  Portability, y'know. */
#define NODELIM		1
#define USAGE		2
#define BADFILE		3
#define TOOMANY		4

#define	TAB		'\t'
#define	NL		'\n'
#define	BS		'\b'
#define	FF		'\f'
#define	CR		'\r'
#define DEL             '\177'
#define SPACE		' '
#define BACKSLASH	'\\'

#define	_MAXSZ		512
#define _MAXFILES	12
#define	CLOSED		((FILE *)-1)
#define ENDLIST		((FILE *)-2)

char *cmdnam;

short int sflag;
static char default_delims[] = {TAB};	/* default delimiter string	 */
char *delims;			/* the pointer to the delimiters        */
int number_of_delims = 1;	/* number of delimiters to use	 	 */

int main(int argc, char **argv);
void docol(int nfiles, char **fnamptr);
void doserial(int nfiles, char **fnamptr);
void delimbuild(char *strptr);
void prerr(int etype, char *estring);

int main(argc, argv)
int argc;
char **argv;
{
  char **arg_ptr;		/* used to save argv, needed for docol() etc */
  int num_files = 0;		/* Number of filenames specified on cmd line */
  sflag = 0;
  delims = default_delims;	/* use default delimiters */

  cmdnam = *argv;

  if (argc >= 2) {

	/* Skip invocation name */
	argv++;
	argc--;

	/* Save argv */
	arg_ptr = argv;
	/* First, parse input options */

	while (argc-- > 0) {
		if (argv[0][0] == '-' && argv[0][1] != '\0') {
			switch (argv[0][1]) {
			    case 'd':
				/* Delimiter character(s) */
				if (*(++argv) == '\0')
					prerr(NODELIM, "");
				else
					delimbuild(*(argv));
				argc--;
				break;

			    case 's':	sflag++;	break;

			    default:	prerr(USAGE, "");
			}
			argv++;
		} else {
			num_files++;
			argv++;
		}
	}

	/* If there are more than MAX_FILES files on the command
	 * line, exit with error message.						 */
	if (num_files > _MAXFILES) prerr(TOOMANY, "");

	/* If no files specified, simply exit.  Otherwise, if not the
	 * old '-s' option, process all files. If '-s', then process
	 * files one-at-a-time. */

	if (!sflag)
		docol(num_files, arg_ptr);	/* Column paste */
	else
		doserial(num_files, arg_ptr);	/* Serial paste */

	exit(0);
  } else
	prerr(USAGE, "");
  return(0);
}

void docol(nfiles, fnamptr)
int nfiles;
char **fnamptr;
{
  char iobuff[_MAXSZ];		/* i/o buffer for the fgets */
  short int somedone;		/* flag for blank field handling */

  /* There is a strange case where all files are just ready to be
   * closed, or will on this round.  In that case, the string of
   * delimiters must be preserved.  delbuf[1] ->delbuf[MAXFILES+1]
   * provides intermediate storage for closed files, if needed;
   * delbuf[0] is the current index.
   */
  char delbuf[_MAXFILES + 2];

  FILE *fileptr[_MAXFILES + 1];

  int filecnt;			/* Set to number of files to process */
  register char *delimptr;	/* Cycling delimiter pointer */
  int index;			/* Working variable */
  int strend;			/* End of string in buffer */

  /* Perform column paste.  First, attempt to open all files. (This
   * could be expanded to an infinite number of files, but at the
   * (considerable) expense of remembering the file and its current
   * offset, then opening/reading/closing.  The commands' utility
   * doesn't warrant the effort; at least, to me...) 
   */

  for (filecnt = 0; (nfiles > 0); fnamptr++) {
	if ((fnamptr[0][0] == '-') && (fnamptr[0][1] != '\0')) {
		if (fnamptr[0][1] == 'd') fnamptr++;
	} else {
		nfiles--;
		if (fnamptr[0][0] == '-') {
			fileptr[filecnt++] = stdin;
		} else {
			fileptr[filecnt] = fopen(fnamptr[0], "r");
			if (fileptr[filecnt++] == NULL)
				prerr(BADFILE, *fnamptr);
		}
	}
  }

  fileptr[filecnt] = ENDLIST;	/* End of list. */

  /* Have all files.  Now, read a line from each file, and output to
   * stdout.  Notice that the old 511 character limitation on the line
   * length no longer applies, since this program doesn't do the
   * buffering.  Do this until you go through the loop and don't
   * successfully read from any of the files.
   */
  for (; filecnt;) {
	somedone = 0;		/* Blank field handling flag */
	delimptr = delims;	/* Start at beginning of delim list */
	delbuf[0] = 0;		/* No squirreled delims */

	for (index = 0; (fileptr[index] != ENDLIST) && filecnt; index++) {
		/* Read a line and immediately output. If it's too
		 * big for the buffer, then dump what was read and go
		 * back for more.
		 * 
		 * Otherwise, if it is from the last file, then leave
		 * the carriage return in place; if not, replace with
		 * a delimiter (if any) 
 		 */

		strend = 0;	/* Set so can easily detect EOF */

		if (fileptr[index] != CLOSED)
			while (fgets(iobuff, (_MAXSZ - 1), 
					fileptr[index]) != NULL) {
				strend = strlen(iobuff);/* Did the buf fill? */

				if (strend == (_MAXSZ - 1)) {
					/* Gosh, what a long line. */
					fputs(iobuff, stdout);
					strend = 0;
					continue;
				}

				/* Ok got whole line in buffer. */
				break;	/* Out of loop for this file */
			}

		/* Ended either on an EOF (well, actually NULL
		 * return-- it *could* be some sort of file error,
		 * but but if the file was opened successfully, this
		 * is unlikely. Besides, error checking on streams
		 * doesn't allow us to decide exactly what went
		 * wrong, so I'm going to be very Unix-like and
		 * ignore it!), or a closed file, or a received line.
		 * If an EOF, close the file and mark it in the list.
		 * In any case, output the delimiter of choice. 
		 */

		if (!strend) {
			if (fileptr[index] != CLOSED) {
				fclose(fileptr[index]);
				fileptr[index] = CLOSED;
				filecnt--;
			}

			/* Is this the end of the whole thing? */
			if ((fileptr[index + 1] == ENDLIST) && !somedone)
				continue;	/* EXITS */

			/* Ok, some files not closed this line. Last file? */
			if (fileptr[index + 1] == ENDLIST) {
				if (delbuf[0]) {
					fputs(&delbuf[1], stdout);
					delbuf[0] = 0;
				}
				putc((int) NL, stdout);
				continue;	/* Next read of files */
			} else {
				/* Closed file; setup delim */
				if (*delimptr != DEL) {
					delbuf[0]++;
					delbuf[delbuf[0]] = *delimptr++;
					delbuf[delbuf[0] + 1] = '\0';
				} else
					delimptr++;
			}

			/* Reset end of delimiter string if necessary */
			if (*delimptr == '\0') delimptr = delims;
		} else {
			/* Some data read. */
			somedone++;

			/* Any saved delims? */
			if (delbuf[0]) {
				fputs(&delbuf[1], stdout);
				delbuf[0] = 0;
			}

			/* If last file, last char will be NL. */
			if (fileptr[index + 1] != ENDLIST) {
				if (*delimptr == DEL) {
					delimptr++;
					iobuff[strend - 1] = '\0';/* No delim*/
				} else
					iobuff[strend - 1] = *delimptr++;
			}
			if (*delimptr == '\0') delimptr = delims;

			/* Now dump the buffer */
			fputs(iobuff, stdout);
			fflush(stdout);
		}
	}
  }
}

void doserial(nfiles, fnamptr)
int nfiles;
char **fnamptr;
{
  /* Do serial paste.  Simply scarf characters, performing
   * one-character buffering to facilitate delim processing. 
   */

  register int charnew, charold;
  register char *delimptr;

  register FILE *fileptr;

  for (; nfiles != 0; fnamptr++) {
	if ((fnamptr[0][0] == '-') && (fnamptr[0][1] != '\0')) {
		if (fnamptr[0][1] == 'd') fnamptr++;
	} else {
		if (fnamptr[0][0] == '-') {
			fileptr = stdin;
		} else {
			fileptr = fopen(*fnamptr, "r");

			if (fileptr == NULL) prerr(BADFILE, *fnamptr);
		}

		/* The file is open; just keep taking characters,
		 * stashing them in charnew; output charold,
		 * converting to the appropriate delimiter character
		 * if needful.  After the EOF, simply output
		 * 'charold' if it's a newline; otherwise, output it
		 * and then a newline. 
		 */

		delimptr = delims;	/* Set up for delimiter string */

		if ((charold = getc(fileptr)) == EOF) {
			/* Empty file! */
			putc(NL, stdout);
			fflush(stdout);
			continue;	/* Go on to the next file */
		}

		/* Ok, 'charold' is set up.  Hit it! */

		while ((charnew = getc(fileptr)) != EOF) {
			/* Ok, process the old character */
			if (charold == NL) {
				if (*delimptr != DEL)
					putc((int) *delimptr++, stdout);

				/* Reset pointer at end of delimiter string */
				if (*delimptr == '\0') delimptr = delims;
			} else
				putc(charold, stdout);

			charold = charnew;
		}

		/* Ok, hit EOF.  Process that last character */

		putc(charold, stdout);
		if ((char) charold != NL) putc(NL, stdout);
		fflush(stdout);
		nfiles--;
	}
  }
}

void delimbuild(strptr)
char *strptr;
{
  /* Process the delimiter string into something that can be used by
   * the routines.  This involves, primarily, collapsing the backslash
   * representations of special characters into their actual values,
   * and terminating the string in a manner that the routines can
   * recognize.  The set of possible backslash characters has been
   * expanded beyond that recognized by the vanilla Unix(Tm) version. 
   */

  register char *strout;

  delims = strptr;		/* delims now points to argv[...]	 */
  strout = strptr;		/* Start at the same place, anyway */

  while (*strptr) {
	if (*strptr != '\\')	/* Is it an escape character? */
		*strout++ = *strptr++;	/* No, just transfer it */
	else {
		strptr++;	/* Get past escape character */

		switch (*strptr) {
		    case '0':	*strout++ = DEL;	break;

		    case 't':	*strout++ = TAB;	break;

		    case 'n':	*strout++ = NL;	break;

		    case 'b':	*strout++ = BS;	break;

		    case 'f':	*strout++ = FF;	break;

		    case 'r':	*strout++ = CR;	break;

		    case '\\':
			*strout++ = BACKSLASH;
			break;

		    default:	*strout++ = *strptr;
		}

		strptr++;
	}

  }
  *strout = '\0';		/* Heaven forfend that we forget this! */
}

void prerr(etype, estring)
int etype;
char *estring;
{
  switch (etype) {
      case USAGE:
	fprintf(stderr, "%s : Usage: %s [-s] [-d <delimiters>] file1 file2 ...\n", cmdnam, cmdnam);
	break;

      case NODELIM:
	fprintf(stderr, "%s : no delimiters\n", cmdnam);
	break;

      case BADFILE:
	fprintf(stderr, "%s : %s : cannot open\n", cmdnam, estring);
	break;

      case TOOMANY:
	fprintf(stderr, "%s : too many files\n", cmdnam);
	break;
  }
  exit(1);
}
