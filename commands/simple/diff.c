/* diff  - print differences between 2 files	  Author: Erik Baalbergen */

/* Poor man's implementation of diff(1) 	- no options available
* 	- may give more output than other diffs,
*	  due to the straight-forward algorithm
* 	- runs out of memory if the differing chunks become too large
* 	- input line length should not exceed LINELEN; longer lines are
*	  truncated, while only the first LINELEN characters are compared
*
* 	- Bug fixes by Rick Thomas Sept. 1989
*
* Please report bugs and suggestions to erikb@cs.vu.nl
*------------------------------------------------------------------------------
* Changed diff to conform to POSIX 1003.2 ( Draft 11) by Thomas Brupbacher
* ( tobr@mw.lpc.ethz.ch).
*
* To incorporate the context diff option -c in the program, the source code
* for the program cdiff has been copied to the end of this program. Only
* slight modifications for the cdiff code to work within the program diff
* were made( e.g. main() -> context_diff()).
*
* New options:
* -c, -C n where n=0,1,...:
*  	produces a context diff as the program cdiff. The default is to
*  	print 3 lines of context, this value can be changed with -C
*	( e.g. -C 5 prints five lines of context.)
* -e :	Prints an ed script, so you can convert <file1> to <file2> with
*  	the command ed <file1> < `diff -e <file1> <file2>`.
* -b :	Causes trailing blanks to be ignored and spaces of multiple blanks
*  	to be reduced to one blank before comparison.
*-----------------------------------------------------------------------------
*/

#include <stdlib.h>
#include <limits.h>		/* NAME_MAX for maximal filename length	 */
#include <string.h>		/* string manipulation			 */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <ctype.h>
#include <time.h>
#include <dirent.h>
#include <unistd.h>
#include <stdio.h>

/* These definitions are needed only to suppress warning messages. */
#define Nullfp 		((FILE*)0)
#define Nullch 		((char*)0)
#define NullStructLine	((struct line *)0)

#define LINELEN 128		/* max line length included in diff	 */


#define NOT_SET 0		/* Defines to characterise if a flag 	 */
#define SET	1		/* is set				 */

 /* Indexes of the warning-message array	 */
#define EXCLUSIVE_OPTIONS	0
#define CANNOT_OPEN_FILE	1

 /* Used to define the mode 		 */
typedef enum {
  undefined, context, ed_mode
} MODE;

 /* Global variables for the 'normal' diff part	 */
char *progname;			/* program name	(on command line)	 */
int diffs = 0;			/* number of differences		 */
MODE mode;			/* which mode is used			 */
int severe_error;		/* nonzero after severe, non-fatal error */

/* The following global variables are used with the -r option:
 * for every pair of files that are different, a "command line" of the
 * form "diff <options> <oldfile> <newfile>" is printed before the real
 * output starts.							 */
int firstoutput = 1;		/* flag to print one time		 */
char options_string[10];	/* string to hold command line options 	 */
char oldfile[PATH_MAX];		/* first file				 */
char newfile[PATH_MAX];		/* second file				 */


 /* Global variables for the command-line options */
int trim_blanks = NOT_SET;	/* SET if -b specified	 		 */
int recursive_dir = NOT_SET;	/* SET if -r specified	 		 */
int context_lines = 3;		/* numbers of lines in a context	 */
static int offset;		/* offset of the actual line number for -e */

 /* Function prototypes for the functions in this file	 */
struct f;
_PROTOTYPE(int main, (int argc, char **argv ));
_PROTOTYPE(void process_command_line, (int argc, char **argv ));
_PROTOTYPE(void analyse_input_files, (char *arg1, char *arg2, char *input1, 
							char *input2 ));
_PROTOTYPE(void diff, (char *filename1, char *filename2 ));
_PROTOTYPE(FILE *check_file, (char *name ));
_PROTOTYPE(void build_option_string, (void ));
_PROTOTYPE(void fatal_error, (char *fmt, char *s ));
_PROTOTYPE(void warn, (int number, char *string ));
_PROTOTYPE(void trimming_blanks, (char *l_text ));
_PROTOTYPE(char *filename, (char *path_string));
_PROTOTYPE(struct line *new_line, (int size ));
_PROTOTYPE(void free_line, (struct line *l ));
_PROTOTYPE(int equal_line, (struct line *l1, struct line *l2 ));
_PROTOTYPE(int equal_3, (struct line *l1, struct line *l2 ));
_PROTOTYPE(struct line *read_line, (FILE *fp ));
_PROTOTYPE(void advance, (struct f *f ));
_PROTOTYPE(void aside, (struct f *f, struct line *l ));
_PROTOTYPE(struct line *next, (struct f *f ));
_PROTOTYPE(void init_f, (struct f *f, FILE *fp ));
_PROTOTYPE(void update, (struct f *f, char *s ));
_PROTOTYPE(void __diff, (FILE *fp1, FILE *fp2 ));
_PROTOTYPE(void differ, (struct f *f1, struct f *f2 ));
_PROTOTYPE(int wlen, (struct f *f ));
_PROTOTYPE(void range, (int a, int b ));
_PROTOTYPE(void cdiff, (char *old, char *new, FILE *file1, FILE *file2 ));
_PROTOTYPE(void dumphunk, (void ));
_PROTOTYPE(char *getold, (int targ ));
_PROTOTYPE(char *getnew, (int targ ));
_PROTOTYPE(int isdir, (char *path ));
_PROTOTYPE(void diff_recursive, (char *dir1, char *dir2 ));
_PROTOTYPE(void file_type_error, (char *filename1, char *filename2, 
			struct stat *statbuf1, struct stat *statbuf2 ));
