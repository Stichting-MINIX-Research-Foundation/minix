/* decomp16: decompress 16bit compressed files on a 16bit Intel processor
 *
 * Version 1.3 of 25 Mar 92.
 *
 * This was written by John N. White on 6/30/91 and is Public Domain.
 * Patched to run under news by Will Rose, Feb 92.
 * J N White's (earlier) patches added by Will Rose, 20 Feb 92.
 * Unsigned int increment/wrap bug fixed by Will Rose, 24 Mar 92.
 * Argument bug fixed, stdio generalised by Will Rose, 25 Mar 92.
 *
 * decomp16 can use as as little as 512 bytes of stack; since it forks
 * four additional copies, it's probably worth using minimum stack rather
 * than the 8192 byte Minix default.  To reduce memory still further,
 * change BUFSZ below to 256; it is currently set to 1024 for speed.  The
 * minimal decomp16 needs about 280k to run in pipe mode (56k per copy).
 *
 * This program acts as a filter:
 *    decomp16 < compressed_file > decompressed_file
 * The arguments -0 to -4 run only the corresponding pass.
 * Thus:
 *    decomp16 -4 < compressed_file > 3;
 *    decomp16 -3 < 3 > 2;
 *    decomp16 -2 < 2 > 1;
 *    decomp16 -1 < 1 > 0;
 *    decomp16 -0 < 0 > decompressed_file
 * will also work, as will connecting the passes by explicit pipes if
 * there is enough memory to do so.  File name arguments can also be
 * given directly on the command line.
 *
 * Compress uses a modified LZW compression algorithm. A compressed file
 * is a set of indices into a dictionary of strings. The number of bits
 * used to store each index depends on the number of entries currently
 * in the dictionary. If there are between 257 and 512 entries, 9 bits
 * are used. With 513 entries, 10 bits are used, etc. The initial dictionary
 * consists of 0-255 (which are the corresponding chars) and 256 (which
 * is a special CLEAR code). As each index in the compressed file is read,
 * a new entry is added to the dictionary consisting of the current string
 * with the first char of the next string appended. When the dictionary
 * is full, no further entries are added. If a CLEAR code is received,
 * the dictionary will be completely reset. The first two bytes of the
 * compressed file are a magic number, and the third byte indicates the
 * maximum number of bits, and whether the CLEAR code is used (older versions
 * of compress didn't have CLEAR).
 *
 * This program works by forking four more copies of itself. The five
 * programs form a pipeline. Copy 0 writes to stdout, and forks copy 1
 * to supply its input, which in turn forks and reads from copy 2, etc.
 * This sequence is used so that when the program exits, all writes
 * are completed and a program that has exec'd uncompress (such as news)
 * can immediately use the uncompressed data when the wait() call returns.
 *
 * If given a switch -#, where # is a digit from 0 to 4 (example: -2), the
 * program will run as that copy, reading from stdin and writing to stdout.
 * This allows decompressing with very limited RAM because only one of the
 * five passes is in memory at a time.
 *
 * The compressed data is a series of string indices (and a header at
 * the beginning and an occasional CLEAR code). As these indices flow
 * through the pipes, each program decodes the ones it can. The result
 * of each decoding will be indices that the following programs can handle.
 *
 * Each of the 65536 strings in the dictionary is an earlier string with
 * some character added to the end (except for the the 256 predefined
 * single char strings). When new entries are made to the dictionary,
 * the string index part will just be the last index to pass through.
 * But the char part is the first char of the next string, which isn't
 * known yet. So the string can be stored as a pair of indices. When
 * this string is specified, it is converted to this pair of indices,
 * which are flagged so that the first will be decoded in full while
 * the second will be decoded to its first char. The dictionary takes
 * 256k to store (64k strings of 2 indices of 2 bytes each). This is
 * too big for a 64k data segment, so it is divided into 5 equal parts.
 * Copy 4 of the program maintains the high part and copy 0 holds the
 * low part.
 */

