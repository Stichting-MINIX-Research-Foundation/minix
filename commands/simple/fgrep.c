/* fgrep - fast grep			Author: Bert Gijsbers */

/* Copyright (c) 1991 by Bert Gijsbers.  All rights reserved.
 * Permission to use and redistribute this software is hereby granted provided
 * that this copyright notice remains intact and that any modifications are
 * clearly marked as such.
 *
 * syntax:
 *	fgrep -chlnsv <[-e string] ... [-f file] ... | string> [file] ...
 * options:
 *	-c : print the number of matching lines
 *	-h : don't print file name headers if more than one file
 *	-l : print only the file names of the files containing a match
 *	-n : print line numbers
 *	-s : don't print, return status only
 *	-v : reverse, lines not containing one of the strings match
 *	-e string : search for this string
 *	-f file : file contains strings to search for
 * notes:
 *	Options are processed by getopt(3).
 *	Multiple strings per command line are supported, eg.
 *		fgrep -e str1 -e str2 *.c
 *	Instead of a filename - is allowed, meaning standard input.
 */

/* #include <ansi.h> */
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MAX_STR_LEN	 256	/* maximum length of strings to search for */
#define BYTE		0xFF	/* convert from char to int */
#define READ_SIZE	4096	/* read() request size */
#define BUF_SIZE (2*READ_SIZE)	/* size of buffer */

typedef struct test_str {
  struct test_str *next;	/* linked list */
  char *str;			/* string to be found */
  char *str_end;		/* points to last character */
  int len;			/* string length */
  char *bufp;			/* pointer into input buffer */
  unsigned char table[256];	/* table for Boyer-Moore algorithm */
} test_str;

test_str *strings;
char *prog_name;
int cflag, hflag, lflag, nflag, sflag, vflag;
unsigned line_num;		/* line number in current file */

int fd_in, eof_seen;		/* file descriptor for input and eof status */
char input_buffer[BUF_SIZE + 2];/* buffer + sentinel margin */
#define buffer	(&input_buffer[2])

/* Pointers into the input buffer */
char *input;			/* points to current input char */
char *max_input;		/* points to first invalid char */
char *buf_end;			/* points to first char not read */

/* Error messages */
char no_mem[] = "not enough memory";
char no_arg[] = "argument missing";

extern char *optarg;
extern int optind;

_PROTOTYPE(int main, (int argc, char **argv));
_PROTOTYPE(char *search_str, (test_str * ts));
_PROTOTYPE(int fill_buffer, (void));
_PROTOTYPE(void failure, (char *mesg));
_PROTOTYPE(void file_open, (void));
_PROTOTYPE(void usage, (void));
_PROTOTYPE(char *get_line, (void));
_PROTOTYPE(void string_file, (void));
_PROTOTYPE(void add_string, (char *str));
_PROTOTYPE(int getopt, (int argc, char **argv, char *optstring));

int main(argc, argv)
int argc;
char **argv;
{
  char *line;
  int c;
  unsigned count;		/* number of matching lines in current file */
  unsigned found_one = 0;	/* was there any match in any file at all ? */

#ifdef noperprintf
  noperprintf(stdout);
#else
  static char outbuf[BUFSIZ];

  setvbuf(stdout, outbuf, _IOFBF, sizeof outbuf);
#endif

  prog_name = argv[0];
  if (argc == 1) usage();
  while ((c = getopt(argc, argv, "ce:f:hlnsv")) != EOF) {
	switch (c) {
	    case 'c':	cflag++;	break;
	    case 'e':	add_string(optarg);	break;
	    case 'f':	string_file();	break;
	    case 'h':	hflag++;	break;
	    case 'l':	lflag++;	break;
	    case 'n':	nflag++;	break;
	    case 's':	sflag++;	break;
	    case 'v':	vflag++;	break;
	    default:	usage();	break;
	}
  }

  /* If no -e or -f option is used take a string from the command line. */
  if (strings == (test_str *) NULL) {
	if (optind == argc) failure(no_arg);
	add_string(argv[optind++]);
  }
  if (argc - optind < 2)
	hflag++;		/* don't print filenames if less than two
			 * files */

  /* Handle every matching line according to the flags. */
  do {
	optarg = argv[optind];
	file_open();
	count = 0;
	while ((line = get_line()) != (char *) NULL) {
		count++;
		if (sflag) return 0;
		if (lflag) {
			printf("%s\n", optarg);
			fflush(stdout);
			break;
		}
		if (cflag) continue;
		if (hflag == 0) printf("%s:", optarg);
		if (nflag) printf("%u:", line_num);
		do {
			putchar(*line);
		} while (++line < input);
		fflush(stdout);
	}
	found_one |= count;
	if (cflag) {
		if (hflag == 0) printf("%s: ", optarg);
		printf("%u\n", count);
		fflush(stdout);
	}
	close(fd_in);
  } while (++optind < argc);

  /* Exit nonzero if no match is found. */
  return found_one ? 0 : 1;
}

void usage()
{
  fprintf(stderr,
	"Usage: %s -chlnsv <[-e string] ... [-f file] ... | string> [file] ...\n",
	prog_name);
  exit(2);
}

void failure(mesg)
char *mesg;
{
  fprintf(stderr, "%s: %s\n", prog_name, mesg);
  exit(1);
}

