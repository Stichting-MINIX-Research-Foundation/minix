/* grep - search a file for a pattern	Author: Norbert Schlenker */

/* Norbert Schlenker (nfs@princeton.edu)  1990-02-08
 * Released into the public domain.
 *
 * Grep searches files for lines containing a pattern, as specified by
 * a regular expression, and prints those lines.  It is invoked by:
 *	grep [flags] [pattern] [file ...]
 *
 * Flags:
 *	-e pattern	useful when pattern begins with '-'
 *	-c		print a count of lines matched
 *	-i		ignore case
 *	-l		prints just file names, no lines (quietly overrides -n)
 *	-n		printed lines are preceded by relative line numbers
 *	-s		prints errors only (quietly overrides -l and -n)
 *	-v		prints lines which don't contain the pattern
 *
 * Semantic note:
 * 	If both -l and -v are specified, grep prints the names of those
 *	files which do not contain the pattern *anywhere*.
 *
 * Exit:
 *	Grep sets an exit status which can be tested by the caller.
 *	Note that these settings are not necessarily compatible with
 *	any other version of grep, especially when -v is specified.
 *	Possible status values are:
 *	  0	if any matches are found
 *	  1	if no matches are found
 *	  2	if syntax errors are detected or any file cannot be opened
 */


/* External interfaces */
#include <sys/types.h>
#include <regexp.h>		/* Thanks to Henry Spencer */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

/* Internal constants */
#define MATCH		0	/* exit code: some match somewhere */
#define NO_MATCH	1	/* exit code: no match on any line */
#define FAILURE		2	/* exit code: syntax error or bad file name */

/* Macros */
#define SET_FLAG(c)	(flags[(c)-'a'] = 1)
#define FLAG(c)		(flags[(c)-'a'] != 0)

#define uppercase(c)	(((unsigned) ((c) - 'A')) <= ('Z' - 'A'))
#define downcase(c)	((c) - 'A' + 'a')

/* Private storage */
static char *program;		/* program name */
static char flags[26];		/* invocation flags */
static regexp *expression;	/* compiled search pattern */
static char *rerr;              /* error message */

/* External variables. */
extern int optind;
extern char *optarg;

/* Internal interfaces */
_PROTOTYPE(int main, (int argc, char **argv));
_PROTOTYPE(static int match, (FILE *input, char *label, char *filename));
_PROTOTYPE(static char *get_line, (FILE *input));
_PROTOTYPE(static char *map_nocase, (char *line));
_PROTOTYPE(void regerror , (char *s ) );
_PROTOTYPE(static void tov8, (char *v8pattern, char *pattern));

int main(argc, argv)
int argc;
char *argv[];
{
  int opt;			/* option letter from getopt() */
  int egrep=0;			/* using extended regexp operators */
  char *pattern;		/* search pattern */
  char *v8pattern;		/* v8 regexp search pattern */
  int exit_status = NO_MATCH;	/* exit status for our caller */
  int file_status;		/* status of search in one file */
  FILE *input;			/* input file (if not stdin) */

  program = argv[0];
  if (strlen(program)>=5 && strcmp(program+strlen(program)-5,"egrep")==0) egrep=1;
  memset(flags, 0, sizeof(flags));
  pattern = NULL;

/* Process any command line flags. */
  while ((opt = getopt(argc, argv, "e:cilnsv")) != EOF) {
	if (opt == '?')
		exit_status = FAILURE;
	else
	if (opt == 'e')
		pattern = optarg;
	else
		SET_FLAG(opt);
  }

/* Detect a few problems. */
  if ((exit_status == FAILURE) || (optind == argc && pattern == NULL)) {
	fprintf(stderr,"Usage: %s [-cilnsv] [-e] expression [file ...]\n",program);
	exit(FAILURE);
  }

/* Ensure we have a usable pattern. */
  if (pattern == NULL)
	pattern = argv[optind++];

/* Map pattern to lowercase if -i given. */
  if (FLAG('i')) {
	char *p;
	for (p = pattern; *p != '\0'; p++) {
		if (uppercase(*p))
			*p = downcase(*p);
	}
  }

  if (!egrep) {
	  if ((v8pattern=malloc(2*strlen(pattern)))==(char*)0) {
		fprintf(stderr,"%s: out of memory\n");
		exit(FAILURE);
	  }
	  tov8(v8pattern,pattern);
  } else v8pattern=pattern;

  rerr=(char*)0;
  if ((expression = regcomp(v8pattern)) == NULL) {
	fprintf(stderr,"%s: bad regular expression",program);
	if (rerr) fprintf(stderr," (%s)",rerr);
	fprintf(stderr,"\n");
	exit(FAILURE);
  }

/* Process the files appropriately. */
  if (optind == argc) {		/* no file names - find pattern in stdin */
	exit_status = match(stdin, (char *) NULL, "<stdin>");
  }
  else 
  if (optind + 1 == argc) {	/* one file name - find pattern in it */
	if (strcmp(argv[optind], "-") == 0) {
		exit_status = match(stdin, (char *) NULL, "-");
	} else {
		if ((input = fopen(argv[optind], "r")) == NULL) {
			fprintf(stderr, "%s: couldn't open %s\n",
							program, argv[optind]);
			exit_status = FAILURE;
		}
		else {
			exit_status = match(input, (char *) NULL, argv[optind]);
		}
	}
  }
  else
  while (optind < argc) {	/* lots of file names - find pattern in all */
	if (strcmp(argv[optind], "-") == 0) {
		file_status = match(stdin, "-", "-");
	} else {
		if ((input = fopen(argv[optind], "r")) == NULL) {
			fprintf(stderr, "%s: couldn't open %s\n",
							program, argv[optind]);
			exit_status = FAILURE;
		} else {
			file_status = match(input, argv[optind], argv[optind]);
			fclose(input);
		}
	}
	if (exit_status != FAILURE)
		exit_status &= file_status;
	++optind;
  }
  return(exit_status);
}


