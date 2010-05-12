/* join - relation data base operator	Author:  Saeko Hirabayashi */

/* Written by Saeko Hirabayashi, 1989.
 * 1992-01-28 Modified by Kouichi Hirabayashi to add some POSIX1003.2 options.
 *
 * This a free program.
 */

#include <string.h>
#include <stdio.h>

#define MAXFLD	200		/* maximum # of fields to accept */

_PROTOTYPE(void main, (int argc, char **argv));
_PROTOTYPE(void error, (char *s, char *t));
_PROTOTYPE(void usage, (void));
_PROTOTYPE(void match, (void));
_PROTOTYPE(void f1_only, (void));
_PROTOTYPE(void f2_only, (void));
_PROTOTYPE(void output, (int flag));
_PROTOTYPE(void outfld, (int file));
_PROTOTYPE(void outputf, (int flag));
_PROTOTYPE(int compare, (void));
_PROTOTYPE(int get1, (void));
_PROTOTYPE(int get2, (int back));
_PROTOTYPE(int getrec, (int file));
_PROTOTYPE(int split, (int file));
_PROTOTYPE(int atoi, (char *str));
_PROTOTYPE(int exit, (int val));
_PROTOTYPE(FILE * efopen, (char *file, char *mode));
_PROTOTYPE(void (*outfun), (int file));	/* output func: output() or outputf()*/

#define F1	1
#define F2	2
#define SEP	(sep ? sep : ' ')

FILE *fp[2];			/* file pointer for file1 and file2 */
long head;			/* head of the current (same)key group of the
				 * file2 */

char buf[2][BUFSIZ];		/* input buffer for file1 and file2 */
char *fld[2][MAXFLD];		/* field vector for file1 and file2 */
int nfld[2];			/* # of fields for file1 and file2 */

int kpos[2];			/* key field position for file1 and file2
				 * (from 0) */
char oldkey[BUFSIZ];		/* previous key of the file1 */

struct {			/* output list by -o option */
  int o_file;			/* file #: 0 or 1 */
  int o_field;			/* field #: 0, 1, 2, .. */
} olist[MAXFLD];
int nout;			/* # of output filed */

int aflag;			/* n for '-an': F1 or F2 or both */
int vflag;			/* n for '-vn': F1 or F2 or both */
char *es;			/* s for '-e s' */
char sep;			/* c for -tc: filed separator */
char *cmd;			/* name of this program */

void main(argc, argv)
int argc;
char **argv;
{
  register char *s;
  int c, i, j;

  cmd = argv[0];
  outfun = output;		/* default output form */

  while (--argc > 0 && (*++argv)[0] == '-' && (*argv)[1]) {
	/* "-" is a file name (stdin) */
	s = argv[0] + 1;
	if ((c = *s) == '-' && !s[1]) {
		++argv;
		--argc;
		break;		/* -- */
	}
	if (*++s == '\0') {
		s = *++argv;
		--argc;
	}
	switch (c) {
	    case 'a':		/* add unpairable line to output */
		vflag = 0;
		switch (*s) {
		    case '1':	aflag |= F1;	break;
		    case '2':	aflag |= F2;	break;
		    default:	aflag |= (F1 | F2);	break;
		}
		break;

	    case 'e':		/* replace empty field by es */
		es = s;
		break;

	    case 'j':		/* key field (obsolute) */
		c = *s++;
		if (*s == '\0') {
			s = *++argv;
			--argc;
		}

	    case '1':		/* key field of file1 */
	    case '2':		/* key field of file2 */
		i = atoi(s) - 1;

		switch (c) {
		    case '1':	kpos[0] = i;	break;
		    case '2':	kpos[1] = i;	break;
	            default:	kpos[0] = kpos[1] = i;
				break;
		}
		break;

	    case 'o':		/* specify output format */
		do {
			i = j = 0;
			sscanf(s, "%d.%d", &i, &j);
			if (i < 1 || j < 1 || i > 2) usage();
			olist[nout].o_file = i - 1;
			olist[nout].o_field = j - 1;
			nout++;
			if ((s = strchr(s, ',')) != (char *) 0)
				s++;
			else {
				s = *++argv;
				--argc;
			}
		} while (argc > 2 && *s != '-');
		++argc;
		--argv;		/* compensation */
		outfun = outputf;
		break;

	    case 't':		/* tab char */
		sep = *s;
		break;

	    case 'v':		/* output unpairable line only */
		aflag = 0;
		switch (*s) {
		    case '1':	vflag |= F1;	break;
		    case '2':	vflag |= F2;	break;
		    default:	vflag |= (F1 | F2);	break;
		}
		break;

	    default:	usage();
	}
  }
  if (argc != 2) usage();

  fp[0] = strcmp(argv[0], "-") ? efopen(argv[0], "r") : stdin;
  fp[1] = efopen(argv[1], "r");

  nfld[0] = get1();		/* read file1 */
  nfld[1] = get2(0);		/* read file2 */

  while (nfld[0] || nfld[1]) {
	if ((i = compare()) == 0)
		match();
	else if (i < 0)
		f1_only();
	else
		f2_only();
  }
  fflush(stdout);

  exit(0);
}

