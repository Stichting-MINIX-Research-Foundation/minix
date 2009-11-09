/*	archive.c - archive support			Author: Kees J. Bot
 *								13 Nov 1993
 */
#include "h.h"

#ifdef unix

#include <unistd.h>
#include <fcntl.h>

#define arraysize(a)	(sizeof(a) / sizeof((a)[0]))
#define arraylimit(a)	((a) + arraysize(a))

/* ASCII ar header. */

#define ASCII_ARMAG	"!<arch>\n"
#define ASCII_SARMAG	8
#define ASCII_ARFMAG	"`\n"

struct ascii_ar_hdr {
	char	ar_name[16];
	char	ar_date[12];
	char	ar_uid[6];
	char	ar_gid[6];
	char	ar_mode[8];
	char	ar_size[10];
	char	ar_fmag[2];
};

/* ACK ar header. */

#define	ACK_ARMAG	0177545
#define ACK_AALMAG	0177454

struct ack_ar_hdr {
	char		ar_name[14];
	unsigned long	ar_date;
	unsigned char	ar_uid;
	unsigned char	ar_gid;
	unsigned short	ar_mode;
	unsigned long	ar_size;
};

typedef struct archname {
	struct archname	*next;		/* Next on the hash chain. */
	char		name[16];	/* One archive entry. */
	time_t		date;		/* The timestamp. */
	/* (no need for other attibutes) */
} archname_t;

static size_t namelen;			/* Max name length, 14 or 16. */

#define HASHSIZE	(64 << sizeof(int))

static archname_t *nametab[HASHSIZE];

_PROTOTYPE( static int hash, (char *name) );
_PROTOTYPE( static int searchtab, (char *name, time_t *date, int scan) );
_PROTOTYPE( static void deltab, (void) );
_PROTOTYPE( static long ar_atol, (char *s, size_t n) );
_PROTOTYPE( static int read_ascii_archive, (int afd) );
_PROTOTYPE( static int read_ack_archive, (int afd) );

static char *lpar, *rpar;	/* Leave these at '(' and ')'. */

int is_archive_ref(name) char *name;
/* True if name is of the form "archive(file)". */
{
  char *p = name;

  while (*p != 0 && *p != '(' && *p != ')') p++;
  lpar = p;
  if (*p++ != '(') return 0;

  while (*p != 0 && *p != '(' && *p != ')') p++;
  rpar = p;
  if (*p++ != ')') return 0;

  return *p == 0;
}

static int hash(name) char *name;
/* Compute a hash value out of a name. */
{
	unsigned h = 0;
	unsigned char *p = (unsigned char *) name;
	int n = namelen;

	while (*p != 0) {
		h = h * 0x1111 + *p++;
		if (--n == 0) break;
	}

	return h % arraysize(nametab);
}

static int searchtab(name, date, scan) char *name; time_t *date; int scan;
/* Enter a name to the table, or return the date of one already there. */
{
	archname_t **pnp, *np;
	int cmp = 1;

	pnp = &nametab[hash(name)];

	while ((np = *pnp) != NULL
			&& (cmp = strncmp(name, np->name, namelen)) > 0) {
		pnp= &np->next;
	}

	if (cmp != 0) {
		if (scan) {
			errno = ENOENT;
			return -1;
		}
		if ((np = (archname_t *) malloc(sizeof(*np))) == NULL)
			fatal("No memory for archive name cache",(char *)0,0);
		strncpy(np->name, name, namelen);
		np->date = *date;
		np->next = *pnp;
		*pnp = np;
	}
	if (scan) *date = np->date;
	return 0;
}

static void deltab()
/* Delete the name cache, a different library is to be read. */
{
	archname_t **pnp, *np, *junk;

	for (pnp = nametab; pnp < arraylimit(nametab); pnp++) {
		for (np = *pnp; np != NULL; ) {
			junk = np;
			np = np->next;
			free(junk);
		}
		*pnp = NULL;
	}
}

static long ar_atol(s, n) char *s; size_t n;
/* Transform a string into a number.  Ignore the space padding. */
{
  long l= 0;

  while (n > 0) {
	if (*s != ' ') l= l * 10 + (*s - '0');
	s++;
	n--;
  }
  return l;
}