/* match - matches the lines of a file with the regular expression.
 * To improve performance when either -s or -l is specified, this
 * function handles those cases specially.
 */

static int match(input, label, filename)
FILE *input;
char *label;
char *filename;
{
  char *line, *testline;	/* pointers to input line */
  long int lineno = 0;		/* line number */
  long int matchcount = 0;	/* lines matched */
  int status = NO_MATCH;	/* summary of what was found in this file */

  if (FLAG('s') || FLAG('l')) {
	while ((line = get_line(input)) != NULL) {
		testline = FLAG('i') ? map_nocase(line) : line;
		if (regexec(expression, testline, 1)) {
			status = MATCH;
			break;
		}
	}
	if (FLAG('l'))
		if ((!FLAG('v') && status == MATCH) ||
		    ( FLAG('v') && status == NO_MATCH))
			puts(filename);
	return status;
  }

  while ((line = get_line(input)) != NULL) {
	++lineno;
	testline = FLAG('i') ? map_nocase(line) : line;
	if (regexec(expression, testline, 1)) {
		status = MATCH;
		if (!FLAG('v')) {
			if (label != NULL)
				printf("%s:", label);
			if (FLAG('n'))
				printf("%ld:", lineno);
			if (!FLAG('c')) puts(line);
			matchcount++;
		}
	} else {
		if (FLAG('v')) {
			if (label != NULL)
				printf("%s:", label);
			if (FLAG('n'))
				printf("%ld:", lineno);
			if (!FLAG('c')) puts(line);
			matchcount++;
		}
	}
  }
  if (FLAG('c')) printf("%ld\n", matchcount);
  return status;
}


/* get_line - fetch a line from the input file
 * This function reads a line from the input file into a dynamically
 * allocated buffer.  If the line is too long for the current buffer,
 * attempts will be made to increase its size to accomodate the line.
 * The trailing newline is stripped before returning to the caller.
 */

#define FIRST_BUFFER (size_t)256		/* first buffer size */

static char *buf = NULL;	/* input buffer */
static size_t buf_size = 0;		/* input buffer size */

static char *get_line(input)
FILE *input;
{
  int n;
  register char *bp;
  register int c;
  char *new_buf;
  size_t new_size;

  if (buf_size == 0) {
	if ((buf = (char *) malloc(FIRST_BUFFER)) == NULL) {
		fprintf(stderr,"%s: not enough memory\n",program);
		exit(FAILURE);
	}
	buf_size = FIRST_BUFFER;
  }

  bp = buf;
  n = buf_size;
  while (1) {
	while (--n > 0 && (c = getc(input)) != EOF) {
		if (c == '\n') {
			*bp = '\0';
			return buf;
		}
		*bp++ = c;
	}
	if (c == EOF)
		return (ferror(input) || bp == buf) ? NULL : buf;
	new_size = buf_size << 1;
	if ((new_buf = (char *) realloc(buf, new_size)) == NULL) {
		fprintf(stderr, "%s: line too long - truncated\n", program);
		while ((c = getc(input)) != EOF && c != '\n') ;
		*bp = '\0';
		return buf;
	} else {
		bp = new_buf + (buf_size - 1);
		n = buf_size + 1;
		buf = new_buf;
		buf_size = new_size;
	}
  }
}


/* map_nocase - map a line down to lowercase letters only.
 * bad points:	assumes line gotten from get_line.
 *		there is more than A-Z you say?
 */

static char *map_nocase(line)
char *line;
{
  static char *mapped=(char*)0;
  static size_t map_size = 0;
  char *mp;

  if (map_size < buf_size) {
	if ((mapped=realloc(mapped,map_size=buf_size)) == NULL) {
		fprintf(stderr,"%s: not enough memory\n",program);
		exit(FAILURE);
	}
  }

  mp = mapped;
  do {
	*mp++ = uppercase(*line) ? downcase(*line) : *line;
  } while (*line++ != '\0');

  return mapped;
}

/* In basic regular expressions, the characters ?, +, |, (, and )
   are taken literally; use the backslashed versions for RE operators.
   In v8 regular expressions, things are the other way round, so
   we have to swap those characters and their backslashed versions.
*/
static void tov8(char *v8, char *basic)
{
  while (*basic) switch (*basic)
  {
    case '?':
    case '+':
    case '|':
    case '(':
    case ')':        
    {        
      *v8++='\\';
      *v8++=*basic++;
      break;
    }                    
    case '\\':
    {
      switch (*(basic+1))
      {
        case '?':
        case '+':
        case '|':
        case '(': 
        case ')':
        {
          *v8++=*++basic;
          ++basic;
          break;
        }
        case '\0':
        {
          *v8++=*basic++;
          break;
        }
        default:
        {       
          *v8++=*basic++;
          *v8++=*basic++;
        }                
      }     
      break;
    }       
    default:
    {
      *v8++=*basic++;
    }                
  }   
  *v8++='\0';
}

/* Regular expression code calls this routine to print errors. */

void regerror(s)
char *s;
{
  rerr=s;
}
