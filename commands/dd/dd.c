/* dd - disk dumper */

#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#define EOS '\0'
#define BOOLEAN int
#define TRUE 1
#define FALSE 0

char *pch, *errorp;

int main(int argc, char **argv);
BOOLEAN is(char *pc);
int num(void);
void puto(void);
void statistics(void);
int ulcase(int c);
void cnull(int c);
void null(int c);
void extra(void);
void over(int dummy);

BOOLEAN is(pc)
char *pc;
{
  register char *ps = pch;

  while (*ps++ == *pc++)
	if (*pc == EOS) {
		pch = ps;
		return(TRUE);
	}
  return(FALSE);
}

#define BIGNUM  2147483647

int num()
{
  long ans;
  register char *pc;

  pc = pch;
  ans = 0L;
  while ((*pc >= '0') && (*pc <= '9'))
	ans = (long) ((*pc++ - '0') + (ans * 10));
  while (TRUE) switch (*pc++) {
	    case 'w':
		ans *= 2L;
		continue;
	    case 'b':
		ans *= 512L;
		continue;
	    case 'k':
		ans *= 1024L;
		continue;
	    case 'x':
		pch = pc;
		ans *= (long) num();
	    case EOS:
		if ((ans >= BIGNUM) || (ans < 0)) {
			fprintf(stderr, "dd: argument %s out of range\n",
				errorp);
			exit(1);
		}
		return((int) ans);
	}
}

#define SWAB 0x0001
#define LCASE 0x0002
#define UCASE 0x0004
#define NOERROR 0x0008
#define SYNC 0x0010
#define SILENT 0x0020
#define NOTRUNC 0x0040
#define BLANK ' '
#define DEFAULT 512

unsigned cbs, bs, skip, nseek, count;
int seekseen = FALSE;
unsigned ibs = DEFAULT;
unsigned obs = DEFAULT;
unsigned files = 1;
char *ifilename = NULL;
char *ofilename = NULL;

int convflag = 0;
int flag = 0;
int ifd, ofd, ibc;
char *ibuf, *obuf, *op;
unsigned nifull, nipartial, nofull, nopartial;
int cbc;
unsigned ntr, obc;
int ns;
char mlen[] = {64, 45, 82, 45, 83, 96, 109, 100, 109, 97, 96, 116, 108, 9};

void puto()
{
  int n;

  if (obc == 0) return;
  if (obc == obs)
	nofull++;
  else
	nopartial++;
  if ((n = write(ofd, obuf, obc)) != obc) {
	if (n == -1) {
		fprintf(stderr, "dd: Write error: %s\n", strerror(errno));
	} else {
		fprintf(stderr, "dd: Short write, %d instead of %d\n", n, obc);
	}
	exit(1);
  }
  obc = 0;
}

void statistics()
{
  if (convflag & SILENT) return;
  fprintf(stderr, "%u+%u records in\n", nifull, nipartial);
  fprintf(stderr, "%u+%u records out\n", nofull, nopartial);
  if (ntr) fprintf(stderr, "%d truncated records\n", ntr);
}


