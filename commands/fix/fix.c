/* fix file difflist - update file from difflist     Author: Erik Baalbergen */


/* Notes: files old and old.patch are equal after the following commands
     diff old new > difflist
     patch old difflist > old.patch
   * the diff output is assumed to be produced by my diff program.
   * the difflist has the following form:
     difflist ::= chunk*
     chunk ::= append | delete | change ;
     append ::= n1 'a' n2 [',' n3]? '\n' ['> ' line '\n'](n3 - n2 + 1)
     delete ::= n1 [',' n2]? 'd' n3 '\n' ['< ' line '\n'](n2 - n1 + 1)
     change ::= n1 [',' n2]? 'c' n3 [',' n4]? '\n'
	      ['< ' line '\n'](n2 - n1 + 1)
	      '---\n'
	      ['> ' line '\n'](n4 - n3 + 1)
     where
     - n[1234] is an unsigned integer
     - "[pat](expr)" means "(expr) occurences of pat"
     - "[pat]?" means "either pat or nothing"
   * the information in the diff listing is checked against the file to which
     it is applied; an error is printed if there is a conflict
*/

#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define IGNORE_WHITE_SPACE	/* This makes it white space insensitive */

#ifdef IGNORE_WHITE_SPACE
#define strcmp strwcmp
#endif

#define LINELEN	1024

char *prog = 0, *processing = 0;

/* getline() already declared in stdio.h */
#define getline fix_getline
char *getline(FILE *fp, char *b);
char *range(char *s, int *p1, int *p2);
int getcommand(FILE *fp, int *o1, int *o2, char *pcmd, int *n1, int
	*n2);
void fatal(const char *s, ...);
int strwcmp(char *s1, char *s2);
int whitespace(int ch);

char *
getline(FILE *fp, char *b)
{
  if (fgets(b, LINELEN, fp) == NULL) fatal("unexpected eof");

  return b;
}

#define copy(str) printf("%s", str)

int main(int argc, char **argv)
{
  char cmd, *fl, *fd, obuf[LINELEN], nbuf[LINELEN];
  int o1, o2, n1, n2, here;
  FILE *fpf, *fpd;

  prog = argv[0];
  processing = argv[1];
  if (argc != 3) fatal("use: %s original-file diff-list-file", prog);
  if ((fpf = fopen(argv[1], "r")) == NULL) fatal("can't read %s", argv[1]);
  if ((fpd = fopen(argv[2], "r")) == NULL) fatal("can't read %s", argv[2]);
  here = 0;
  while (getcommand(fpd, &o1, &o2, &cmd, &n1, &n2)) {
	while (here < o1 - 1) {
		here++;
		copy(getline(fpf, obuf));
	}
	switch (cmd) {
	    case 'c':
	    case 'd':
		if (cmd == 'd' && n1 != n2) fatal("delete count conflict");
		while (o1 <= o2) {
			fl = getline(fpf, obuf);
			here++;
			fd = getline(fpd, nbuf);
			if (strncmp(fd, "<", (size_t)1))
				fatal("illegal delete line");
			if (strcmp(fl, fd + 2))
				fatal("delete line conflict");
			o1++;
		}
		if (cmd == 'd') break;
		if (strcmp(getline(fpd, nbuf), "---\n"))
			fatal("illegal separator in chunk");
		/* FALLTHROUGH */
	    case 'a':
		if (cmd == 'a') {
			if (o1 != o2) fatal("append count conflict");
			copy(getline(fpf, obuf));
			here++;
		}
		while (n1 <= n2) {
			if (strncmp(getline(fpd, nbuf), ">", (size_t)1))
				fatal("illegal append line");
			copy(nbuf + 2);
			n1++;
		}
		break;
	}
  }
  while (fgets(obuf, LINELEN, fpf) != NULL) copy(obuf);
  return(0);
}

char *
 range(s, p1, p2)
char *s;
int *p1, *p2;
{
  register int v1 = 0, v2;

  while (isdigit(*s)) v1 = 10 * v1 + *s++ - '0';
  v2 = v1;
  if (*s == ',') {
	s++;
	v2 = 0;
	while (isdigit(*s)) v2 = 10 * v2 + *s++ - '0';
  }
  if (v1 > v2) fatal("illegal range");
  *p1 = v1;
  *p2 = v2;
  return s;
}

int getcommand(fp, o1, o2, pcmd, n1, n2)
FILE *fp;
int *o1, *o2, *n1, *n2;
char *pcmd;
{
  char buf[LINELEN];
  register char *s;
  char cmd;

  if ((s = fgets(buf, LINELEN, fp)) == NULL) return 0;
  s = range(s, o1, o2);
  if ((cmd = *s++) != 'a' && cmd != 'c' && cmd != 'd')
	fatal("illegal command");
  s = range(s, n1, n2);
  if (*s != '\n' && s[1] != '\0')
	fatal("extra characters at end of command: %s", s);
  *pcmd = cmd;
  return 1;
}

#ifdef __STDC__
void fatal(const char *s, ...)
{
  va_list args;

  va_start (args, s);
  fprintf(stderr, "%s: processing: %s fatal: ", prog, processing);
  vfprintf(stderr, s, args);
  fprintf(stderr, "\n");
  va_end(args);
  exit(1);
}
#else
/* the K&R lib does not have vfprintf */
void fatal(s, a)
const char *s, *a;
{
  fprintf(stderr, "%s: processing: %s fatal: ", prog, processing);
  fprintf(stderr, s, a);
  fprintf(stderr, "\n");
  exit(1);
}
#endif

#ifdef IGNORE_WHITE_SPACE

/* This routine is a white space insensitive version of strcmp.
   It is needed for testing things which might have undergone
   tab conversion or trailing space removal
   Bret Mckee June, 1988 */

int strwcmp(s1, s2)
char *s1, *s2;
{
  char *x1 = s1, *x2 = s2;

  /* Remove leading white space */
  while (whitespace(*s1)) s1++;
  while (whitespace(*s2)) s2++;
  do {
	while ((*s1 == *s2) && *s1 && *s2) {
		s1++;
		s2++;
	}
	;			/* consume identical characters */
	while (whitespace(*s1)) s1++;
	while (whitespace(*s2)) s2++;
  } while (*s1 && *s2 && (*s1 == *s2));
  if (*s1 - *s2)
	fprintf(stderr, "Failing for (%x)[%s]\n            (%x)[%s]\n",
		(int) *s1, x1, (int) *s2, x2);
  return(*s1 - *s2);
}

int whitespace(ch)
char ch;
{
  switch (ch) {
      case ' ':
      case '\n':
      case 0x0D:
      case '\t':
	return(1);
      default:	return(0);
}
}

#endif
