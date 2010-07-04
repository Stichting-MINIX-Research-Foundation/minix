/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */
/* ar - archiver		Author: Michiel Huisjes */
/* Made into arch/aal by Ceriel Jacobs
*/

#include <sys/types.h>
#include <fcntl.h>

#include "rd.h"
#include "wr_bytes.h"
#include "wr_long.h"
#include "wr_int2.h"
#include "arch.h"
#include "archiver.h"
#include "print.h"

static char RcsId[] = "$Header$";

/*
 * Usage: [arch|aal] [adprtvx] archive [file] ...
 *	  v: verbose
 *	  x: extract
 *	  a: append
 *	  r: replace (append when not in archive)
 *	  d: delete
 *	  t: print contents of archive
 *	  p: print named files
 *	  l: temporaries in current directory instead of /usr/tmp
 *	  c: don't give "create" message
 *	  u: replace only if dated later than member in archive
#ifdef DISTRIBUTION
 *	  D: make distribution: use distr_time, uid=2, gid=2, mode=0644
#endif
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifndef	S_IREAD
#define	S_IREAD		S_IRUSR
#endif
#ifndef	S_IWRITE
#define	S_IWRITE	S_IWUSR
#endif
#ifndef	S_IEXEC
#define	S_IEXEC		S_IXUSR
#endif
#include <signal.h>
#include <arch.h>
#ifdef AAL
#include <ranlib.h>
#include <out.h>
#define MAGIC_NUMBER	AALMAG
long	offset;
struct ranlib *tab;
unsigned int	tnum = 0;
char	*tstrtab;
unsigned int	tssiz = 0;
long	time();
unsigned int tabsz, strtabsz;
#else
#define MAGIC_NUMBER	ARMAG
#endif
long	lseek();

#define odd(nr)		(nr & 01)
#define even(nr)	(odd(nr) ? nr + 1 : nr)

typedef char BOOL;
#define FALSE		0
#define TRUE		1

#define READ		0
#define APPEND		2
#define CREATE		1

#define MEMBER		struct ar_hdr

#define IO_SIZE		(10 * 1024)

#define equal(str1, str2)	(!strncmp((str1), (str2), 14))
#ifndef S_ISDIR
#define S_ISDIR(m)	(m & S_IFDIR)		/* is a directory */
#endif

BOOL verbose;
BOOL app_fl;
BOOL ex_fl;
BOOL show_fl;
BOOL pr_fl;
BOOL rep_fl;
BOOL del_fl;
BOOL nocr_fl;
BOOL local_fl;
BOOL update_fl;
#ifdef DISTRIBUTION
BOOL distr_fl;
long distr_time;
#endif

int ar_fd;

char io_buffer[IO_SIZE];

char *progname;

char temp_buf[32];
char *temp_arch = &temp_buf[0];
extern char *mktemp();
extern char *ctime();

/* Forward declarations. */
static void enter_name(struct outname *namep);
static void do_names(struct outhead *headp);
static void do_object(int f, long size);
static void show(char *s, char *name);
static void write_symdef(void);
static void mwrite(int fd, char *address, int bytes);
static void extract(register MEMBER *member);
static void copy_member(MEMBER *member, int from, int to, int extracting);
static void add(char *name, int fd, char *mess);
static void get(int argc, char *argv[]);

/*VARARGS2*/
void error1(BOOL quit, char *str1)
{
	fputs(str1,stderr);
  	if (quit) {
		unlink(temp_arch);
		_exit(1);
  	}
}

void error2(BOOL quit, char *str1, char *str2)
{
	fprintf(stderr,str1,str2);
  	if (quit) {
		unlink(temp_arch);
		_exit(1);
  	}
}

void error3(BOOL quit, char *str1, char *str2, char *str3)
{
	fprintf(stderr,str1,str2,str3);
  	if (quit) {
		unlink(temp_arch);
		_exit(1);
  	}
}

void usage()
{
	error3(TRUE, "usage: %s %s archive [file] ...\n",
		progname,
#ifdef AAL
		"[acdrtxvlu]"
#else
		"[acdprtxvlu]"
#endif
		);
}

char *basename(char *path)
{
  register char *ptr = path;
  register char *last = NULL;

  while (*ptr != '\0') {
	if (*ptr == '/')
		last = ptr;
	ptr++;
  }
  if (last == NULL)
	return path;
  if (*(last + 1) == '\0') {
	*last = '\0';
	return basename(path);
  }
  return last + 1;
}