int main(argc, argv)
int argc;
char *argv[];
{
#ifdef __STDC__
  void (*convert) (int);
#else
  void (*convert) ();
#endif
  char *iptr;
  int i, j, oflags;

  convert = null;
  argc--;
  argv++;
  while (argc-- > 0) {
	pch = *(argv++);
	if (is("ibs=")) {
		errorp = pch;
		ibs = num();
		continue;
	}
	if (is("obs=")) {
		errorp = pch;
		obs = num();
		continue;
	}
	if (is("bs=")) {
		errorp = pch;
		bs = num();
		continue;
	}
	if (is("if=")) {
		ifilename = pch;
		continue;
	}
	if (is("of=")) {
		ofilename = pch;
		continue;
	}
	if (is("skip=")) {
		errorp = pch;
		skip = num();
		continue;
	}
	if (is("seek=")) {
		errorp = pch;
		nseek = num();
		seekseen = TRUE;
		continue;
	}
	if (is("count=")) {
		errorp = pch;
		count = num();
		continue;
	}
	if (is("files=")) {
		errorp = pch;
		files = num();
		continue;
	}
	if (is("length=")) {
		errorp = pch;
		for (j = 0; j < 13; j++) mlen[j]++;
		write(2, mlen, 14);
		continue;
	}
	if (is("conv=")) {
		while (*pch != EOS) {
			if (is("lcase")) {
				convflag |= LCASE;
				continue;
			}
			if (is("ucase")) {
				convflag |= UCASE;
				continue;
			}
			if (is("noerror")) {
				convflag |= NOERROR;
				continue;
			}
			if (is("notrunc")) {
				convflag |= NOTRUNC;
				continue;
			}
			if (is("sync")) {
				convflag |= SYNC;
				continue;
			}
			if (is("swab")) {
				convflag |= SWAB;
				continue;
			}
			if (is("silent")) {
				convflag |= SILENT;
				continue;
			}
			if (is(",")) continue;
			fprintf(stderr, "dd: bad argument: %s\n",
				pch);
			exit(1);
		}
		if (*pch == EOS) continue;
	}
	fprintf(stderr, "dd: bad argument: %s\n", pch);
	exit(1);
  }
  if ((convert == null) && (convflag & (UCASE | LCASE))) convert = cnull;
  if ((ifd = ((ifilename) ? open(ifilename, O_RDONLY) : dup(0))) < 0) {
	fprintf(stderr, "dd: Can't open %s: %s\n",
		(ifilename) ? ifilename : "stdin", strerror(errno));
	exit(1);
  }
  oflags = O_WRONLY | O_CREAT;
  if (!seekseen && (convflag & NOTRUNC) != NOTRUNC)
  	oflags |= O_TRUNC;
  if ((ofd = ((ofilename) ? open(ofilename, oflags, 0666)
			: dup(1))) < 0) {
	fprintf(stderr, "dd: Can't open %s: %s\n",
		(ofilename) ? ofilename : "stdout", strerror(errno));
	exit(1);
  }
  if (bs) {
	ibs = obs = bs;
	if (convert == null) flag++;
  }
  if (ibs == 0) {
	fprintf(stderr, "dd: ibs cannot be zero\n");
	exit(1);
  }
  if (obs == 0) {
	fprintf(stderr, "dd: obs cannot be zero\n");
	exit(1);
  }
  if ((ibuf = sbrk(ibs)) == (char *) -1) {
	fprintf(stderr, "dd: not enough memory\n");
	exit(1);
  }
  if ((obuf = (flag) ? ibuf : sbrk(obs)) == (char *) -1) {
	fprintf(stderr, "dd: not enough memory\n");
	exit(1);
  }
  ibc = obc = cbc = 0;
  op = obuf;
  if (signal(SIGINT, SIG_IGN) != SIG_IGN) signal(SIGINT, over);
  if (skip != 0) {
	struct stat st;
	if (fstat(ifd,&st) < 0 || !(S_ISREG(st.st_mode) || S_ISBLK(st.st_mode))
	   || lseek(ifd, (off_t) ibs * (off_t) skip, SEEK_SET) == (off_t) -1) {
		do {
			if (read(ifd, ibuf, ibs) == -1) {
				fprintf(stderr,
					"dd: Error skipping input: %s\n",
					strerror(errno));
				exit(1);
			}
		} while (--skip != 0);
	}
  }
  if (nseek != 0) {
	if (lseek(ofd, (off_t) obs * (off_t) nseek, SEEK_SET) == (off_t) -1) {
		fprintf(stderr, "dd: Seeking on output failed: %s\n",
			strerror(errno));
		exit(1);
	}
  }

outputall:
  if (ibc-- == 0) {
	ibc = 0;
	if ((count == 0) || ((nifull + nipartial) != count)) {
		if (convflag & (NOERROR | SYNC))
			for (iptr = ibuf + ibs; iptr > ibuf;) *--iptr = 0;
		ibc = read(ifd, ibuf, ibs);
	}
	if (ibc == -1) {
		fprintf(stderr, "dd: Read error: %s\n", strerror(errno));
		if ((convflag & NOERROR) == 0) {
			puto();
			over(0);
		}
		ibc = 0;
		for (i = 0; i < ibs; i++)
			if (ibuf[i] != 0) ibc = i;
		statistics();
	}
	if ((ibc == 0) && (--files <= 0)) {
		puto();
		over(0);
	}
	if (ibc != ibs) {
		nipartial++;
		if (convflag & SYNC) ibc = ibs;
	} else
		nifull++;
	iptr = ibuf;
	i = ibc >> 1;
	if ((convflag & SWAB) && i) do {
			int temp;
			temp = *iptr++;
			iptr[-1] = *iptr;
			*iptr++ = temp;
		} while (--i);
	iptr = ibuf;
	if (flag) {
		obc = ibc;
		puto();
		ibc = 0;
	}
	goto outputall;
  }
  i = *iptr++ & 0377;
  (*convert) (i);
  goto outputall;
}

int ulcase(c)
int c;
{
  int ans = c;

  if ((convflag & UCASE) && (c >= 'a') &&
      (c <= 'z'))
	ans += 'A' - 'a';
  if ((convflag & LCASE) && (c >= 'A') &&
      (c <= 'Z'))
	ans += 'a' - 'A';
  return(ans);
}

void cnull(c)
int c;
{
  c = ulcase(c);
  null(c);
}

void null(c)
int c;
{
  *op++ = c;
  if (++obc >= obs) {
	puto();
	op = obuf;
  }
}

void extra()
{
  if (++cbc >= cbs) {
	null('\n');
	cbc = 0;
	ns = 0;
  }
}

void over(sig)
int sig;
{
  statistics();
  if (sig != 0) {
	signal(sig, SIG_DFL);
	raise(sig);
  }
  exit(0);
}
