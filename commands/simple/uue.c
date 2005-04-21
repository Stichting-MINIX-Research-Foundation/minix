/* uue - bulletproof version of uuencode */

/* Uue -- encode a file so that it's printable ascii, short lines
 *
 * Slightly modified from a version posted to net.sources a while back,
 * and suitable for compilation on the IBM PC
 *
 * modified for Lattice C on the ST - 11.05.85 by MSD
 * modified for ALCYON on the ST -    10-24-86 by RDR
 * modified a little more for MWC...  02/09/87 by JPHD
 * (An optional first argument of the form: -nnumber (e.g. -500), will
 * produce a serie of files that long, linked by the include statement,
 * such files are automatically uudecoded by the companion program.)
 * More mods, - ...		   05/06/87 by jphd
 * Mods for TOPS 20, and more.     08/06/87 by jphd
 *     (remove freopen and rindex...change filename generation...)
 * (A lot more to do about I/O speed, avoiding completely the stdio.h...)
 * May be called as uuencode.       Oct 2 1993 by Kees J. Bot
 *
 */


#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define USAGE
#define FILE_NAME 10		/* affects how long names are truncated */

/* ENC is the basic 1 character encoding function to make a char printing */
#define ENC(c) (((c) & 077) + ' ')

FILE *fp, *outp;
char ofname[80];
int lenofname;
int stdo = 0;

#ifdef ST
#define READ "rb"
#else
#define READ "r"
#endif

int part = 'a', chap = 'a';
#define SEQMAX 'z'
#define SEQMIN 'a'
char seqc = SEQMAX;

int split = 0;
int fileln = 32000;

_PROTOTYPE(int main, (int argc, char **argv));
_PROTOTYPE(void maketable, (void));
_PROTOTYPE(void makename, (void));
_PROTOTYPE(void encode, (void));
_PROTOTYPE(void outdec, (char *p));
_PROTOTYPE(int fr, (char *buf, int cnt));

int main(argc, argv)
int argc;
char *argv[];
{
  char *prog_name;
  char *fname;
  int filter;

  prog_name = argv[0] + strlen(argv[0]);
  while (prog_name > argv[0] && prog_name[-1] != '/') prog_name--;
  filter = strcmp(prog_name, "uuencode") == 0;

  if (argc < 2) {
	fprintf(stderr, "Usage: %s [-n] inputfile [-]\n", prog_name);
	exit(2);
  }
  if (argv[1][0] == '-') {
	fileln = -atoi(argv[1]);
	if (fileln <= 0) {
		fprintf(stderr, "Wrong file length arg.\n");
		exit(3);
	}
	split = 1;
	argv++;
	argc--;
  }
  if (filter) {		/* old uuencode reads from standard input */
	fp = stdin;
  } else {
	if ((fp = fopen(argv[1], READ)) == NULL) {	/* binary input !!! */
		fprintf(stderr, "Cannot open %s\n", argv[1]);
		exit(1);
	}
  }
  fname = argv[1] + strlen(argv[1]);
  while (fname > argv[1] && fname[-1] != '/') fname--;
  strcpy(ofname, fname);
  fname = ofname;
  do {
	if (*fname == '.') *fname = '\0';
  } while (*fname++);
  /* 10 char prefix + .uue -> 14 chars MAX */
  lenofname = strlen(ofname);
  if (lenofname > FILE_NAME) ofname[FILE_NAME] = '\0';
  strcat(ofname, ".uue");
  lenofname = strlen(ofname);
  if (!split && (filter || (argc > 2) && (argv[2][0] == '-'))) {
	stdo = 1;
	outp = stdout;
  } else {
	makename();
	if ((outp = fopen(ofname, "w")) == NULL) {
		fprintf(stderr, "Cannot open %s\n", ofname);
		exit(1);
	}
  }
  maketable();
  fprintf(outp, "begin %o %s\n", 0644, argv[1]);
  encode();
  fprintf(outp, "end\n");
  fclose(outp);
  return(0);
}

/* Create ASCII table so a mailer can screw it up and the decode
 * program can restore the error.
 */
void maketable()
{
  register int i, j;

  fputs("table\n", outp);
  for (i = ' ', j = 0; i < '`'; j++) {
	if (j == 32) putc('\n', outp);
	fputc(i++, outp);
  }
  putc('\n', outp);
}

/* Generate the names needed for single and multiple part encoding.  */
void makename()
{
  if (split) {
	ofname[lenofname - 1] = part;
	ofname[lenofname - 2] = chap;
  }
}

/* Copy from in to out, encoding as you go along.  */
void encode()
{
  char buf[80];
  register int i, n;
  register int lines;
  lines = 6;

  for (;;) {
	n = fr(buf, 45);
	putc(ENC(n), outp);
	for (i = 0; i < n; i += 3) outdec(&buf[i]);
	putc(seqc, outp);
	seqc--;
	if (seqc < SEQMIN) seqc = SEQMAX;
	putc('\n', outp);
	++lines;
	if (split && (lines > fileln)) {
		part++;
		if (part > 'z') {
			part = 'a';
			if (chap == 'z')
				chap = 'a';	/* loop ... */
			else
				chap++;
		}
		makename();
		fprintf(outp, "include %s\n", ofname);
		fclose(outp);
		if ((outp = fopen(ofname, "w")) == NULL) {
			fprintf(stderr, "Cannot open %s\n", ofname);
			exit(1);
		}
		maketable();
		fprintf(outp, "begin part %c %s\n", part, ofname);
		lines = 6;
	}
	if (n <= 0) break;
  }
}

/* Output one group of 3 bytes, pointed at by p, on file f.  */
void outdec(p)
register char *p;
{
  register int c1, c2, c3, c4;

  c1 = *p >> 2;
  c2 = ((*p << 4) & 060) | ((p[1] >> 4) & 017);
  c3 = ((p[1] << 2) & 074) | ((p[2] >> 6) & 03);
  c4 = p[2] & 077;
  putc(ENC(c1), outp);
  putc(ENC(c2), outp);
  putc(ENC(c3), outp);
  putc(ENC(c4), outp);
}

/* Fr: like read but stdio */
int fr(buf, cnt)
register char *buf;
register int cnt;
{
  register int c, i;
  for (i = 0; i < cnt; i++) {
	c = fgetc(fp);
	if (feof(fp)) return(i);
	buf[i] = c;
  }
  return(cnt);
}