/* Add a string to search for to the global linked list `strings'. */
void add_string(str)
char *str;
{
  test_str *ts;
  int len;

  if (str == (char *) NULL || (len = strlen(str)) == 0) return;
  if (len > MAX_STR_LEN) failure("string too long");
  if ((ts = (test_str *) malloc(sizeof(*ts))) == (test_str *) NULL)
	failure(no_mem);

  /* Initialize Boyer-Moore table. */
  memset(ts->table, len, sizeof(ts->table));
  ts->len = len;
  ts->str = str;
  ts->str_end = str + len - 1;
  for (; --len >= 0; str++) ts->table[*str & BYTE] = len;

  /* Put it on the list */
  ts->next = strings;
  strings = ts;
}

/* Open a file for reading.  Initialize input buffer pointers. */
void file_open()
{
  /* Use stdin if no file arguments are given on the command line. */
  if (optarg == (char *) NULL || strcmp(optarg, "-") == 0) {
	fd_in = 0;
	optarg = "stdin";
  } else if ((fd_in = open(optarg, O_RDONLY)) == -1) {
	fprintf(stderr, "%s: can't open %s\n", prog_name, optarg);
	exit(1);
  }
  input = max_input = buf_end = buffer;
  buffer[-1] = '\n';		/* sentinel */
  eof_seen = 0;
  line_num = 0;
}

/* Move any leftover characters to the head of the buffer.
 * Read characters into the rest of the buffer.
 * Round off the available input to whole lines.
 * Return the number of valid input characters.
 */
int fill_buffer()
{
  char *bufp;
  int size;

  if (eof_seen) return 0;

  size = buf_end - max_input;
  memmove(buffer, max_input, size);
  bufp = &buffer[size];

  do {
	if ((size = read(fd_in, bufp, READ_SIZE)) <= 0) {
		if (size != 0) failure("read error");
		eof_seen++;
		if (bufp == buffer)	/* no input left */
			return 0;
		/* Make sure the last char of a file is '\n'. */
		*bufp++ = '\n';
		break;
	}
	bufp += size;
  } while (bufp - buffer < READ_SIZE && bufp[-1] != '\n');

  buf_end = bufp;
  while (*--bufp != '\n');
  if (++bufp == buffer) {
	/* Line too long. */
	*buf_end++ = '\n';
	bufp = buf_end;
  }
  max_input = bufp;
  input = buffer;

  return max_input - buffer;
}

/* Read strings from a file.  Give duplicates to add_string(). */
void string_file()
{
  char *str, *p;

  file_open();
  while (input < max_input || fill_buffer() > 0) {
	p = (char *) memchr(input, '\n', BUF_SIZE);
	*p++ = '\0';
	if ((str = (char *) malloc(p - input)) == (char *) NULL)
		failure(no_mem);
	memcpy(str, input, p - input);
	add_string(str);
	input = p;
  }
  close(fd_in);
}

/* Scan the rest of the available input for a string using Boyer-Moore.
 * Return a pointer to the match or a pointer beyond end of input if no match.
 * Record how far the input is scanned.
 */
char *search_str(ts)
test_str *ts;
{
  char *bufp, *prevbufp, *s;

  bufp = input + ts->len - 1;
  while (bufp < max_input) {
	prevbufp = bufp;
	bufp += ts->table[*bufp & BYTE];
	if (bufp > prevbufp) continue;
	s = ts->str_end;
	do {
		if (s == ts->str) {	/* match found */
			ts->bufp = bufp;
			return bufp;
		}
	} while (*--bufp == *--s);
	bufp = prevbufp + 1;
  }
  ts->bufp = bufp;

  return bufp;
}

/* Return the next line in which one of the strings occurs.
 * Or, if the -v option is used, the next line without a match.
 * Or NULL on EOF.
 */
char *get_line()
{
  test_str *ts;
  char *match, *line;

  /* Loop until a line is found. */
  while (1) {
	if (input >= max_input && fill_buffer() == 0) {	/* EOF */
		line = (char *) NULL;
		break;
	}

	/* If match is still equal to max_input after the next loop
	 * then no match is found. */
	match = max_input;
	ts = strings;
	do {
		if (input == buffer) {
			if (search_str(ts) < match) match = ts->bufp;
		} else if (ts->bufp < match) {
			if (ts->bufp >= input || search_str(ts) < match)
				match = ts->bufp;
		}
	} while ((ts = ts->next) != (test_str *) NULL);

	/* Determine if and in what line a match is found. Only do
	 * line number counting if it is necessary or very easy. */
	if (vflag) {
		line_num++;
		line = input;
		input = 1 + (char *) memchr(line, '\n', BUF_SIZE);
		if (input <= match) break;	/* no match in current line */
	} else if (nflag) {
		do {
			line_num++;
			line = input;
			input = 1 + (char *) memchr(line, '\n', BUF_SIZE);
		} while (input < match ||
			 (input == match && match < max_input));
		if (match < max_input) break;	/* match found */
	} else if (match < max_input) {
		/* Match found. */
		for (line = match; *--line != '\n';);
		line++;
		input = 1 + (char *) memchr(match, '\n', BUF_SIZE);
		break;
	} else
		input = max_input;
  }

  return line;
}