_PROTOTYPE(void *xmalloc, (size_t size));
_PROTOTYPE(void *xrealloc, (void *ptr, size_t size));

int main(argc, argv)
int argc;
char **argv;
{
  char file1[PATH_MAX], file2[PATH_MAX];
  extern int optind;		/* index of the current string in argv	 */

  progname = argv[0];
  process_command_line(argc, argv);

  analyse_input_files(argv[optind], argv[optind + 1], file1, file2);
  optind++;

  if (recursive_dir == SET) {
	build_option_string();
	diff_recursive(file1, file2);
  } else {
	diff(file1, file2);
  }

  return(severe_error ? 2 : diffs > 0 ? 1 : 0);
}

/* Process the command line and set the flags for the different
 * options. the processing of the command line is done with the
 * getopt() library function. a minimal error processing is done
 * for the number of command line arguments.				 */
void process_command_line(argc, argv)
int argc;			/* number of arguments on command line	 */
char **argv;			/* ** to arguments on command line	 */
{
  int c;
  extern char *optarg;		/* points to string with options	 */
  extern int optind;		/* index of the current string in argv	 */

  /* Are there enough arguments?		 */
  if (argc < 3) {
	fatal_error("Usage: %s [-c|-e|-C n][-br] file1 file2\n", progname);
  }

  /* Process all options using getopt()	 */
  while ((c = getopt(argc, argv, "ceC:br")) != -1) {
	switch (c) {
	    case 'c':
		if (mode != undefined) warn(EXCLUSIVE_OPTIONS, "c");
		mode = context;
		context_lines = 3;
		break;
	    case 'e':
		if (mode != undefined) warn(EXCLUSIVE_OPTIONS, "e");
		mode = ed_mode;
		break;
	    case 'C':
		if (mode != undefined) warn(EXCLUSIVE_OPTIONS, "C");
		mode = context;
		context_lines = atoi(optarg);
		break;
	    case 'b':	trim_blanks = SET;	break;
	    case 'r':	recursive_dir = SET;	break;
	    case '?':
		exit(2);
	}
  }

  /* We should have two arguments left	 */
  if ((argc - optind) != 2)
	fatal_error("Need exactly two input file-names!\n", "");
}

/* Analyse_input_files takes the two input files on the command line
 * and decides what to do. returns the (corrected) filenames that
 * can be used to call diff().
 * if two directories are given, then a recursive diff is done.
 * one directory and one filename compares the file with <filename>
 * in the directory <directory> with <filename>.
 * if two filenames are specified, no special action takes place.
 */
void analyse_input_files(arg1, arg2, input1, input2)
char *arg1, *arg2;		/* filenames on the command line	 */
char *input1, *input2;		/* filenames to be used with diff()	 */
{
  int stat1 = 0, stat2 = 0;

  if (strcmp(arg1, "-") != 0)
	stat1 = isdir(arg1);	/* != 0 <-> arg1 is directory		 */
  if (strcmp(arg2, "-") != 0) stat2 = isdir(arg2);
#ifdef DEBUG
  fprintf(stderr, "%s, stat = %d\n", arg1, stat1);
  fprintf(stderr, "%s, stat = %d\n", arg2, stat2);
#endif
  if (stat1 && stat2) {		/* both arg1 and arg2 are directories */
	recursive_dir = SET;
	strcpy(input1, arg1);
	strcpy(input2, arg2);
	return;
  }
  if (stat1 != 0) {		/* arg1 is a dir, arg2 not		 */
	if (strcmp(arg2, "-") != 0) {	/* arg2 != stdin	 */
		strcpy(input1, arg1);
		strcat(input1, "/");
		strcat(input1, arg2);
		strcpy(input2, arg2);
		return;
	} else {
		fatal_error("cannot compare stdin (-) with a directory!", "");
	}
  }
  if (stat2 != 0) {		/* arg2 is a dir, arg1 not		 */
	if (strcmp(arg1, "-") != 0) {	/* arg1 != stdin	 */
		strcpy(input1, arg1);
		strcpy(input2, arg2);
		strcat(input2, "/");
		strcat(input2, arg1);
		return;
	} else {		/* arg1 == stdin			 */
		fatal_error("cannot compare stdin (-) with a directory!", "");
	}
  }

  /* Both arg1 and arg2 are normal  files	 */
  strcpy(input1, arg1);
  strcpy(input2, arg2);
}