static int read_ascii_archive(afd)
int afd;
/* Read a modern ASCII type archive. */
{
  struct ascii_ar_hdr hdr;
  off_t pos= 8;
  char *p;
  time_t date;

  namelen = 16;

  for (;;) {
	if (lseek(afd, pos, SEEK_SET) == -1) return -1;

	switch (read(afd, &hdr, sizeof(hdr))) {
	case sizeof(hdr):
		break;
	case -1:
		return -1;
	default:
		return 0;
	}

	if (strncmp(hdr.ar_fmag, ASCII_ARFMAG, sizeof(hdr.ar_fmag)) != 0) {
		errno= EINVAL;
		return -1;
	}

	/* Strings are space padded! */
	for (p= hdr.ar_name; p < hdr.ar_name + sizeof(hdr.ar_name); p++) {
		if (*p == ' ') {
			*p= 0;
			break;
		}
	}

	/* Add a file to the cache. */
	date = ar_atol(hdr.ar_date, sizeof(hdr.ar_date));
	searchtab(hdr.ar_name, &date, 0);

	pos+= sizeof(hdr) + ar_atol(hdr.ar_size, sizeof(hdr.ar_size));
	pos= (pos + 1) & (~ (off_t) 1);
  }
}

static int read_ack_archive(afd)
int afd;
/* Read an ACK type archive. */
{
  unsigned char raw_hdr[14 + 4 + 1 + 1 + 2 + 4];
  struct ack_ar_hdr hdr;
  off_t pos= 2;
  time_t date;

  namelen = 14;

  for (;;) {
	if (lseek(afd, pos, SEEK_SET) == -1) return -1;

	switch (read(afd, raw_hdr, sizeof(raw_hdr))) {
	case sizeof(raw_hdr):
		break;
	case -1:
		return -1;
	default:
		return 0;
	}

	/* Copy the useful fields from the raw bytes transforming PDP-11
	 * style numbers to native format.
	 */
	memcpy(hdr.ar_name, raw_hdr + 0, 14);
	hdr.ar_date=	  (long) raw_hdr[14 + 1] << 24
			| (long) raw_hdr[14 + 0] << 16
			| (long) raw_hdr[14 + 3] <<  8
			| (long) raw_hdr[14 + 2] <<  0;
	hdr.ar_size=	  (long) raw_hdr[22 + 1] << 24
			| (long) raw_hdr[22 + 0] << 16
			| (long) raw_hdr[22 + 3] <<  8
			| (long) raw_hdr[22 + 2] <<  0;

	/* Add a file to the cache. */
	date = hdr.ar_date;
	searchtab(hdr.ar_name, &date, 0);

	pos= (pos + 26 + hdr.ar_size + 1) & (~ (off_t) 1);
  }
}

int archive_stat(name, stp) char *name; struct stat *stp;
/* Search an archive for a file and return that file's stat info. */
{
  int afd;
  int r= -1;
  char magic[8];
  char *file;
  static dev_t ardev;
  static ino_t arino = 0;
  static time_t armtime;

  if (!is_archive_ref(name)) { errno = EINVAL; return -1; }
  *lpar= 0;
  *rpar= 0;
  file= lpar + 1;

  if (stat(name, stp) < 0) goto bail_out;

  if (stp->st_ino != arino || stp->st_dev != ardev) {
	/* Either the first (and probably only) library, or a different
	 * library.
	 */
	arino = stp->st_ino;
	ardev = stp->st_dev;
	armtime = stp->st_mtime;
	deltab();

	if ((afd= open(name, O_RDONLY)) < 0) goto bail_out;

	switch (read(afd, magic, sizeof(magic))) {
	case 8:
		if (strncmp(magic, ASCII_ARMAG, 8) == 0) {
			r= read_ascii_archive(afd);
			break;
		}
		if ((magic[0] & 0xFF) == ((ACK_AALMAG >> 0) & 0xFF)
			&& (magic[1] & 0xFF) == ((ACK_AALMAG >> 8) & 0xFF)
		) {
			r= read_ack_archive(afd);
			break;
		}
		/*FALL THROUGH*/
	default:
		errno = EINVAL;
		/*FALL THROUGH*/
	case -1:
		/* r= -1 */;
	}
	{ int e= errno; close(afd); errno= e; }
  } else {
	/* Library is cached. */
	r = 0;
  }

  if (r == 0) {
	/* Search the cache. */
	r = searchtab(file, &stp->st_mtime, 1);
	if (stp->st_mtime > armtime) stp->st_mtime = armtime;
  }

bail_out:
  /* Repair the name(file) thing. */
  *lpar= '(';
  *rpar= ')';
  return r;
}
#endif
