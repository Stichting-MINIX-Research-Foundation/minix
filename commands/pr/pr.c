/* pr - print files			Author: Michiel Huisjes */

/* Pr - print files
 *
 * Author: Michiel Huisjes.
 * Modified: Jacob P. Bunschoten.	(30 nov 87)
 *	When "columns" is not given and numbering is on:
 *		line numbers are correlated with input lines.
 *	(try pr [-1] -n file )
 *	tabs are accounted for.
 *	When numbering is turned on, width know this.
 *	automatic line-folding. -f to get the original program.
 *	backspaces are accounted for. -b to disable this.
 *	multi-column mode changed.
 *	header can be given and used.
 *	format changed may occur between printing of several files:
 *		pr -l30 file1 -w75 file2
 *
 * Modified: Rick Thomas.		(Sept 12, 1988)
 *	added "-M" option to cover functionality of old "-n" option,
 *	and made "-n" option behavior compatible with system V.
 *
 * Usage: pr [+page] [-columns] [-h header] [-wwidth] [-llength] [-ntm] [files]
 *        -t : Do not print the 5 line header and trailer at the page.
 *        -n : Turn on line numbering.
 *        -M : Use "Minix" style line numbering -- Each page begins at
 *             a line number that is an even multiple of the page length.
 *             Like the listings in Appendix E of the book.
 *        +page    : Start printing at page n.
 *        -columns : Print files in n-columns.
 *        -l length: Take the length of the page to be n instead of 66
 *        -h header: Take next argument as page header.
 *        -w width  : Take the width of the page to be n instead of default 79
 *	  -f : do not fold lines.
 *
 * Modified: Lars Fredriksen		(Jan 19, 1990)
 *	fixed the program so that
 *		pr -n *.c
 *	would work. The clobal variable 'width' was decremented
 *	by NUM_WIDTH, for each file, resulting in width finally
 *	being so small that nothing was printed. Used the local
 *	variable 'w' for the width adjustment (in print())
 *
 * Modified: Kenneth J. Hendrickson	(10 April 1991)
 *	date in header should be last modification date for files,
 *	and the current time for stdin.
 *
 * Modified: Kees J. Bot		(5 October 1992)
 *	Use localtime(3) to get the date, it knows TZ.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>


#define DEF_LENGTH	66
#define DEF_WIDTH	79
#define NUM_WIDTH	8
#define TAB_WIDTH	8	/* fixed tab_width */

/* Used to compute next (fixed) tabstop */
#define TO_TAB(x)	(( (x) + TAB_WIDTH ) & ~07 )

typedef char BOOL;

#define FALSE		0
#define TRUE		1

/* EAT:	eat rest of input line */
#define EAT(fp)		while((c=getc(fp))!='\n' && c!=EOF)

/* L_BUF: calculate address of pointer to char (string) used in format */
#define L_BUF(i,j)	* (char **) (line_buf + (i + j*length)*sizeof(char *))

char *header;
BOOL no_header;
BOOL number = FALSE;
BOOL minix_number = FALSE;
BOOL ext_header_set = FALSE;	/* external header given */
BOOL back_space = TRUE;		/* back space correction in line width */
BOOL dont_fold = FALSE;		/* original. If the line does not fit eat it. */
short columns;
short cwidth;
short start_page = 1;
short width = DEF_WIDTH;
short length = DEF_LENGTH;
short linenr;
char *line_buf;			/* used in format for multi-column output */

char output[1024];

int main(int argc, char **argv);
static char *myalloc(size_t size);
char skip_page(int lines, int width, FILE * filep);
void format(FILE * filep);
void print_page(int pagenr, int maxcol);
void print(FILE * filep);
void out_header(int page);
void print_time(time_t t);