/* Diff() is the front end for all modes of the program diff, execpt
 * the recursive_dir option.
 * diff() expects the filenames of the two files to be compared as
 * arguments. the mode is determined from the global variable mode.
 */
void diff(filename1, filename2)
char *filename1, *filename2;
{
  FILE *file1 = check_file(filename1);
  FILE *file2 = check_file(filename2);
  struct stat statbuf1, statbuf2;

  if ((file1 != Nullfp) && (file2 != Nullfp)) {
	/* If we do a recursive diff, then we don't compare block
	 * special, character special or FIFO special files to any
	 * file.			  */
	fstat(fileno(file1), &statbuf1);
	fstat(fileno(file2), &statbuf2);
	if ((((statbuf1.st_mode & S_IFREG) != S_IFREG) ||
	     ((statbuf2.st_mode & S_IFREG) != S_IFREG)) &&
	    (recursive_dir == SET)) {
		file_type_error(filename1, filename2, &statbuf1, &statbuf2);
	} else {
		switch (mode) {
		    case context:
			cdiff(filename1, filename2, file1, file2);
			break;
		    case ed_mode:
		    case undefined:
			__diff(file1, file2);
			if (mode == ed_mode) printf("w\n");
			break;
		}
	}
  } else
	severe_error = 1;
  if (file1 != Nullfp) fclose(file1);
  if (file2 != Nullfp) fclose(file2);
}

/* Check_file() opens the fileptr with name <filename>. if <filename>
 * equals "-" stdin is associated with the return value.
 */
FILE *check_file(name)
char *name;
{
  FILE *temp;

  if (strcmp(name, "-") == 0) {
	return(stdin);
  } else {
	temp = fopen(name, "r");
	if (temp == Nullfp) warn(CANNOT_OPEN_FILE, name);
	return(temp);
  }
}

/* Build_option_string() is called before recursive_dir() is called
 * from the main() function. its purpose is to build the string that
 * is used on the command line to get the current operation mode.
 * e.g. "-C 6 -b".
 */
void build_option_string()
{
  switch (mode) {
	    case ed_mode:sprintf(options_string, "-e");
	break;
      case context:
	if (context_lines == 3)
		sprintf(options_string, "-c");
	else
		sprintf(options_string, "-C %d", context_lines);
	break;
  }

}


/* The fatal error handler.
 * Expects a format string and a string as arguments. The arguments
 * are printed to stderr and the program exits with an error code 2.
 */
void fatal_error(fmt, s)
char *fmt;			/* the format sttring to be printed	 */
char *s;			/* string to be inserted into the format
				 * string				 */
{
  fprintf(stderr, "%s: ", progname);
  fprintf(stderr, fmt, s);
  fprintf(stderr, "\n");
  exit(2);
}

/* This function prints non fatal error messages to stderr.
 * Expects the index of the message to be printed and a pointer
 * to the (optional) string to be printed.
 * Returns no value.
 */
void warn(number, string)
int number;			/* index of the warning			 */
char *string;			/* string to be inserted to the warning	 */
{
  static char *warning[] = {
    "%s: The options -c, -e, -C n are mutually exclusive! Assuming -%c\n",
    "%s: cannot open file %s for reading\n",
  };
  fprintf(stderr, warning[number], progname, string);
}

/* Function used with the optione -b, trims the blanks in a input line:
 * - blanks between words are reduced to one
 * - trailing blanks are eliminated.
 */
void trimming_blanks(l_text)
char *l_text;			/* begin of the char array		 */
{
  char *line = l_text;
  char *copy_to, *copy_from;

  do {
	if (*line == ' ') {
		copy_from = line;
		copy_to = line;
		while (*(++copy_from) == ' ');
		if (*copy_from != '\n') copy_to++;
		while (*copy_from != '\0') *(copy_to++) = *(copy_from++);
		*copy_to = '\0';
	}
  } while (*(++line) != '\0');
}


/* Filename separates the filename and the relative path in path_string.
 * Returns the filename with a leading /
 */
char *filename(path_string)
char *path_string;
{
  char name[NAME_MAX + 2];	/* filename plus /		 	 */
  char *ptr;

  name[0] = '/';
  ptr = strrchr(path_string, '/');

  if (ptr == 0) {		/* no / in path_string, only a filename	 */
	strcat(name, path_string);
  } else {
	strcat(name, ptr);
  }

  return(name);
}

/* The line module: one member in a linked list of lines. */
struct line {
  struct line *l_next;		/* pointer to the next line	 */
  char l_eof;			/* == 0 if last line in file	 */
  char *l_text;			/* array with the text		 */
};