extern unsigned int rd_unsigned2();

int open_archive(char *name, int mode)
{
  unsigned short magic = 0;
  int fd;

  if (mode == CREATE) {
	if ((fd = creat(name, 0666)) < 0)
		error2(TRUE, "cannot creat %s\n", name);
	magic = MAGIC_NUMBER;
	wr_int2(fd, magic);
	return fd;
  }

  if ((fd = open(name, mode)) < 0) {
	if (mode == APPEND) {
		close(open_archive(name, CREATE));
		if (!nocr_fl) error3(FALSE, "%s: creating %s\n", progname, name);
		return open_archive(name, APPEND);
	}
	error2(TRUE, "cannot open %s\n", name);
  }
  lseek(fd, 0L, 0);
  magic = rd_unsigned2(fd);
  if (magic != AALMAG && magic != ARMAG)
	error2(TRUE, "%s is not in ar format\n", name);
  
  return fd;
}

#if __STDC__
void catch(int sig)
#else
catch()
#endif
{
	unlink(temp_arch);
	_exit (2);
}

int main(int argc, char *argv[])
{
  register char *ptr;
  int needs_arg = 0;

  progname = argv[0];

  if (argc < 3)
	usage();
  
  for (ptr = argv[1]; *ptr; ptr++) {
	switch (*ptr) {
		case 't' :
			show_fl = TRUE;
			break;
		case 'v' :
			verbose = TRUE;
			break;
		case 'x' :
			ex_fl = TRUE;
			break;
		case 'a' :
			needs_arg = 1;
			app_fl = TRUE;
			break;
		case 'c' :
			nocr_fl = TRUE;
			break;
#ifndef AAL
		case 'p' :
			needs_arg = 1;
			pr_fl = TRUE;
			break;
#endif
		case 'd' :
			needs_arg = 1;
			del_fl = TRUE;
			break;
		case 'r' :
			needs_arg = 1;
			rep_fl = TRUE;
			break;
		case 'l' :
			local_fl = TRUE;
			break;
		case 'u' :
			update_fl = TRUE;
			break;
#ifdef DISTRIBUTION
		case 'D' :
			distr_fl = TRUE;
			break;
#endif
		default :
			usage();
	}
  }

  if (needs_arg && argc <= 3)
	usage();
#ifdef DISTRIBUTION
  if (distr_fl) {
	struct stat statbuf;

	stat(progname, &statbuf);
	distr_time = statbuf.st_mtime;
  }
#endif
  if (local_fl) strcpy(temp_arch, "ar.XXXXXX");
  else	strcpy(temp_arch, "/usr/tmp/ar.XXXXXX");

  if (app_fl + ex_fl + del_fl + rep_fl + show_fl + pr_fl != 1)
	usage();

  if (update_fl && !rep_fl)
	usage();

  if (rep_fl || del_fl
#ifdef AAL
	|| app_fl
#endif
     ) {
	mktemp(temp_arch);
  }
#ifdef AAL
  tab = (struct ranlib *) malloc(512 * sizeof(struct ranlib));
  tstrtab = malloc(4096);
  if (!tab || !tstrtab) error1(TRUE,"Out of core\n");
  tabsz = 512;
  strtabsz = 4096;
#endif

  signal(SIGINT, catch);
  get(argc, argv);
  
  return 0;
}

MEMBER *
get_member()
{
  static MEMBER member;

again:
  if (rd_arhdr(ar_fd, &member) == 0)
	return NULL;
  if (member.ar_size < 0) {
	error1(TRUE, "archive has member with negative size\n");
  }
#ifdef AAL
  if (equal(SYMDEF, member.ar_name)) {
	lseek(ar_fd, member.ar_size, 1);
	goto again;
  }
#endif
  return &member;
}

char *get_mode();