void usage()
{
  fprintf(stderr,
    "Usage: %s [-an|-vn] [-e str] [-o list] [-tc] [-1 f] [-2 f] file1 file2\n",
    cmd);
  exit(1);
}

int compare()
{				/* compare key field */
  register int r;

  if (nfld[1] == 0)		/* file2 EOF */
	r = -1;
  else if (nfld[0] == 0)	/* file1 EOF */
	r = 1;
  else {
	if (nfld[0] <= kpos[0])
		error("missing key field in file1", (char *) 0);
	if (nfld[1] <= kpos[1])
		error("missing key field in file2", (char *) 0);

	r = strcmp(fld[0][kpos[0]], fld[1][kpos[1]]);
  }
  return r;
}

void match()
{
  long p;

  if (!vflag) (*outfun) (F1 | F2);

  p = ftell(fp[1]);
  nfld[1] = get2(0);		/* check key order */
  if (nfld[1] == 0 || strcmp(fld[0][kpos[0]], fld[1][kpos[1]])) {
	nfld[0] = get1();
	if (strcmp(fld[0][kpos[0]], oldkey) == 0) {
		fseek(fp[1], head, 0);	/* re-do from head */
		nfld[1] = get2(1);	/* don't check key order */
	} else
		head = p;	/* mark here */
  }
}

void f1_only()
{
  if ((aflag & F1) || (vflag & F1)) (*outfun) (F1);
  nfld[0] = get1();
}

void f2_only()
{
  if ((aflag & F2) || (vflag & F2)) (*outfun) (F2);
  head = ftell(fp[1]);		/* mark */
  nfld[1] = get2(0);		/* check key order */
}

void output(f)
{				/* default output form */
  if (f & F1)
	fputs(fld[0][kpos[0]], stdout);
  else
	fputs(fld[1][kpos[1]], stdout);
  if (f & F1) outfld(0);
  if (f & F2) outfld(1);
  fputc('\n', stdout);
}

void outfld(file)
{				/* output all fields except key_field */
  register int i;
  int k, n;

  k = kpos[file];
  n = nfld[file];
  for (i = 0; i < n; i++)
	if (i != k) {
		fputc(SEP, stdout);
		fputs(fld[file][i], stdout);
	}
}

void outputf(f)
{				/* output by '-o list' */
  int i, j, k;
  register char *s;

  for (i = k = 0; i < nout; i++) {
	j = olist[i].o_file;
	if ((f & (j + 1)) && (olist[i].o_field < nfld[j]))
		s = fld[j][olist[i].o_field];
	else
		s = es;
	if (s) {
		if (k++) fputc(SEP, stdout);
		fputs(s, stdout);
	}
  }
  fputc('\n', stdout);
}

int get1()
{				/* read file1 */
  int r;
  static char oldkey1[BUFSIZ];

  if (fld[0][kpos[0]]) {
        strcpy(oldkey, fld[0][kpos[0]]);  /* save previous key for control */
  }
  r = getrec(0);

  if (r) {
        if (strcmp(oldkey1, fld[0][kpos[0]]) > 0)
	      error("file1 is not sorted", (char *) 0);
        strcpy(oldkey1, fld[0][kpos[0]]);  /* save prev key for sort check */
  }
  return r;
}

int get2(back)
{				/* read file2 */
  static char oldkey2[BUFSIZ];
  int r;

  r = getrec(1);

  if (r) {
        if (!back && strcmp(oldkey2, fld[1][kpos[1]]) > 0)
	      error("file2 is not sorted", (char *) 0);
        strcpy(oldkey2, fld[1][kpos[1]]);  /* save prev key for sort check */
  }
  return r;
}

int getrec(file)
{				/* read one line to split it */
  if (fgets(buf[file], BUFSIZ, fp[file]) == (char *) 0)
	*buf[file] = '\0';
  else if (*buf[file] == '\n' || *buf[file] == '\r')
	error("null line in file%s", file ? "1" : "0");

  return split(file);
}

int split(file)
{				/* setup fields */
  register int n;
  register char *s, *t;

  for (n = 0, s = buf[file]; *s && *s != '\n' && *s != '\r';) {
	if (sep) {
		for (t = s; *s && *s != sep && *s != '\n' && *s != '\r'; s++);
	} else {
		while (*s == ' ' || *s == '\t')
			s++;	/* skip leading white space */
		for (t = s; *s && *s != ' ' && *s != '\t'
		     && *s != '\n' && *s != '\r'; s++);
		/* We will treat trailing white space as NULL field */
	}
	if (*s) *s++ = '\0';
	fld[file][n++] = t;
	if (n == MAXFLD) error("too many filed in file%s", file ? "1" : "0");
  }
  fld[file][n] = (char *) 0;

  return n;
}

FILE *efopen(file, mode)
char *file, *mode;
{
  FILE *fp;

  if ((fp = fopen(file, mode)) == (FILE *) 0) error("can't open %s", file);

  return fp;
}

void error(s, t)
char *s, *t;
{
  fprintf(stderr, "%s: ", cmd);
  fprintf(stderr, s, t);
  fprintf(stderr, "\n");

  exit(1);
}