struct line *freelist = 0;
#define stepup(ll) ( ((ll) && ((ll)->l_eof==0)) ? (ll)->l_next : (ll) )

/* Function to allocate space for a new line containing SIZE chars	*/
struct line *new_line(size)
int size;
{
  register struct line *l;

  if ((l = freelist) != NullStructLine)
	freelist = freelist->l_next;
  else {
	l = (struct line *) xmalloc(3 * sizeof(void *));
	l->l_text = (char *) xmalloc((size + 2) * sizeof(char));
	if ((l == 0) || (l->l_text == 0)) fatal_error("Out of memory", "");
  }
  return l;
}


/* Free_line() releases storage allocated for <l>. */
void free_line(l)
register struct line *l;
{
  l->l_next = freelist;
  freelist = l;
}

/* Equal_line() compares two lines, <l1> and <l2>.
 * the returned value is the result of the strcmp() function.
 */
int equal_line(l1, l2)
struct line *l1, *l2;
{
  if (l1 == 0 || l2 == 0)
	return(0);
  else if (l1->l_eof || l2->l_eof)
	return(l1->l_eof == l2->l_eof);
  else
	return(strcmp(l1->l_text, l2->l_text) == 0);
}

int equal_3(l1, l2)
struct line *l1, *l2;
{
  register int i, ansr;

  ansr = 1;
#ifdef DEBUG
  if (l1 == 0)
	fprintf(stderr, "\t(null)\n");
  else if (l1->l_eof)
	fprintf(stderr, "\t(eof)\n");
  else
	fprintf(stderr, "\t%s", l1->l_text);
  if (l2 == 0)
	fprintf(stderr, "\t(null)\n");
  else if (l2->l_eof)
	fprintf(stderr, "\t(eof)\n");
  else
	fprintf(stderr, "\t%s", l2->l_text);
#endif
  for (i = 0; i < 3; ++i) {
	if (!equal_line(l1, l2)) {
		ansr = 0;
		break;
	}
	l1 = stepup(l1);
	l2 = stepup(l2);
  }
#ifdef DEBUG
  fprintf(stderr, "\t%d\n", ansr);
#endif
  return(ansr);
}

struct line *
 read_line(fp)
FILE *fp;
{
  register struct line *l = new_line(LINELEN);
  register char *p;
  register int c;

  (p = &(l->l_text[LINELEN]))[1] = '\377';
  l->l_eof = 0;
  if (fgets(l->l_text, LINELEN + 2, fp) == 0) {
	l->l_eof = 1;
	l->l_text[0] = 0;
  } else if ((p[1] & 0377) != 0377 && *p != '\n') {
	while ((c = fgetc(fp)) != '\n' && c != EOF) {
	}
	*p++ = '\n';
	*p = '\0';
  }
  l->l_next = 0;
  if (trim_blanks == SET) {
#ifdef DEBUG
	printf("xxx %s xxx\n", l->l_text);
#endif
	trimming_blanks(l->l_text);
#ifdef DEBUG
	printf("xxx %s xxx\n", l->l_text);
#endif
  }
  return l;
}

/* File window handler */
struct f {
  struct line *f_bwin, *f_ewin;
  struct line *f_aside;
  int f_linecnt;		/* line number in file of last advanced line */
  FILE *f_fp;
};

void advance(f)
register struct f *f;
{
  register struct line *l;

  if ((l = f->f_bwin) != NullStructLine) {
	if (f->f_ewin == l)
		f->f_bwin = f->f_ewin = 0;
	else
		f->f_bwin = l->l_next;
	free_line(l);
	(f->f_linecnt)++;
  }
}

void aside(f, l)
struct f *f;
struct line *l;
{
  register struct line *ll;

  if (l == 0) return;
  if ((ll = l->l_next) != NullStructLine) {
	while (ll->l_next) ll = ll->l_next;
	ll->l_next = f->f_aside;
	f->f_aside = l->l_next;
	l->l_next = 0;
	f->f_ewin = l;
  }
}


struct line *next(f)
register struct f *f;
{
  register struct line *l;

  if ((l = f->f_aside) != NullStructLine) {
	f->f_aside = l->l_next;
	l->l_next = 0;
  } else
	l = read_line(f->f_fp);
  if (l) {
	if (f->f_bwin == 0)
		f->f_bwin = f->f_ewin = l;
	else {
		if (f->f_ewin->l_eof && l->l_eof) {
			free_line(l);
			return(f->f_ewin);
		}
		f->f_ewin->l_next = l;
		f->f_ewin = l;
	}
  }
  return l;
}


/* Init_f() initialises a window structure (struct f). <fp> is the
 * file associated with <f>.
 */