int main(argc, argv)
int argc;
char *argv[];
{
  FILE *file;
  char *ptr;
  int index = 1;		/* index is one ahead of argc */
  int line, col;

  setbuf(stdout, output);
  do {
	if (argc == index)	/* No arguments (left) */
		goto pr_files;

	ptr = argv[index++];
	if (*ptr == '+') {
		start_page = atoi(++ptr);
		continue;
	}
	if (*ptr != '-') {	/* no flags */
		index--;
		goto pr_files;
	}
	if (*++ptr >= '0' && *ptr <= '9') {
		columns = atoi(ptr);
		if (columns <= 0) columns = 1;
		continue;	/* Fetch next flag */
	}
	while (*ptr) switch (*ptr++) {
		    case 't':	no_header = TRUE;	break;
		    case 'n':
			number = TRUE;
			minix_number = FALSE;
			break;
		    case 'M':
			number = TRUE;
			minix_number = TRUE;
			break;
		    case 'h':
			header = argv[index++];
			ext_header_set = TRUE;
			break;
		    case 'w':
			if ((width = atoi(ptr)) <= 0) width = DEF_WIDTH;
			*ptr = '\0';
			break;
		    case 'l':
			if ((length = atoi(ptr)) <= 0) length = DEF_LENGTH;
			*ptr = '\0';
			break;
		    case 'b':	/* back_space correction off */
			back_space = FALSE;
			break;
		    case 'f':	/* do not fold lines */
			dont_fold = TRUE;
			break;
		    default:
			fprintf(stderr, "Usage: %s [+page] [-columns] [-h header] [-w<width>] [-l<length>] [-nMt] [files]\n", argv[0]);
			exit(1);
		}
	continue;		/* Scan for next flags */


	/* ==============  flags are read. Print the file(s) ========= */

pr_files:

	if (!no_header) length -= 10;

	if (columns) {
		cwidth = width / columns + 1;
		if (columns > width) {
			fprintf(stderr, "Too many columns for page width.\n");
			exit(1);
		}

		/* Allocate piece of mem to hold some pointers */
		line_buf = myalloc(length * columns * sizeof(char *));
	}
	for (line = 0; line < length; line++)
		for (col = 0; col < columns; col++)
			L_BUF(line, col) = NULL;

	if (length <= 0) {
		fprintf(stderr, "Minimal length should be %d\n", no_header ?
			1 : 11);
		exit(1);
	}
	while (index <= argc) {	/* print all files, including stdin */
		if (index < argc && (*argv[index] == '-' || *argv[index] == '+'))
			break;	/* Format change */

		if (argc == index) {	/* no file specified, so stdin */
			if (!ext_header_set) header = "";
			file = stdin;
		} else {
			if ((file = fopen(argv[index], "r")) == (FILE *) 0) {
				fprintf(stderr, "Cannot open %s\n", argv[index++]);
				continue;
			}
			if (!ext_header_set) header = argv[index];
		}
		if (columns)
			format(file);
		else
			print(file);
		fclose(file);
		if (++index >= argc)
			break;	/* all files (including stdin) done */
	}
	if (index >= argc) break;
	/* When control comes here. format changes are to be done.
	 * reinitialize some variables */
	if (!no_header) length += 10;

	start_page = 1;
	ext_header_set = FALSE;
	if (columns) free(line_buf);
  } while (index <= argc);	/* "pr -l60" should work too */

  (void) fflush(stdout);
  return(0);
}

char skip_page(lines, width, filep)
int lines, width;
FILE *filep;
{
  short c;
  int char_cnt;
  int w;

  do {
	w = width;
	if (number)		/* first lines are shorter */
		if (!columns ||	/* called from print(file)  */
		    !(lines % columns))	/* called from format(file) */
			w -= NUM_WIDTH;

	char_cnt = 0;
	while ((c = getc(filep)) != '\n' && c != EOF && char_cnt < w) {
		/* Calculate if this line is longer than "width (w)"
		 * characters */
		if (c == '\b' && back_space) {
			if (--char_cnt < 0) char_cnt = 0;
		} else if (c == '\t')
			char_cnt = TO_TAB(char_cnt);
		else
			char_cnt++;
	}
	if (dont_fold && c != '\n' && c != EOF) EAT(filep);
	lines--;
	if (c == '\n') linenr++;
  } while (lines > 0 && c != EOF);

  return c;			/* last char read */
}

void format(filep)
FILE *filep;
{
  char buf[512];
  short c = '\0';
  short index, lines, i;
  short page_number = 0;
  short maxcol = columns;
  short wdth;
  short line, col;

  do {
	/* Check printing of page */
	page_number++;

	if (page_number < start_page && c != EOF) {
		c = (char) skip_page(columns * length, cwidth, filep);
		continue;
	}
	if (c == EOF) return;

	lines = columns * length;
	for (line = 0; line < length; line++)
		for (col = 0; col < columns; col++) {
			if (L_BUF(line, col) != NULL)
				free(L_BUF(line, col));
			L_BUF(line, col) = (char *) NULL;
		}
	line = 0;
	col = 0;
	do {
		index = 0;
		wdth = cwidth - 1;
		if (number && !col)	/* need room for numbers */
			wdth -= NUM_WIDTH;

		/* Intermidiate colums are shortened by 1 char */
		/* Last column not */
		if (col + 1 == columns) wdth++;
		for (i = 0; i < wdth - 1; i++) {
			c = getc(filep);
			if (c == '\n' || c == EOF) break;

			if (c == '\b' && back_space) {
				buf[index++] = '\b';
				if (--i < 0) {	/* just in case ... */
					i = 0;
					index = 0;
				}
			} else if (c == '\t') {
				int cnt, max;

				max = TO_TAB(i);
				for (cnt = i; cnt < max; cnt++)
					buf[index++] = ' ';
				i = max - 1;
			} else
				buf[index++] = (char) c;
		}
		buf[index++] = '\0';
		/* Collected enough chars (or eoln, or EOF) */

		/* First char is EOF */
		if (i == 0 && lines == columns * length && c == EOF) return;

		/* Alloc mem to hold this (sub) string */
		L_BUF(line, col) = myalloc(index * sizeof(char));
		strcpy(L_BUF(line, col), buf);

		line++;
		line %= length;
		if (line == 0) {
			col++;
			col %= columns;
		}
		if (dont_fold && c != '\n' && c != EOF) EAT(filep);
		lines--;	/* line ready for output */
		if (c == EOF) {
			maxcol = columns - lines / length;
		}
	} while (c != EOF && lines);
	print_page(page_number, maxcol);
  } while (c != EOF);
}