static void get(int argc, char *argv[])
{
  register MEMBER *member;
  int i = 0;
  int temp_fd, read_chars;

  ar_fd = open_archive(argv[2], (show_fl || pr_fl || ex_fl) ? READ : APPEND);
  if (rep_fl || del_fl
#ifdef AAL
	|| app_fl
#endif
  )
	temp_fd = open_archive(temp_arch, CREATE);
  while ((member = get_member()) != NULL) {
	if (argc > 3) {
		for (i = 3; i < argc; i++) {
			if (equal(basename(argv[i]), member->ar_name))
				break;
		}
		if (i == argc || app_fl) {
			if (rep_fl || del_fl
#ifdef AAL
				|| app_fl
#endif
			) {
#ifdef AAL
				if (i != argc) {
					print("%s: already in archive\n", argv[i]);
					argv[i] = "";
				}
#endif
				wr_arhdr(temp_fd, member);
				copy_member(member, ar_fd, temp_fd, 0);
			}
			else {
#ifndef AAL
				if (app_fl && i != argc) {
					print("%s: already in archive\n", argv[i]);
					argv[i] = "";
				}
#endif
				lseek(ar_fd, even(member->ar_size),1);
			}
			continue;
		}
	}
	if (ex_fl || pr_fl)
		extract(member);
	else {
		if (rep_fl) {
			int isold = 0;
			if(update_fl) {
				struct stat status;
				if (stat(argv[i], &status) >= 0) {
					if(status.st_mtime <= member->ar_date)
						isold = 1;
				}
			}
			if(!isold)
				add(argv[i], temp_fd, "r - %s\n");
			else {
				wr_arhdr(temp_fd, member);
				copy_member(member, ar_fd, temp_fd, 0);
				if(verbose)
					show("r - %s (old)\n", member->ar_name);
			}
		}
		else if (show_fl) {
			char buf[sizeof(member->ar_name) + 2];
			register char *p = buf, *q = member->ar_name;

			while (q <= &member->ar_name[sizeof(member->ar_name)-1] && *q) {
				*p++ = *q++;
			}
			*p++ = '\n';
			*p = '\0';
			if (verbose) {
				char *mode = get_mode(member->ar_mode);
				char *date = ctime(&(member->ar_date));

				*(date + 16) = '\0';
				*(date + 24) = '\0';

				print("%s%3u/%u%7ld %s %s %s",
					mode,
					(unsigned) (member->ar_uid & 0377),
					(unsigned) (member->ar_gid & 0377),
					member->ar_size,
					date+4,
					date+20,
					buf);
			}
			else	print(buf);
		}
		else if (del_fl)
			show("d - %s\n", member->ar_name);
		lseek(ar_fd, even(member->ar_size), 1);
	}
	argv[i] = "";
  }

  if (argc > 3) {
	for (i = 3; i < argc; i++)
		if (argv[i][0] != '\0') {
#ifndef AAL
			if (app_fl)
				add(argv[i], ar_fd, "a - %s\n");
			else
#endif
			if (rep_fl
#ifdef AAL
				|| app_fl
#endif
			)
				add(argv[i], temp_fd, "a - %s\n");
			else {
				print("%s: not found\n", argv[i]);
			}
		}
  }

  if (rep_fl || del_fl
#ifdef AAL
		|| app_fl
#endif
  ) {
	signal(SIGINT, SIG_IGN);
	close(ar_fd);
	close(temp_fd);
	ar_fd = open_archive(argv[2], CREATE);
	temp_fd = open_archive(temp_arch, APPEND);
#ifdef AAL
	write_symdef();
#endif
	while ((read_chars = read(temp_fd, io_buffer, IO_SIZE)) > 0)
		mwrite(ar_fd, io_buffer, read_chars);
	close(temp_fd);
	unlink(temp_arch);
  }
  close(ar_fd);
}

static void add(char *name, int fd, char *mess)
{
  static MEMBER member;
  register int read_chars;
  struct stat status;
  int src_fd;

  if (stat(name, &status) < 0) {
	error2(FALSE, "cannot find %s\n", name);
	return;
  }
  else if (S_ISDIR(status.st_mode)) {
	error2(FALSE, "%s is a directory (ignored)\n", name);
	return;
  }
  else if ((src_fd = open(name, 0)) < 0) {
	error2(FALSE, "cannot open %s\n", name);
	return;
  }

  strncpy (member.ar_name, basename (name), sizeof(member.ar_name));
  member.ar_uid = status.st_uid;
  member.ar_gid = status.st_gid;
  member.ar_mode = status.st_mode;
  member.ar_date = status.st_mtime;
  member.ar_size = status.st_size;
#ifdef DISTRIBUTION
  if (distr_fl) {
	member.ar_uid = 2;
	member.ar_gid = 2;
	member.ar_mode = 0644;
	member.ar_date = distr_time;
  }
#endif
  wr_arhdr(fd, &member);
#ifdef AAL
  do_object(src_fd, member.ar_size);
  lseek(src_fd, 0L, 0);
  offset += AR_TOTAL + even(member.ar_size);
#endif
  while (status.st_size > 0) {
	int x = IO_SIZE;

	read_chars = x;
	if (status.st_size < x) {
		x = status.st_size;
		read_chars = x;
		status.st_size = 0;
		x = even(x);
	}
	else	status.st_size -= x;
  	if (read(src_fd, io_buffer, read_chars) != read_chars) {
		error2(FALSE,"%s seems to shrink\n", name);
		break;
	}
	mwrite(fd, io_buffer, x);
  }

  if (verbose)
	show(mess, member.ar_name);
  close(src_fd);
}