void init_f(f, fp)
register struct f *f;
FILE *fp;
{
  f->f_bwin = f->f_ewin = f->f_aside = 0;
  f->f_linecnt = 0;
  f->f_fp = fp;
}


/* Update() prints a window. <f> is a pointer to the window, <s> is the
 * string containing the "prefix" to the printout( either "<" or ">").
 * after completion of update(), the window is empty.
 */
void update(f, s)
register struct f *f;
char *s;
{
  char *help;
  int only_dot = 0;

  if (firstoutput && (recursive_dir == SET)) {
	printf("diff %s %s %s\n", options_string, oldfile, newfile);
	firstoutput = 0;
  }
  while (f->f_bwin && f->f_bwin != f->f_ewin) {
	if (mode != ed_mode) {
		printf("%s%s", s, f->f_bwin->l_text);
	} else {
#ifdef DEBUG
		printf("ed_mode: test for only dot");
		printf("%s", f->f_bwin->l_text);
#endif
		help = f->f_bwin->l_text;
		while ((*help == ' ') ||
		       (*help == '.') ||
		       (*help == '\t')) {
			if (*(help++) == '.') only_dot++;
			if (only_dot > 1) break;
		}

		/* If only_dot is equal 1, there is only one dot on
		 * the line, so we have to take special actions.
		 * f the line with only one dot is found, we output
		 * two dots (".."), terminate the append modus and
		 * substitute "." for "..". Afterwards we restart
		 * with the append command.			 */
		if (*help == '\n' && only_dot == 1) {
			help = f->f_bwin->l_text;
			while (*help != '\0') {
				if (*help == '.') printf(".");
				putchar((int) *(help++));
			}
			printf(".\n");
			printf(".s/\\.\\././\n");
			printf("a\n");
		} else {
			printf("%s%s", s, f->f_bwin->l_text);
		}
	}
	advance(f);
  }
}

/* __Diff(), performs the "core operation" of the program.
 * Expects two file-pointers as arguments. This functions does
 * *not* check if the file-pointers are valid.
 */

void __diff(fp1, fp2)
FILE *fp1, *fp2;
{
  struct f f1, f2;
  struct line *l1, *s1, *b1, *l2, *s2, *b2;
  register struct line *ll;

  init_f(&f1, fp1);
  init_f(&f2, fp2);
  l1 = next(&f1);
  l2 = next(&f2);
  while ((l1->l_eof == 0) || (l2->l_eof == 0)) {
	if (equal_line(l1, l2)) {
  equal:
		advance(&f1);
		advance(&f2);
		l1 = next(&f1);
		l2 = next(&f2);
		continue;
	}
	s1 = b1 = l1;
	s2 = b2 = l2;
	/* Read several more lines */
	next(&f1);
	next(&f1);
	next(&f2);
	next(&f2);
	/* Start searching */
search:
	next(&f2);
	ll = s1;
	do {
		if (equal_3(ll, b2)) {
			l1 = ll;
			l2 = b2;
			aside(&f1, ll);
			aside(&f2, b2);
			differ(&f1, &f2);
			goto equal;
		}
		if (ll->l_eof) break;
		ll = stepup(ll);
	} while (ll);
	b2 = stepup(b2);

	next(&f1);
	ll = s2;
	do {
		if (equal_3(b1, ll)) {
			l1 = b1;
			l2 = ll;
			aside(&f2, ll);
			aside(&f1, b1);
			differ(&f1, &f2);
			goto equal;
		}
		if (ll->l_eof != 0) break;
		ll = stepup(ll);
	} while (ll);
	b1 = stepup(b1);

	goto search;
  }

  /* Both of the files reached EOF */
}

/* Differ() prints the differences between files. the arguments <f1> and
 * <f2> are pointers to the two windows, where the differences are.
 */
void differ(f1, f2)
register struct f *f1, *f2;
{
  int cnt1 = f1->f_linecnt, len1 = wlen(f1);
  int cnt2 = f2->f_linecnt, len2 = wlen(f2);
  if ((len1 != 0) || (len2 != 0)) {
	if (len1 == 0) {
		if (mode == ed_mode) {
			cnt1 += offset;
			printf("%d a\n", cnt1);
			update(f2, "");
			printf(".\n");
			offset += len2;
		} else {
			printf("%da", cnt1);
			range(cnt2 + 1, cnt2 + len2);
		}
	} else if (len2 == 0) {
		if (mode == ed_mode) {
			cnt1 += offset;
			range(cnt1 + 1, cnt1 + len1);
			printf("d\n");
			offset -= len1;
			while (f1->f_bwin && f1->f_bwin != f1->f_ewin)
				advance(f1);
		} else {
			range(cnt1 + 1, cnt1 + len1);
			printf("d%d", cnt2);
		}
	} else {
		if (mode != ed_mode) {
			range(cnt1 + 1, cnt1 + len1);
			putchar('c');
			range(cnt2 + 1, cnt2 + len2);
		} else {
			cnt1 += offset;
			if (len1 == len2) {
				range(cnt1 + 1, cnt1 + len1);
				printf("c\n");
				update(f2, "");
				printf(".\n");
			} else {
				range(cnt1 + 1, cnt1 + len1);
				printf("d\n");
				printf("%d a\n", cnt1);
				update(f2, "");
				printf(".\n");
				offset -= len1 - len2;
			}
			while (f1->f_bwin && f1->f_bwin != f1->f_ewin)
				advance(f1);
		}
	}
	if (mode != ed_mode) {
		putchar('\n');
		if (len1 != 0) update(f1, "< ");
		if ((len1 != 0) && (len2 != 0)) printf("---\n");
		if (len2 != 0) update(f2, "> ");
	}
	diffs++;
  }
}