void print_page(pagenr, maxcol)
short pagenr, maxcol;
{
  short pad, i, j;
  short width;
  char *p;

  if (minix_number)
	linenr = (pagenr - 1) * length + 1;
  else
	linenr = 1;

  if (!no_header) out_header(pagenr);

  for (i = 0; i < length; i++) {
	for (j = 0; j < maxcol; j++) {
		width = cwidth;
		if (number && j == 0) {	/* first columns */
			printf("%7.7d ", linenr++);	/* 7 == NUM_WIDTH-1 */
			width -= NUM_WIDTH;
		}
		pad = 0;
		if (p = (char *) L_BUF(i, j))
			for (; pad < width - 1 && *p; pad++) putchar(*p++);
		if (j < maxcol - 1) while (pad++ < width - 1)
				putchar(' ');
	}
	putchar('\n');
  }
  if (!no_header) printf("\n\n\n\n\n");
}

void print(filep)
FILE *filep;
{
  short c = '\0';
  short page_number = 0;
  short lines;
  short cnt;
  short w = width;
  BOOL pr_number = TRUE;	/* only real lines are numbered, not folded
			 * parts */

  linenr = 1;
  if (number) w -= NUM_WIDTH;

  do {
	/* Check printing of page */
	page_number++;

	if (page_number < start_page && c != EOF) {
		pr_number = FALSE;
		c = skip_page(length, w, filep);
		if (c == '\n') pr_number = TRUE;
		continue;
	}
	if (c == EOF) return;

	if (minix_number) linenr = (page_number - 1) * length + 1;

	if (page_number == start_page) c = getc(filep);

	/* Print the page */
	lines = length;
	while (lines && c != EOF) {
		if (lines == length && !no_header) out_header(page_number);
		if (number)
			if (pr_number)
				printf("%7.7d ", linenr++);	/* 7 == NUM_WIDTH-1 */
			else
				printf("%7c ", ' ');	/* 7 == NUM_WIDTH-1 */
		pr_number = FALSE;
		cnt = 0;
		while (c != '\n' && c != EOF && cnt < w) {
			if (c == '\t') {
				int i, max;
				max = TO_TAB(cnt);
				for (i = cnt; i < max; i++) putchar(' ');
				cnt = max - 1;
			} else if (c == '\b' && back_space) {
				putchar('\b');
				cnt--;
			} else
				putchar(c);
			c = getc(filep);
			cnt++;
		}
		putchar('\n');
		if (dont_fold && c != '\n' && c != EOF) EAT(filep);
		lines--;
		if (c == '\n') {
			c = getc(filep);
			pr_number = TRUE;
		}
	}
	if (lines == length)	/* We never printed anything on this
				 * page --  */
		return;		/* even the header, so dont try to fill it up */
	if (!no_header)		/* print the trailer -- 5 blank lines */
		printf("\n\n\n\n\n");
  } while (c != EOF);

  /* Fill last page */
  if (page_number >= start_page) {
	while (lines--) putchar('\n');
  }
}

static char *myalloc(size)
size_t size;			/* How many bytes */
{
  void *ptr;

  ptr = malloc(size);
  if (ptr == NULL) {
	fprintf(stderr, "malloc returned NULL\n");
	exit(1);
  }
  return(char *) ptr;
}

void out_header(page)
short page;
{
  time_t t;
  struct stat buf;

  if (strlen(header)) {
	stat(header, &buf);	/* use last modify time for file */
	t = buf.st_mtime;
  } else
	(void) time(&t);	/* use current time for stdin */
  print_time(t);
  printf("  %s   Page %d\n\n\n", header, page);
}

char *moname[] = {
	  "Jan", "Feb", "Mar", "Apr", "May", "Jun",
	  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

/* Print the date. */
void print_time(t)
time_t t;
{
  struct tm *tm;

  tm = localtime(&t);

  printf("\n\n%s %2d %2d:%02d %d",
         moname[tm->tm_mon],
         tm->tm_mday,
         tm->tm_hour,
         tm->tm_min,
         1900 + tm->tm_year
	);
}