static void extract(register MEMBER *member)
{
  int fd = 1;
  char buf[sizeof(member->ar_name) + 1];

  strncpy(buf, member->ar_name, sizeof(member->ar_name));
  buf[sizeof(member->ar_name)] = 0;
  if (pr_fl == FALSE && (fd = creat(buf, 0666)) < 0) {
	error2(FALSE, "cannot create %s\n", buf);
	fd = -1;
  }

  if (verbose) {
	if (pr_fl == FALSE) show("x - %s\n", buf);
	else show("\n<%s>\n\n", buf);
  }

  copy_member(member, ar_fd, fd, 1);

  if (fd >= 0 && fd != 1)
  	close(fd);
  if (pr_fl == FALSE) chmod(buf, member->ar_mode);
}

static void copy_member(MEMBER *member, int from, int to, int extracting)
{
  register int rest;
  long mem_size = member->ar_size;
  BOOL is_odd = odd(mem_size) ? TRUE : FALSE;

#ifdef AAL
  if (! extracting) {
  	long pos = lseek(from, 0L, 1);

	do_object(from, mem_size);
	offset += AR_TOTAL + even(mem_size);
  	lseek(from, pos, 0);
  }
#endif
  do {
	rest = mem_size > (long) IO_SIZE ? IO_SIZE : (int) mem_size;
	if (read(from, io_buffer, rest) != rest) {
		char buf[sizeof(member->ar_name) + 1];

		strncpy(buf, member->ar_name, sizeof(member->ar_name));
		buf[sizeof(member->ar_name)] = 0;
		error2(TRUE, "read error on %s\n", buf);
	}
	if (to >= 0) mwrite(to, io_buffer, rest);
	mem_size -= (long) rest;
  } while (mem_size > 0L);

  if (is_odd) {
	lseek(from, 1L, 1);
	if (to >= 0 && ! extracting)
		lseek(to, 1L, 1);
  }
}

char *
get_mode(mode)
register int mode;
{
  static char mode_buf[11];
  register int tmp = mode;
  int i;

  mode_buf[9] = ' ';
  for (i = 0; i < 3; i++) {
	mode_buf[i * 3] = (tmp & S_IREAD) ? 'r' : '-';
	mode_buf[i * 3 + 1] = (tmp & S_IWRITE) ? 'w' : '-';
	mode_buf[i * 3 + 2] = (tmp & S_IEXEC) ? 'x' : '-';
	tmp <<= 3;
  }
  if (mode & S_ISUID)
	mode_buf[2] = 's';
  if (mode & S_ISGID)
	mode_buf[5] = 's';
  return mode_buf;
}

void wr_fatal()
{
	error1(TRUE, "write error\n");
}

void rd_fatal()
{
	error1(TRUE, "read error\n");
}

static void mwrite(int fd, char *address, int bytes)
{
  if (write(fd, address, bytes) != bytes)
	error1(TRUE, "write error\n");
}

static void show(char *s, char *name)
{
  MEMBER x;
  char buf[sizeof(x.ar_name)+1];
  register char *p = buf, *q = name;

  while (q <= &name[sizeof(x.ar_name)-1] && *q) *p++ = *q++;
  *p++ = '\0';
  print(s, buf);
}

#ifdef AAL
/*
 * Write out the ranlib table: first 4 bytes telling how many ranlib structs
 * there are, followed by the ranlib structs,
 * then 4 bytes giving the size of the string table, followed by the string
 * table itself.
 */
