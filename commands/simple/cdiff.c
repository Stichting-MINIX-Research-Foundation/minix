/* cdiff - context diff			Author: Larry Wall */

/* Cdiff - turns a regular diff into a new-style context diff
 *
 * Usage: cdiff file1 file2
 */

#define PATCHLEVEL 2

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <limits.h>
#include <stdio.h>

char buff[512];

FILE *inputfp, *oldfp, *newfp;

int oldmin, oldmax, newmin, newmax;
int oldbeg, oldend, newbeg, newend;
int preoldmax, prenewmax;
int preoldbeg, preoldend, prenewbeg, prenewend;
int oldwanted, newwanted;

char *oldhunk, *newhunk;
char *progname;
size_t oldsize, oldalloc, newsize, newalloc;

_PROTOTYPE(int main, (int argc, char **argv));
_PROTOTYPE(void dumphunk, (void));
_PROTOTYPE(char *getold, (int targ));
_PROTOTYPE(char *getnew, (int targ));
_PROTOTYPE(void *xmalloc, (size_t size));
_PROTOTYPE(void *xrealloc, (void *ptr, size_t size));

#define Nullfp (FILE*)0
#define Nullch (char*)0
#define ENOUGH (NAME_MAX + PATH_MAX + 1)
#define CRC_END 12

int main(argc, argv)
int argc;
char **argv;
{
  FILE *crcfp;
  char *old, *new;
  int context = 3;
  struct stat statbuf;
  register char *s;
  char op;
  char *newmark, *oldmark;
  char sysbuf1[ENOUGH], sysbuf2[ENOUGH];
  int len;
  char *line;
  int i;
  int status;

  progname = argv[0];
  oldalloc = 512;
  oldhunk = (char *) xmalloc(oldalloc);
  newalloc = 512;
  newhunk = (char *) xmalloc(newalloc);

  for (argc--, argv++; argc; argc--, argv++) {
	if (argv[0][0] != '-') break;

	if (argv[0][1] == 'c') context = atoi(argv[0] + 2);
  }

  if (argc != 2) {
	fprintf(stderr, "Usage: cdiff old new\n");
	exit(2);
  }
  old = argv[0];
  new = argv[1];

  oldfp = fopen(old, "r");
  if (!oldfp) {
	fprintf(stderr, "Can't open %s\n", old);
	exit(2);
  }
  newfp = fopen(new, "r");
  if (!newfp) {
	fprintf(stderr, "Can't open %s\n", new);
	exit(2);
  }

  /* Compute crcs by popen()ing crc and reading the output.  Do this before
   * popen()ing diff to do the work.  popen() attempts to support multiple
   * clients, but the 1.3-1.6.24b versions don't succeed.
   */
  sprintf(sysbuf1, "crc %s", old);
  crcfp = popen(sysbuf1, "r");
  if (!crcfp) {
	/* The only advantage of cdiff over diff is that it prints crcs, so
	 * give up easily if crc fails.
	 */
	fprintf(stderr, "Can't execute crc %s\n", old);
	exit(2);
  }
  fgets(sysbuf1, sizeof(sysbuf1), crcfp);
  sysbuf1[CRC_END] = '\0';
  status = pclose(crcfp);
  if (status != 0) {
	fprintf(stderr, "crc %s returned bad status %d\n", old, status);
	exit(2);
  }
  sprintf(sysbuf2, "crc %s", new);
  crcfp = popen(sysbuf2, "r");
  if (!crcfp) {
	fprintf(stderr, "Can't execute crc %s\n", new);
	exit(2);
  }
  fgets(sysbuf2, sizeof(sysbuf2), crcfp);
  sysbuf2[CRC_END] = '\0';
  status = pclose(crcfp);
  if (status != 0) {
	fprintf(stderr, "crc %s returned bad status %d\n", new, status);
	exit(2);
  }

  sprintf(buff, "diff %s %s 2>/dev/null", old, new);
  inputfp = popen(buff, "r");
  if (!inputfp) {
	fprintf(stderr, "Can't execute diff %s %s\n", old, new);
	exit(2);
  }

  fstat(fileno(oldfp), &statbuf);
  printf("*** %s  crc=%s\t%s", old, sysbuf1, ctime(&statbuf.st_mtime));
  fstat(fileno(newfp), &statbuf);
  printf("--- %s  crc=%s\t%s", new, sysbuf2, ctime(&statbuf.st_mtime));

  preoldend = -1000;

  while (fgets(buff, sizeof buff, inputfp) != Nullch) {
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
			fprintf(stderr, "Unparseable input: %s\n", s);
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
			fprintf(stderr, "Unparseable input: %s\n", s);
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
		oldbeg = oldmin - context;
		oldend = oldmax + context;
		if (oldbeg < 1) oldbeg = 1;
		newbeg = newmin - context;
		newend = newmax + context;
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

  if (preoldend >= 0) {
	dumphunk();
  }
  status = pclose(inputfp);
  if (!WIFEXITED(status)) exit(2);
  status = WEXITSTATUS(status);
  return(status == 0 || status == 1 ? status : 2);
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
  printf("***************\n");
  if (preoldbeg >= preoldend) {
	printf("*** %d ****\n", preoldend);
  } else {
	printf("*** %d,%d ****\n", preoldbeg, preoldend);
  }
  if (oldwanted) {
	printf("%s", oldhunk);
  }
  oldsize = 0;
  *oldhunk = '\0';
  if (prenewbeg >= prenewend) {
	printf("--- %d ----\n", prenewend);
  } else {
	printf("--- %d,%d ----\n", prenewbeg, prenewend);
  }
  if (newwanted) {
	printf("%s", newhunk);
  }
  newsize = 0;
  *newhunk = '\0';
}

char *getold(targ)
int targ;
{
  static int oldline = 0;

  while (fgets(buff, sizeof buff, oldfp) != Nullch) {
	oldline++;
	if (oldline == targ) return buff;
  }
  return Nullch;
}

char *getnew(targ)
int targ;
{
  static int newline = 0;

  while (fgets(buff, sizeof buff, newfp) != Nullch) {
	newline++;
	if (newline == targ) return buff;
  }
  return Nullch;
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