/* Function wlen() calculates the number of lines in a window. */
int wlen(f)
struct f *f;
{
  register cnt = 0;
  register struct line *l = f->f_bwin, *e = f->f_ewin;

  while (l && l != e) {
	cnt++;
	l = l->l_next;
  }
  return cnt;
}


/* Range() prints the line numbers of a range. the arguments <a> and <b>
 * are the beginning and the ending line number of the range. if
 * <a> == <b>, only one line number is printed. otherwise <a> and <b> are
 * separated by a ",".
 */
void range(a, b)
int a, b;
{
  printf(((a == b) ? "%d" : "%d,%d"), a, b);
}

/* Here follows the code for option -c.
 * This code is from the cdiff program by Larry Wall. I changed it only
 * slightly to reflect the POSIX standard and to call the main routine
 * as function context_diff().
 */

/* Cdiff - context diff			Author: Larry Wall */

/* These global variables are still here from the original cdiff program...
 * I was to lazy just to sort them out...
 */
char buff[512];
FILE *oldfp, *newfp;

int oldmin, oldmax, newmin, newmax;
int oldbeg, oldend, newbeg, newend;
int preoldmax, prenewmax;
int preoldbeg, preoldend, prenewbeg, prenewend;
int oldwanted, newwanted;

char *oldhunk, *newhunk;
size_t oldsize, oldalloc, newsize, newalloc;

int oldline, newline; /* Jose */

void cdiff(old, new, file1, file2)
char *old, *new;		/* The names of the two files to be compared */
FILE *file1, *file2;		/* The corresponding file-pointers	 */
{
  FILE *inputfp;
  struct stat statbuf;
  register char *s;
  char op;
  char *newmark, *oldmark;
  int len;
  char *line;
  int i, status;

  oldfp = file1;
  newfp = file2;

  oldalloc = 512;
  oldhunk = (char *) xmalloc(oldalloc);
  newalloc = 512;
  newhunk = (char *) xmalloc(newalloc);


/* The context diff spawns a new process that executes a normal diff
 * and parses the output.
 */
  if (trim_blanks == SET)
	sprintf(buff, "diff -b %s %s", old, new);
  else
	sprintf(buff, "diff %s %s", old, new);

  inputfp = popen(buff, "r");
  if (!inputfp) {
	fprintf(stderr, "Can't execute diff %s %s\n", old, new);
	exit(2);
  }
  preoldend = -1000;
  firstoutput = 1;
  oldline = newline = 0;
  while (fgets(buff, sizeof buff, inputfp) != Nullch) {
	if (firstoutput) {
		if (recursive_dir == SET) {
			printf("diff %s %s %s\n", options_string,
			       oldfile, newfile);
		}
		fstat(fileno(oldfp), &statbuf);
		printf("*** %s %s", old, ctime(&statbuf.st_mtime));
		fstat(fileno(newfp), &statbuf);
		printf("--- %s %s", new, ctime(&statbuf.st_mtime));
		firstoutput = 0;
	}
	if (isdigit(*buff)) {
		oldmin = atoi(buff);
		for (s = buff; isdigit(*s); s++);
		if (*s == ',') {
			s++;
			oldmax = atoi(s);
			for (; isdigit(*s); s++);
		} else {
			oldmax = oldmin;
		}
		if (*s != 'a' && *s != 'd' && *s != 'c') {
			fprintf(stderr, "Unparseable input: %s", s);
			exit(2);
		}
		op = *s;
		s++;
		newmin = atoi(s);
		for (; isdigit(*s); s++);
		if (*s == ',') {
			s++;
			newmax = atoi(s);
			for (; isdigit(*s); s++);
		} else {
			newmax = newmin;
		}
		if (*s != '\n' && *s != ' ') {
			fprintf(stderr, "Unparseable input: %s", s);
			exit(2);
		}
		newmark = oldmark = "! ";
		if (op == 'a') {
			oldmin++;
			newmark = "+ ";
		}
		if (op == 'd') {
			newmin++;
			oldmark = "- ";
		}
		oldbeg = oldmin - context_lines;
		oldend = oldmax + context_lines;
		if (oldbeg < 1) oldbeg = 1;
		newbeg = newmin - context_lines;
		newend = newmax + context_lines;
		if (newbeg < 1) newbeg = 1;

		if (preoldend < oldbeg - 1) {
			if (preoldend >= 0) {
				dumphunk();
			}
			preoldbeg = oldbeg;
			prenewbeg = newbeg;
			oldwanted = newwanted = 0;
			oldsize = newsize = 0;
		} else {	/* we want to append to previous hunk */
			oldbeg = preoldmax + 1;
			newbeg = prenewmax + 1;
		}

		for (i = oldbeg; i <= oldmax; i++) {
			line = getold(i);
			if (!line) {
				oldend = oldmax = i - 1;
				break;
			}
			len = strlen(line) + 2;
			if (oldsize + len + 1 >= oldalloc) {
				oldalloc *= 2;
				oldhunk = (char *) xrealloc(oldhunk, oldalloc);
			}
			if (i >= oldmin) {
				strcpy(oldhunk + oldsize, oldmark);
				oldwanted++;
			} else {
				strcpy(oldhunk + oldsize, "  ");
			}
			strcpy(oldhunk + oldsize + 2, line);
			oldsize += len;
		}
		preoldmax = oldmax;
		preoldend = oldend;

		for (i = newbeg; i <= newmax; i++) {
			line = getnew(i);
			if (!line) {
				newend = newmax = i - 1;
				break;
			}
			len = strlen(line) + 2;
			if (newsize + len + 1 >= newalloc) {
				newalloc *= 2;
				newhunk = (char *) xrealloc(newhunk, newalloc);
			}
			if (i >= newmin) {
				strcpy(newhunk + newsize, newmark);
				newwanted++;
			} else {
				strcpy(newhunk + newsize, "  ");
			}
			strcpy(newhunk + newsize + 2, line);
			newsize += len;
		}
		prenewmax = newmax;
		prenewend = newend;
	}
  }
  status = pclose(inputfp);
  if (status != 0) diffs++;
  if (!WIFEXITED(status) || WEXITSTATUS(status) > 1) severe_error = 1;

  if (preoldend >= 0) {
	dumphunk();
  }
}

