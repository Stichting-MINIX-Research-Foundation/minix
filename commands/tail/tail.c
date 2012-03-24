/* tail - copy the end of a file	Author: Norbert Schlenker */

/*   Syntax:	tail [-f] [-c number | -n number] [file]
 *		tail -[number][c|l][f] [file]		(obsolescent)
 *		tail +[number][c|l][f] [file]		(obsolescent)
 *   Flags:
 *	-c number	Measure starting point in bytes.  If number begins
 *			with '+', the starting point is relative to the
 *			the file's beginning.  If number begins with '-'
 *			or has no sign, the starting point is relative to
 *			the end of the file.
 *	-f		Keep trying to read after EOF on files and FIFOs.
 *	-n number	Measure starting point in lines.  The number
 *			following the flag has significance similar to
 *			that described for the -c flag.
 *
 *   If neither -c nor -n are specified, the default is tail -n 10.
 *
 *   In the obsolescent syntax, an argument with a 'c' following the
 *   (optional) number is equivalent to "-c number" in the standard
 *   syntax, with number including the leading sign ('+' or '-') of the
 *   argument.  An argument with 'l' following the number is equivalent
 *   to "-n number" in the standard syntax.  If the number is not
 *   specified, 10 is used as the default.  If neither 'c' nor 'l' are
 *   specified, 'l' is assumed.  The character 'f' may be suffixed to
 *   the argument and is equivalent to specifying "-f" in the standard
 *   syntax.  Look for lines marked "OBSOLESCENT".
 *
 *   If no file is specified, standard input is assumed. 
 *
 *   P1003.2 does not specify tail's behavior when a count of 0 is given.
 *   It also does not specify clearly whether the first byte (line) of a
 *   file should be numbered 0 or 1.  Historical behavior is that the
 *   first byte is actually number 1 (contrary to all Unix standards).
 *   Historically, a count of 0 (or -0) results in no output whatsoever,
 *   while a count of +0 results in the entire file being copied (just like
 *   +1).  The implementor does not agree with these behaviors, but has
 *   copied them slavishly.  Look for lines marked "HISTORICAL".
 *   
 *   Author:    Norbert Schlenker
 *   Copyright: None.  Released to the public domain.
 *   Reference: P1003.2 section 4.59 (draft 10)
 *   Notes:	Under Minix, this program requires chmem =30000.
 *   Bugs:	No internationalization support; all messages are in English.
 */

/* Force visible Posix names */
#ifndef _POSIX_SOURCE
#define _POSIX_SOURCE 1
#endif

/* External interfaces */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>

/* External interfaces that should have been standardized into <getopt.h> */
extern char *optarg;
extern int optind;

/* We expect this constant to be defined in <limits.h> in a Posix program,
 * but we'll specify it here just in case it's been left out.
 */
#ifndef LINE_MAX
#define LINE_MAX 2048		/* minimum acceptable lower bound */
#endif

/* Magic numbers suggested or required by Posix specification */
#define SUCCESS	0		/* exit code in case of success */
#define FAILURE 1		/*                   or failure */
#define DEFAULT_COUNT 10	/* default number of lines or bytes */
#define MIN_BUFSIZE (LINE_MAX * DEFAULT_COUNT)
#define SLEEP_INTERVAL	1	/* sleep for one second intervals with -f */

#define FALSE 0
#define TRUE 1

/* Internal functions - prototyped under Minix */
int main(int argc, char **argv);
int tail(int count, int bytes, int read_until_killed);
int keep_reading(void);
void usage(void);