static void write_symdef(void)
{
	register struct ranlib	*ran;
	register int	i;
	register long	delta;
	MEMBER	arbuf;

	if (odd(tssiz))
		tstrtab[tssiz++] = '\0';
	for (i = 0; i < sizeof(arbuf.ar_name); i++)
		arbuf.ar_name[i] = '\0';
	strcpy(arbuf.ar_name, SYMDEF);
	arbuf.ar_size = 4 + 2 * 4 * (long)tnum + 4 + (long)tssiz;
	time(&arbuf.ar_date);
	arbuf.ar_uid = getuid();
	arbuf.ar_gid = getgid();
	arbuf.ar_mode = 0444;
#ifdef DISTRIBUTION
	if (distr_fl) {
		arbuf.ar_uid = 2;
		arbuf.ar_gid = 2;
		arbuf.ar_date = distr_time;
	}
#endif
	wr_arhdr(ar_fd,&arbuf);
	wr_long(ar_fd, (long) tnum);
	/*
	 * Account for the space occupied by the magic number
	 * and the ranlib table.
	 */
	delta = 2 + AR_TOTAL + arbuf.ar_size;
	for (ran = tab; ran < &tab[tnum]; ran++) {
		ran->ran_pos += delta;
	}

	wr_ranlib(ar_fd, tab, (long) tnum);
	wr_long(ar_fd, (long) tssiz);
	wr_bytes(ar_fd, tstrtab, (long) tssiz);
}

/*
 * Return whether the bytes in `buf' form a good object header. 
 * The header is put in `headp'.
 */
int
is_outhead(register struct outhead *headp)
{
	return !BADMAGIC(*headp) && headp->oh_nname != 0;
}

static void do_object(int f, long size)
{
	struct outhead	headbuf;

	if (size < SZ_HEAD) {
		/* It can't be an object file. */
		return;
	}
	/*
	 * Read a header to see if it is an object file.
	 */
	if (! rd_fdopen(f)) {
		rd_fatal();
	}
	rd_ohead(&headbuf);
	if (!is_outhead(&headbuf)) {
		return;
	}
	do_names(&headbuf);
}

/*
 * First skip the names and read in the string table, then seek back to the
 * name table and read and write the names one by one. Update the ranlib table
 * accordingly.
 */
static void do_names(struct outhead	*headp)
{
	register char	*strings;
	register int	nnames = headp->oh_nname;
#define NNAMES 100
	struct outname	namebuf[NNAMES];
	long xxx = OFF_CHAR(*headp);

	if (	headp->oh_nchar != (unsigned int)headp->oh_nchar ||
		(strings = malloc((unsigned int)headp->oh_nchar)) == (char *)0
	   ) {
		error1(TRUE, "string table too big\n");
	}
	rd_string(strings, headp->oh_nchar);
	while (nnames) {
		int i = nnames >= NNAMES ? NNAMES : nnames;
		register struct outname *p = namebuf;

		nnames -= i;
		rd_name(namebuf, i);
		while (i--) {
			long off = p->on_foff - xxx;
			if (p->on_foff == (long)0) {
				p++;
				continue; /* An unrecognizable name. */
			}
			p->on_mptr = strings + off;
			/*
			 * Only enter names that are exported and are really
			 * defined. Also enter common names. Note, that
			 * this might cause problems when the name is really
			 * defined in a later file, with a value != 0.
			 * However, this problem also exists on the Unix
			 * ranlib archives.
			 */
			if (	(p->on_type & S_EXT) &&
				(p->on_type & S_TYP) != S_UND
			   )
				enter_name(p);
			p++;
		}
	}
	free(strings);
}

static void enter_name(struct outname *namep)
{
	register char	*cp;

	if (tnum >= tabsz) {
		tab = (struct ranlib *)
			realloc((char *) tab, (tabsz += 512) * sizeof(struct ranlib));
		if (! tab) error1(TRUE, "Out of core\n");
	}
	tab[tnum].ran_off = tssiz;
	tab[tnum].ran_pos = offset;

	for (cp = namep->on_mptr;; cp++) {
		if (tssiz >= strtabsz) {
			tstrtab = realloc(tstrtab, (strtabsz += 4096));
			if (! tstrtab) error1(TRUE, "string table overflow\n");
		}
		tstrtab[tssiz++]  = *cp;
		if (!*cp) break;
	}
	tnum++;
}
#endif /* AAL */