void dumphunk()
{
  int i;
  char *line;
  int len;

  for (i = preoldmax + 1; i <= preoldend; i++) {
	line = getold(i);
	if (!line) {
		preoldend = i - 1;
		break;
	}
	len = strlen(line) + 2;
	if (oldsize + len + 1 >= oldalloc) {
		oldalloc *= 2;
		oldhunk = (char *) xrealloc(oldhunk, oldalloc);
	}
	strcpy(oldhunk + oldsize, "  ");
	strcpy(oldhunk + oldsize + 2, line);
	oldsize += len;
  }
  for (i = prenewmax + 1; i <= prenewend; i++) {
	line = getnew(i);
	if (!line) {
		prenewend = i - 1;
		break;
	}
	len = strlen(line) + 2;
	if (newsize + len + 1 >= newalloc) {
		newalloc *= 2;
		newhunk = (char *) xrealloc(newhunk, newalloc);
	}
	strcpy(newhunk + newsize, "  ");
	strcpy(newhunk + newsize + 2, line);
	newsize += len;
  }
  fputs("***************\n", stdout);
  if (preoldbeg >= preoldend) {
	printf("*** %d ****\n", preoldend);
  } else {
	printf("*** %d,%d ****\n", preoldbeg, preoldend);
  }
  if (oldwanted) {
	fputs(oldhunk, stdout);
  }
  oldsize = 0;
  *oldhunk = '\0';
  if (prenewbeg >= prenewend) {
	printf("--- %d ----\n", prenewend);
  } else {
	printf("--- %d,%d ----\n", prenewbeg, prenewend);
  }
  if (newwanted) {
	fputs(newhunk, stdout);
  }
  newsize = 0;
  *newhunk = '\0';
}

char *getold(targ)
int targ;
{
  while (fgets(buff, sizeof buff, oldfp) != Nullch) {
	oldline++;
	if (oldline == targ) return buff;
  }
  return Nullch;
}

char *getnew(targ)
int targ;
{
  while (fgets(buff, sizeof buff, newfp) != Nullch) {
	newline++;
	if (newline == targ) return buff;
  }
  return Nullch;
}


/* Isdir() checks, if <path> is the name of a directory. a return value
 * is 0, <path> is a normal file. otherwise the <path> is a directory.
 */
int isdir(path)
char *path;
{
  struct stat buf;
  stat(path, &buf);
  if (buf.st_mode & S_IFDIR) {	/* path is a directory		 */
	return(~0);
  } else {
	return(0);
  }
}



/* This is the "main" function if a diff of two directories has to be
 * done. diff_recursive() expects the names of the two directories to
 * be compared. 							 */