int main(argc, argv)
int argc;
char *argv[];
{
  int cflag = FALSE;
  int nflag = FALSE;
  int fflag = FALSE;
  int number = -DEFAULT_COUNT;
  char *suffix;
  int opt;
  struct stat stat_buf;

/* Determining whether this invocation is via the standard syntax or
 * via an obsolescent one is a nasty kludge.  Here it is, but there is
 * no pretense at elegance.
 */
  if (argc == 1) {		/* simple:  default read of a pipe */
	exit(tail(-DEFAULT_COUNT, 0, fflag));
  }
  if ((argv[1][0] == '+') ||	/* OBSOLESCENT */
      (argv[1][0] == '-' && ((isdigit(argv[1][1])) ||
			     (argv[1][1] == 'l') ||
			     (argv[1][1] == 'c' && argv[1][2] == 'f')))) {
	--argc; ++argv;
	if (isdigit(argv[0][1])) {
		number = (int)strtol(argv[0], &suffix, 10);
		if (number == 0) {		/* HISTORICAL */
			if (argv[0][0] == '+')
				number = 1;
			else
				exit(SUCCESS);
		}
	} else {
		number = (argv[0][0] == '+') ? DEFAULT_COUNT : -DEFAULT_COUNT;
		suffix = &(argv[0][1]);
	}
	if (*suffix != '\0') {
		if (*suffix == 'c') {
			cflag = TRUE;
			++suffix;
		}
		else
		if (*suffix == 'l') {
			nflag = TRUE;
			++suffix;
		}
	}
	if (*suffix != '\0') {
		if (*suffix == 'f') {
			fflag = TRUE;
			++suffix;
		}
	}
	if (*suffix != '\0') {	/* bad form: assume to be a file name */
		number = -DEFAULT_COUNT;
		cflag = nflag = FALSE;
		fflag = FALSE;
	} else {
		--argc; ++argv;
	}
  } else {			/* new standard syntax */
	while ((opt = getopt(argc, argv, "c:fn:")) != EOF) {
		switch (opt) {
		      case 'c':
			cflag = TRUE;
			if (*optarg == '+' || *optarg == '-')
				number = atoi(optarg);
			else
			if (isdigit(*optarg))
				number = -atoi(optarg);
			else
				usage();
			if (number == 0) {		/* HISTORICAL */
				if (*optarg == '+')
					number = 1;
				else
					exit(SUCCESS);
			}
			break;
		      case 'f':
			fflag = TRUE;
			break;
		      case 'n':
			nflag = TRUE;
			if (*optarg == '+' || *optarg == '-')
				number = atoi(optarg);
			else
			if (isdigit(*optarg))
				number = -atoi(optarg);
			else
				usage();
			if (number == 0) {		/* HISTORICAL */
				if (*optarg == '+')
					number = 1;
				else
					exit(SUCCESS);
			}
			break;
		      default:
			usage();
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;
  }

  if (argc > 1 ||		/* too many arguments */
      (cflag && nflag)) {	/* both bytes and lines specified */
	usage();
  }

  if (argc > 0) {		/* an actual file */
	if (freopen(argv[0], "r", stdin) != stdin) {
		fputs("tail: could not open ", stderr);
		fputs(argv[0], stderr);
		fputs("\n", stderr);
		exit(FAILURE);
	}
	/* There is an optimization possibility here.  If a file is being
	 * read, we need not look at the front of it.  If we seek backwards
         * from the end, we can (potentially) avoid looking at most of the
	 * file.  Some systems fail when asked to seek backwards to a point
	 * before the start of the file, so we avoid that possibility.
	 */
	if (number < 0 && fstat(fileno(stdin), &stat_buf) == 0) {
		long offset = cflag ? (long)number : (long)number * LINE_MAX;

		if (-offset < stat_buf.st_size)
			fseek(stdin, offset, SEEK_END);
	}
  } else {
	fflag = FALSE;		/* force -f off when reading a pipe */
  }
  exit(tail(number, cflag, fflag));
  /* NOTREACHED */
}

int tail(count, bytes, read_until_killed)
int count;			/* lines or bytes desired */
int bytes;			/* TRUE if we want bytes */
int read_until_killed;		/* keep reading at EOF */
{
  int c;
  char *buf;			/* pointer to input buffer */
  char *buf_end;		/* and one past its end */
  char *start;			/* pointer to first desired character in buf */
  char *finish;			/* pointer past last desired character */
  int wrapped_once = FALSE;	/* TRUE after buf has been filled once */

/* This is magic.  If count is positive, it means start at the count'th
 * line or byte, with the first line or byte considered number 1.  Thus,
 * we want to SKIP one less line or byte than the number specified.  In
 * the negative case, we look backward from the end of the file for the
 * (count + 1)'th newline or byte, so we really want the count to be one
 * LARGER than was specified (in absolute value).  In either case, the
 * right thing to do is:
 */
  --count;

/* Count is positive:  skip the desired lines or bytes and then copy. */
  if (count >= 0) {
	while (count > 0 && (c = getchar()) != EOF) {
		if (bytes || c == '\n')
			--count;
	}
	while ((c = getchar()) != EOF) {
		if (putchar(c) == EOF)
			return FAILURE;
	}
	if (read_until_killed)
		return keep_reading();
	return ferror(stdin) ? FAILURE : SUCCESS;
  }

/* Count is negative:  allocate a reasonably large buffer. */
  if ((buf = (char *)malloc(MIN_BUFSIZE + 1)) == (char *)NULL) {
	fputs("tail: out of memory\n", stderr);
	return FAILURE;
  }
  buf_end = buf + (MIN_BUFSIZE + 1);

/* Read the entire file into the buffer. */
  finish = buf;
  while ((c = getchar()) != EOF) {
	*finish++ = c;
	if (finish == buf_end) {
		finish = buf;
		wrapped_once = TRUE;
	}
  }
  if (ferror(stdin))
	return FAILURE;

/* Back up inside the buffer.  The count has already been adjusted to
 * back up exactly one character too far, so we will bump the buffer
 * pointer once after we're done.
 * 
 * BUG: For large line counts, the buffer may not be large enough to
 *	hold all the lines.  The specification allows the program to
 *	fail in such a case - this program will simply dump the entire
 *	buffer's contents as its best attempt at the desired behavior.
 */
  if (finish != buf || wrapped_once) {		/* file was not empty */
	start = (finish == buf) ? buf_end - 1 : finish - 1;
	while (start != finish) {
		if ((bytes || *start == '\n') && ++count == 0)
			break;
		if (start == buf) {
			start = buf_end - 1;
			if (!wrapped_once)	/* never wrapped: stop now */
				break;
		} else {
			--start;
		}
	}
	if (++start == buf_end) {		/* bump after going too far */
		start = buf;
	}
	if (finish > start) {
		fwrite(start, 1, finish - start, stdout);
	} else {
		fwrite(start, 1, buf_end - start, stdout);
		fwrite(buf, 1, finish - buf, stdout);
	}
  }
  if (read_until_killed)
	return keep_reading();
  return ferror(stdout) ? FAILURE : SUCCESS;
}

/* Wake at intervals to reread standard input.  Copy anything read to
 * standard output and then go to sleep again.
 */
int keep_reading()
{
  char buf[1024];
  int n;
  int i;
  off_t pos;
  struct stat st;

  fflush(stdout);

  pos = lseek(0, (off_t) 0, SEEK_CUR);
  for (;;) {
  	for (i = 0; i < 60; i++) {
  		while ((n = read(0, buf, sizeof(buf))) > 0) {
  			if (write(1, buf, n) < 0) return FAILURE;
  		}
  		if (n < 0) return FAILURE;

		sleep(SLEEP_INTERVAL);
	}

	/* Rewind if suddenly truncated. */
	if (pos != -1) {
		if (fstat(0, &st) == -1) {
			pos = -1;
		} else
		if (st.st_size < pos) {
			pos = lseek(0, (off_t) 0, SEEK_SET);
		} else {
			pos = st.st_size;
		}
	}
  }
}

/* Tell the user the standard syntax. */
void usage()
{
  fputs("Usage: tail [-f] [-c number | -n number] [file]\n", stderr);
  exit(FAILURE);
}