#include <sys/types.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#define BUFSZ		1024	/* size of i/o buffers */
#define BUFSZ_2		(BUFSZ/2)	/* # of unsigned shorts in i/o bufs */
#define DICTSZ		(unsigned)13056	/* # of local dictionary entries */
#define EOF_INDEX	(unsigned short)0xFFFF	/* EOF flag for pipeline */
#define FALSE		0
#define TRUE		~FALSE

int fdin, fdout, fderr;		/* input, output, and error file descriptors */
int ibufstart, obufind, ibufend;/* i/o buffer indices */
int ipbufind = BUFSZ_2;		/* pipe buffer indices */
int opbufind = 0;
int pnum = -1;			/* ID of this copy */
unsigned short ipbuf[BUFSZ_2];	/* for buffering input */
unsigned short opbuf[BUFSZ_2];	/* for buffering output */
unsigned char *ibuf = (unsigned char *) ipbuf;
unsigned char *obuf = (unsigned char *) opbuf;

unsigned short dindex[DICTSZ];	/* dictionary: index to substring */
unsigned short dchar[DICTSZ];	/* dictionary: last char of string */
unsigned iindex, tindex, tindex2;	/* holds index being processed */
unsigned base;			/* where in global dict local dict starts */
unsigned tbase;
unsigned locend;		/* where in global dict local dict ends */
unsigned curend = 256;		/* current end of global dict */
unsigned maxend;		/* max end of global dict */
int dcharp;			/* ptr to dchar that needs next index entry */
int curbits;			/* number of bits for getbits() to read */
int maxbits;			/* limit on number of bits */
int clearflg;			/* if set, allow CLEAR */
int inmod;			/* mod 8 for getbits() */

int main(int argc, char **argv);
void ffork(void);
void die(char *s);
void myputc(unsigned c);
unsigned mygetc(void);
void getbits(void);
void getpipe(void);
void putpipe(unsigned u, int flag);