void diff_recursive(dir1, dir2)
char *dir1, *dir2;
{
  FILE *ls1, *ls2;
  char file1[PATH_MAX], file2[PATH_MAX];
  char jointfile1[PATH_MAX], jointfile2[PATH_MAX];
  char command[PATH_MAX];
  int difference, eof1, eof2;

  sprintf(command, "ls %s", dir1);
  ls1 = popen(command, "r");
  sprintf(command, "ls %s", dir2);
  ls2 = popen(command, "r");

  if ((ls1 == NULL) || (ls2 == NULL))
	fatal_error("cannot execute ls!", "");

  file1[0] = '\0';
  eof1 = fscanf(ls1, "%s\n", file1);
  file2[0] = '\0';
  eof2 = fscanf(ls2, "%s\n", file2);

  while ((file1[0] != '\0') && (file2[0] != '\0')) {
	difference = strcmp(file1, file2);
	while (difference != 0) {
		if (difference < 0) {
			printf("Only in %s: %s\n", dir1, file1);
			file1[0] = '\0';
			eof1 = fscanf(ls1, "%s\n", file1);
			if (file1[0] == '\0') break;
		} else {
			printf("Only in %s: %s\n", dir2, file2);
			file2[0] = '\0';
			eof2 = fscanf(ls2, "%s\n", file2);
			if (file2[0] == '\0') break;
		}
		difference = strcmp(file1, file2);
	}
	if (eof1 != EOF && eof2 != EOF) {
		strcpy(jointfile1, dir1);
		strcat(jointfile1, "/");
		strcat(jointfile1, file1);
		strcpy(jointfile2, dir2);
		strcat(jointfile2, "/");
		strcat(jointfile2, file2);

		if ((isdir(jointfile1) != 0) && (isdir(jointfile2) != 0)) {
			printf("Common subdirectories: %s and %s\n",
			       jointfile1, jointfile2);
			diff_recursive(jointfile1, jointfile2);
		} else {
			firstoutput = 1;
			strcpy(oldfile, jointfile1);
			strcpy(newfile, jointfile2);
			diff(jointfile1, jointfile2);
		}
		file1[0] = '\0';
		eof1 = fscanf(ls1, "%s\n", file1);
		file2[0] = '\0';
		eof2 = fscanf(ls2, "%s\n", file2);
	}
  }

  if (file1[0] != '\0') {	/* first arg still has files 		 */
	do {
		printf("Only in %s: %s\n", dir1, file1);
		eof1 = fscanf(ls1, " %s\n", file1);
	} while (eof1 != EOF);
  }
  if (file2[0] != '\0') {
	do {
		printf("Only in %s: %s\n", dir2, file2);
		eof2 = fscanf(ls2, " %s\n", file2);
	} while (eof2 != EOF);
  }
  if (pclose(ls1) != 0) severe_error = 1;
  if (pclose(ls2) != 0) severe_error = 1;
}


/* File_type_error is called, if in a recursive diff ( -r) one of the two
 * files a block special, a character special or a FIFO special file is.
 * The corresponding error message is printed here.			  */
void file_type_error(filename1, filename2, statbuf1, statbuf2)
char *filename1, *filename2;
struct stat *statbuf1, *statbuf2;
{
  char type1[25], type2[25];

  switch (statbuf1->st_mode & S_IFMT) {	/* select only file mode */
      case S_IFREG:
	sprintf(type1, "regular file ");
	break;
      case S_IFBLK:
	sprintf(type1, "block special file ");
	break;
      case S_IFDIR:	sprintf(type1, "directory ");	break;
      case S_IFCHR:
	sprintf(type1, "character special file ");
	break;
      case S_IFIFO:
	sprintf(type1, "FIFO special file ");
	break;
  }

  switch (statbuf2->st_mode & S_IFMT) {	/* select only file mode */
      case S_IFREG:
	sprintf(type2, "regular file ");
	break;
      case S_IFBLK:
	sprintf(type2, "block special file ");
	break;
      case S_IFDIR:	sprintf(type2, "directory ");	break;
      case S_IFCHR:
	sprintf(type2, "character special file ");
	break;
      case S_IFIFO:
	sprintf(type2, "FIFO special file ");
	break;
  }
  printf("File %s is a %s while file %s is a %s\n",
         filename1, type1, filename2, type2);
}

void *xmalloc(size)
size_t size;
{
  void *ptr;

  ptr = malloc(size);
  if (ptr == NULL) {
	fprintf(stderr, "%s: out of memory\n", progname);
	exit(2);
  }
  return(ptr);
}

void *xrealloc(ptr, size)
void *ptr;
size_t size;
{
  ptr = realloc(ptr, size);
  if (ptr == NULL) {
	fprintf(stderr, "%s: out of memory\n", progname);
	exit(2);
  }
  return(ptr);
}
