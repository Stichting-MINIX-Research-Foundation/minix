/* od - octal dump		   Author: Andy Tanenbaum */

#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>


int bflag, cflag, dflag, oflag, xflag, hflag, vflag;
int linenr, width, state, ever;
int prevwds[8];
long off;
char buf[512], buffer[BUFSIZ];
int next;
int bytespresent;

_PROTOTYPE(int main, (int argc, char **argv));
_PROTOTYPE(long offset, (int argc, char *argv [], int k));
_PROTOTYPE(void dumpfile, (void));
_PROTOTYPE(void wdump, (short *words, int k, int radix));
_PROTOTYPE(void bdump, (char bytes [16 ], int k, int c));
_PROTOTYPE(void byte, (int val, int c));
_PROTOTYPE(int getwords, (short **words));
_PROTOTYPE(int same, (short *w1, int *w2));
_PROTOTYPE(void outword, (int val, int radix));
_PROTOTYPE(void outnum, (int num, int radix));
_PROTOTYPE(void addrout, (long l));
_PROTOTYPE(char hexit, (int k));
_PROTOTYPE(void usage, (void));

int main(argc, argv)
int argc;
char *argv[];
{
  int k, flags;
  char *p;

  /* Process flags */
  setbuf(stdout, buffer);
  flags = 0;
  p = argv[1];
  if (argc > 1 && *p == '-') {
	/* Flags present. */
	flags++;
	p++;
	while (*p) {
		switch (*p) {
		    case 'b':	bflag++;	break;
		    case 'c':	cflag++;	break;
		    case 'd':	dflag++;	break;
		    case 'h':	hflag++;	break;
		    case 'o':	oflag++;	break;
		    case 'v':	vflag++;	break;	
		    case 'x':	xflag++;	break;
		    default:	usage();
		}
		p++;
	}
  } else {
	oflag = 1;
  }
  if ((bflag | cflag | dflag | oflag | xflag) == 0) oflag = 1;
  k = (flags ? 2 : 1);
  if (bflag | cflag) {
	width = 8;
  } else if (oflag) {
	width = 7;
  } else if (dflag) {
	width = 6;
  } else {
	width = 5;
  }

  /* Process file name, if any. */
  p = argv[k];
  if (k < argc && *p != '+') {
	/* Explicit file name given. */
	close(0);
	if (open(argv[k], O_RDONLY) != 0) {
		fprintf(stderr, "od: cannot open %s\n", argv[k]);
		exit(1);
	}
	k++;
  }

  /* Process offset, if any. */
  if (k < argc) {
	/* Offset present. */
	off = offset(argc, argv, k);
	off = (off / 16L) * 16L;
	lseek(0, off, SEEK_SET);
  }
  dumpfile();
  addrout(off);
  printf("\n");
  return(0);
}


long offset(argc, argv, k)
int argc;
char *argv[];
int k;
{
  int dot, radix;
  char *p, c;
  long val;

  /* See if the offset is decimal. */
  dot = 0;
  p = argv[k];
  while (*p)
	if (*p++ == '.') dot = 1;

  /* Convert offset to binary. */
  radix = (dot ? 10 : 8);
  val = 0;
  p = argv[k];
  if (*p == '+') p++;
  while (*p != 0 && *p != '.') {
	c = *p++;
	if (c < '0' || c > '9') {
		printf("Bad character in offset: %c\n", c);
		exit(1);
	}
	val = radix * val + c - '0';
  }

  p = argv[k + 1];
  if (k + 1 == argc - 1 && *p == 'b') val = 512L * val;
  return(val);
}


void dumpfile()
{
  int k;
  short *words;

  while ((k = getwords(&words))) {	/* 'k' is # bytes read */
	if (!vflag) {		/* ensure 'lazy' evaluation */
		if (k == 16 && ever == 1 && same(words, prevwds)) {
			if (state == 0) {
				printf("*\n");
				state = 1;
				off += 16;
				continue;
			} else if (state == 1) {
				off += 16;
				continue;
			}
		}
	}
	addrout(off);
	off += k;
	state = 0;
	ever = 1;
	linenr = 1;
	if (oflag) wdump(words, k, 8);
	if (dflag) wdump(words, k, 10);
	if (xflag) wdump(words, k, 16);
	if (cflag) bdump((char *)words, k, (int)'c');
	if (bflag) bdump((char *)words, k, (int)'b');
	for (k = 0; k < 8; k++) prevwds[k] = words[k];
	for (k = 0; k < 8; k++) words[k] = 0;
  }
}


void wdump(words, k, radix)
short *words;
int k, radix;
{
  int i;

  if (linenr++ != 1) printf("       ");
  for (i = 0; i < (k + 1) / 2; i++) outword(words[i] & 0xFFFF, radix);
  printf("\n");
}


void bdump(bytes, k, c)
char bytes[16];
int k;
char c;
{
  int i;

  if (linenr++ != 1) printf("       ");
  for (i = 0; i < k; i++) byte(bytes[i] & 0377, c);
  printf("\n");
}

void byte(val, c)
int val;
char c;
{
  if (c == 'b') {
	printf(" ");
	outnum(val, 7);
	return;
  }
  if (val == 0)
	printf("  \\0");
  else if (val == '\b')
	printf("  \\b");
  else if (val == '\f')
	printf("  \\f");
  else if (val == '\n')
	printf("  \\n");
  else if (val == '\r')
	printf("  \\r");
  else if (val == '\t')
	printf("  \\t");
  else if (val >= ' ' && val < 0177)
	printf("   %c", val);
  else {
	printf(" ");
	outnum(val, 7);
  }
}


int getwords(words)
short **words;
{
  int count;

  if (next >= bytespresent) {
	bytespresent = read(0, buf, 512);
	next = 0;
  }
  if (next >= bytespresent) return(0);
  *words = (short *) &buf[next];
  if (next + 16 <= bytespresent)
	count = 16;
  else
	count = bytespresent - next;

  next += count;
  return(count);
}

int same(w1, w2)
short *w1;
int *w2;
{
  int i;
  i = 8;
  while (i--)
	if (*w1++ != *w2++) return(0);
  return(1);
}

void outword(val, radix)
int val, radix;
{
/* Output 'val' in 'radix' in a field of total size 'width'. */

  int i;

  if (radix == 16) i = width - 4;
  if (radix == 10) i = width - 5;
  if (radix == 8) i = width - 6;
  if (i == 1)
	printf(" ");
  else if (i == 2)
	printf("  ");
  else if (i == 3)
	printf("   ");
  else if (i == 4)
	printf("    ");
  outnum(val, radix);
}


void outnum(num, radix)
int num, radix;
{
/* Output a number with all leading 0s present.  Octal is 6 places,
 * decimal is 5 places, hex is 4 places.
 */
  unsigned val;

  val = (unsigned) num;
  if (radix == 8)
	printf ("%06o", val);
  else if (radix == 10)
	printf ("%05u", val);
  else if (radix == 16)
	printf ("%04x", val);
  else if (radix == 7) {
  	/* special case */
	printf ("%03o", val);
  }
}


void addrout(l)
long l;
{
  if (hflag == 0) {
	printf("%07lo", l);
  } else {
	printf("%07lx", l);
  }
}


void usage()
{
  fprintf(stderr, "Usage: od [-bcdhovx] [file] [ [+] offset [.] [b] ]\n");
}