int main(int argc, char **argv)
{
  char c, *cp;
  int j, k, fdtmp;
  unsigned int len;

  /* Find the program name */
  j = 0;
  while (argv[0][j] != '\0') j++;
  len = (unsigned int) j;
  while (j--)
	if (argv[0][j] == '/') break;
  if (argv[0][j] == '/') j++;
  cp = argv[0] + j;
  len -= j;

  /* Sort out the flags */
  for (k = 1; k < argc; k++) {
	if (argv[k][0] == '-') {
		c = argv[k][1];
		switch (c) {
		    case '0':	/* pass numbers */
		    case '1':
		    case '2':
		    case '3':
		    case '4':	pnum = c - '0';	break;
		    case 'd':	/* used by news */
			break;
		    default:
			(void) write(1, "Usage: ", 7);
			(void) write(1, cp, len);
			(void) write(1, " [-#] [in] [out]\n", 17);
			exit(0);
			break;
		}

		/* Once it's checked, lose it anyway */
		for (j = k; j < argc; j++) argv[j] = argv[j + 1];
		argc--;
		k--;
	}
  }

  /* Default i/o settings */
  fdin = 0;
  fdout = 1;
  fderr = 2;

  /* Try to open specific files and connect them to stdin/stdout */
  if (argc > 1) {
	if ((fdtmp = open(argv[1], 0)) == -1) die("input open failed");
	(void) close(0);
	if ((fdin = dup(fdtmp)) == -1) die("input dup failed\n");
	(void) close(fdtmp);
  }
  if (argc > 2) {
	(void) unlink(argv[2]);
	if ((fdtmp = creat(argv[2], 0666)) == -1) die("output creat failed");
	(void) close(1);
	if ((fdout = dup(fdtmp)) == -1) die("output dup failed\n");
	(void) close(fdtmp);
  }

  /* Sort out type of compression */
  if (pnum == -1 || pnum == 4) {/* if this is pass 4 */
	/* Check header of compressed file */
	if (mygetc() != 0x1F || mygetc() != 0x9D)      /* check magic number */
		die("not a compressed file\n");
	iindex = mygetc();	/* get compression style */
  } else
	getpipe();		/* get compression style */

  maxbits = iindex & 0x1F;
  clearflg = ((iindex & 0x80) != 0) ? TRUE : FALSE;
  if (maxbits < 9 || maxbits > 16)	/* check for valid maxbits */
	die("can't decompress\n");
  if (pnum != -1 && pnum != 0)
	putpipe(iindex, 0);	/* pass style to next copy */

  /* Fork off an ancestor if necessary - ffork() increments pnum */
  if (pnum == -1) {
	pnum = 0;
	if (pnum == 0) ffork();
	if (pnum == 1) ffork();
	if (pnum == 2) ffork();
	if (pnum == 3) ffork();
  }

  /* Preliminary inits. Note: end/maxend/curend are highest, not
   * highest + 1 */
  base = DICTSZ * pnum + 256;
  locend = base + DICTSZ - 1;
  maxend = (1 << maxbits) - 1;
  if (maxend > locend) maxend = locend;

  while (TRUE) {
	curend = 255 + (clearflg ? 1 : 0);	/* init dictionary */
	dcharp = DICTSZ;	/* flag for none needed */
	curbits = 9;		/* init curbits (for copy 0) */
	while (TRUE) {		/* for each index in input */
		if (pnum == 4) {/* get index using getbits() */
			if (curbits < maxbits && (1 << curbits) <= curend) {
				/* Curbits needs to be increased */
				/* Due to uglyness in compress, these
				 * indices in the compressed file are
				 * wasted */
				while (inmod) getbits();
				curbits++;
			}
			getbits();
		} else
			getpipe();	/* get next index */

		if (iindex == 256 && clearflg) {
			if (pnum > 0) putpipe(iindex, 0);
			/* Due to uglyness in compress, these indices
			 * in the compressed file are wasted */
			while (inmod) getbits();
			break;
		}
		tindex = iindex;
		/* Convert the index part, ignoring spawned chars */
		while (tindex >= base) tindex = dindex[tindex - base];
		/* Pass on the index */
		putpipe(tindex, 0);
		/* Save the char of the last added entry, if any */
		if (dcharp < DICTSZ) dchar[dcharp++] = tindex;
		if (curend < maxend && ++curend > (base - 1))
			dindex[dcharp = (curend - base)] = iindex;

		/* Do spawned chars. They are naturally produced in
		 * the wrong order. To get them in the right order
		 * without using memory, a series of passes,
		 * progressively less deep, are used */
		tbase = base;
		while ((tindex = iindex) >= tbase) {/* for each char to spawn*/
			while ((tindex2 = dindex[tindex - base]) >= tbase)
				tindex = tindex2;    /* scan to desired char */
			putpipe(dchar[tindex-base], 1); /* put it to the pipe*/
			tbase = tindex + 1;
			if (tbase == 0) break;	/* it's a wrap */
		}
	}
  }
}


/* F f o r k
 *
 * Fork off the previous pass - the parent reads from the child.
 */
void ffork()
{
  int j, pfd[2];

  if (pipe(pfd) == -1) die("pipe() error\n");
  if ((j = fork()) == -1) die("fork() error\n");
  if (j == 0) {			/* this is the child */
	if (close(1) == -1) die("close(1) error\n");
	if (dup(pfd[1]) != 1) die("dup(1) error\n");
	(void) close(pfd[0]);
	pnum++;
  } else {			/* this is the parent */
	if (close(0) == -1) die("close(0) error\n");
	if (dup(pfd[0]) != 0) die("dup(0) error\n");
	(void) close(pfd[1]);
  }
}


/* D i e
 *
 * If s is a message, write it to stderr. Flush buffers if needed. Then exit.
 */
void die(char *s)
{
  /* Flush stdout buffer if needed */
  if (obufind != 0) {
	if (write(fdout, (char *) obuf, (unsigned) obufind) != obufind)
		s = "bad stdout write\n";
	obufind = 0;
  }

  /* Flush pipe if needed */
  do
	putpipe(EOF_INDEX, 0);
  while (opbufind);
  /* Write any error message */
  if (s != (char *) NULL) {
	while (*s) (void) write(fderr, s++, 1);
  }
  exit((s == (char *) NULL) ? 0 : 1);
}


/* M p u t c
 *
 * Put a char to stdout.
 */
void myputc(unsigned c)
{
  obuf[obufind++] = c;
  if (obufind >= BUFSZ) {	/* if stdout buffer full */
	if (write(fdout, (char *) obuf, BUFSZ) != BUFSZ)	/* flush to stdout */
		die("bad stdout write\n");
	obufind = 0;
  }
}


/* M y g e t c
 *
 * Get a char from stdin. If EOF, then die() and exit.
 */
unsigned mygetc()
{
  if (ibufstart >= ibufend) {	/* if stdin buffer empty */
	if ((ibufend = read(fdin, (char *) ibuf, BUFSZ)) <= 0)
		die((char *) NULL);	/* if EOF, do normal exit */
	ibufstart = 0;
  }
  return(ibuf[ibufstart++] & 0xff);
}


/* G e t b i t s
 *
 * Put curbits bits into index from stdin. Note: only copy 4 uses this.
 * The bits within a byte are in the correct order. But when the bits
 * cross a byte boundry, the lowest bits will be in the higher part of
 * the current byte, and the higher bits will be in the lower part of
 * the next byte.
 */
void getbits()
{
  int have;
  static unsigned curbyte;	/* byte having bits extracted from it */
  static int left;		/* how many bits are left in curbyte */

  inmod = (inmod + 1) & 7;	/* count input mod 8 */
  iindex = curbyte;
  have = left;
  if (curbits - have > 8) {
	iindex |= mygetc() << have;
	have += 8;
  }
  iindex |= ((curbyte = mygetc()) << have) & ~((unsigned) 0xFFFF << curbits);
  curbyte >>= curbits - have;
  left = 8 - (curbits - have);
}


/* G e t p i p e
 *
 * Get an index from the pipeline. If flagged firstonly, handle it here.
 */
void getpipe()
{
  static short flags;
  static int n = 0;		/* number of flags in flags */

  while (TRUE) {		/* while index with firstonly flag set */
	if (n <= 0) {
		if (ipbufind >= BUFSZ_2) {	/* if pipe input buffer
						 * empty */
			if (read(fdin, (char *) ipbuf, BUFSZ) != BUFSZ)
				die("bad pipe read\n");
			ipbufind = 0;
		}
		flags = ipbuf[ipbufind++];
		n = 15;
	}
	iindex = ipbuf[ipbufind++];
	if (iindex > curend)
		die((iindex == EOF_INDEX) ? (char *) NULL : "invalid data\n");
	flags <<= 1;
	n--;
	/* Assume flags < 0 if highest remaining flag is set */
	if (flags < 0) {	/* if firstonly flag for index is not set */
		while (iindex >= base) iindex = dindex[iindex - base];
		putpipe(iindex, 1);
	} else
		return;		/* return with valid non-firstonly index */
  }
}


/* P u t p i p e
 *
 * put an index into the pipeline.
 */
void putpipe(unsigned u, int flag)
{
  static unsigned short flags, *flagp;
  static int n = 0;		/* number of flags in flags */

  if (pnum == 0) {		/* if we should write to stdout */
	myputc(u);		/* index will be the char value */
	return;
  }
  if (n == 0) {			/* if we need to reserve a flag entry */
	flags = 0;
	flagp = opbuf + opbufind;
	opbufind++;
  }
  opbuf[opbufind++] = u;	/* add index to buffer */
  flags = (flags << 1) | flag;	/* add firstonly flag */
  if (++n >= 15) {		/* if block of 15 indices */
	n = 0;
	*flagp = flags;		/* insert flags entry */
	if (opbufind >= BUFSZ_2) {	/* if pipe out buffer full */
		opbufind = 0;
		if (write(fdout, (char *) opbuf, BUFSZ) != BUFSZ)
			die("bad pipe write\n");
	}
  }
}
