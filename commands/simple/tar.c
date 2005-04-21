/* tar - tape archiver			Author: Michiel Huisjes */

/* Usage: tar [cxt][vo][F][f] tapefile [files]
 *
 * attempt to make tar to conform to POSIX 1003.1
 * disclaimer: based on an old (1986) POSIX draft.
 * Klamer Schutte, 20/9/89
 *
 * Changes:
 *  Changed to handle the original minix-tar format.	KS 22/9/89
 *  Changed to handle BSD4.3 tar format.		KS 22/9/89
 *  Conform to current umask if not super-user.		KS 22/9/89
 *  Update usage message to show f option		KS 22/9/89
 *
 *
 * 1)	tar will back itself up, should check archive inode num(&dev) and
  then check the target inode number. In verbose mode, issue
  warning, in all cases ignore target.
  marks@mgse		Mon Sep 25 10:38:58 CDT 1989
  	added global varaibles, made changes to main() and add_file();
  maks@mgse Mon Sep 25 12:09:20 CDT 1989

   2)	tar will not notice that a file has changed size while it was being
  backed up. should issue warning.
  marks@mgse		Mon Sep 25 10:38:58 CDT 1989

   3)	the 'f' option was not documented in usage[].
  marks@mgse		Mon Sep 25 12:03:20 CDT 1989
  	changed both usage[] defines. Why are there two (one is commented out)?
  	( deleted by me (was done twice) -- KS, 2/10/89 )
 *
 *  changed stat on tar_fd to an fstat				KS 2/10/89
 *  deleted mkfifo() code -- belongs in libc.a			KS 2/10/89
 *  made ar_dev default to -1 : an illegal device		KS 2/10/89
 *  made impossible to chown if normal user			KS 2/10/89
 *  if names in owner fields not known use numirical values	KS 2/10/89
 *  creat with mask 666 -- use umask if to liberal		KS 2/10/89
 *  allow to make directories as ../directory			KS 2/10/89
 *  allow tmagic field to end with a space (instead of \0)	KS 2/10/89
 *  correct usage of tmagic field 				KS 3/10/89
 *  made mkdir() to return a value if directory == "."  	KS 3/10/89
 *  made lint complains less (On a BSD 4.3 system)		KS 3/10/89
 *  use of directory(3) routines				KS 3/10/89
 *  deleted use of d_namlen selector of struct dirent		KS 18/10/89
 *  support mknod4(2)						EC 7/7/90
 *  forget inodes when link count expires			EC 6/4/91
 *  don't remember directories *twice*!
 *  added 'p' flag to ignore umask for normal user		KJB 6/10/92
 *  mknod4(2) out						KJB 30/10/94
 *  added 'D' flag to not recurse into directories		KJB 19/12/94
 *  status output to stdout unless 'tar cvf -'			KJB 3/5/97
 *
 * Bugs:
 *  verbose mode is not reporting consistent
 *  code needs cleanup
 *  prefix field is not used
 *  timestamp of a directory will not be correct if there are files to be
 *  unpacked in the directory
 *	(add you favorite bug here (or two (or three (or ...))))
*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pwd.h>
#include <grp.h>
#include <tar.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <utime.h>
#include <sys/wait.h>
#include <stdio.h>		/* need NULL */
#include <errno.h>

#define	POSIX_COMP		/* POSIX compatible */
#define DIRECT_3		/* use directory(3) routines */

#ifdef DIRECT_3
#ifndef BSD
/* To all minix users: i am sorry, developed this piece of code on a
 * BSD system. KS 18/10/89 */
#include <dirent.h>
#define	direct	dirent		/* stupid BSD non-POSIX compatible name! */
#else				/* BSD */
#include <sys/dir.h>
#include <dir.h>
#endif				/* BSD */
#endif				/* DIRECT_3 */

#ifdef S_IFIFO
#define	HAVE_FIFO		/* have incorporated Simon Pooles' changes */
#endif
#ifdef S_IFLNK
#define HAVE_SYMLINK
#endif

typedef char BOOL;
#define TRUE	1
#define FALSE	0

#define STRING_SIZE	256	/* string buffer size */
#define HEADER_SIZE	TBLOCK
#define NAME_SIZE	NAMSIZ
/* #define BLOCK_BOUNDARY	 20 -- not in POSIX ! */

typedef union hblock HEADER;

/* Make the MINIX member names overlap to the POSIX names */
#define	m_name		name
#define m_mode		mode
#define m_uid		uid
#define m_gid		gid
#define m_size		size
#define	m_time		mtime
#define	m_checksum	chksum
#define	m_linked	typeflag
#define	m_link		linkname
#define	hdr_block	dummy
#define	m		header
#define	member		dbuf

#if 0				/* original structure -- see tar.h for new
			 * structure */
typedef union {
  char hdr_block[HEADER_SIZE];
  struct m {
	char m_name[NAME_SIZE];
	char m_mode[8];
	char m_uid[8];
	char m_gid[8];
	char m_size[12];
	char m_time[12];
	char m_checksum[8];
	char m_linked;
	char m_link[NAME_SIZE];
  } member;
} HEADER;

#endif

/* Structure used to note links */
struct link {
  ino_t ino;
  dev_t dev;
  nlink_t nlink;
  struct link *next;
  char name[1];
} *link_top = NULL;

HEADER header;

#define INT_TYPE	(sizeof(header.member.m_uid))
#define LONG_TYPE	(sizeof(header.member.m_size))

#define NIL_HEADER	((HEADER *) 0)
#define NIL_PTR		((char *) 0)
#define TBLOCK_SIZE	TBLOCK

#define flush()		print(NIL_PTR)

BOOL show_fl, creat_fl, ext_fl;

int tar_fd;
/* Char usage[] = "Usage: tar [cxt] tarfile [files]."; */
char usage[] = "Usage: tar [cxt][vo][F][f] tarfile [files].";
char io_buffer[TBLOCK_SIZE];
char path[NAME_SIZE];
char pathname[NAME_SIZE];
int force_flag = 0;
#ifdef ORIGINAL_DEFAULTS
int chown_flag = 1;
int verbose_flag = 1;
#else
int chown_flag = 0;
int verbose_flag = 0;
#endif
int norec_flag = 0;

/* Make sure we don't tar ourselves. marks@mgse Mon Sep 25 12:06:28 CDT 1989 */
ino_t ar_inode;			/* archive inode number	 */
dev_t ar_dev;			/* archive device number */

int total_blocks;
int u_mask;			/* one's complement of current umask */

#define block_size()	(int) ((convert(header.member.m_size, LONG_TYPE) \
  + (long) TBLOCK_SIZE - 1) / (long) TBLOCK_SIZE)

_PROTOTYPE(int main, (int argc, char **argv));
_PROTOTYPE(void error, (char *s1, char *s2));
_PROTOTYPE(BOOL get_header, (void));
_PROTOTYPE(void tarfile, (void));
_PROTOTYPE(void skip_entry, (void));
_PROTOTYPE(void extract, (char *file));
_PROTOTYPE(void delete, (char *file));
_PROTOTYPE(void do_chown, (char *file));
_PROTOTYPE(void timestamp, (char *file));
_PROTOTYPE(void copy, (char *file, int from, int to, long bytes));
_PROTOTYPE(long convert, (char str[], int type));
_PROTOTYPE(int checksum, (void));
_PROTOTYPE(int is_dir, (char *file));
_PROTOTYPE(char *path_name, (char *file));
_PROTOTYPE(void add_path, (char *name));
_PROTOTYPE(void add_file, (char *file));
_PROTOTYPE(void verb_print, (char *s1, char *s2));
_PROTOTYPE(void add_close, (int fd));
_PROTOTYPE(int add_open, (char *file, struct stat * st));
_PROTOTYPE(void make_header, (char *file, struct stat * st));
_PROTOTYPE(void is_added, (struct stat * st, char *file));
_PROTOTYPE(void is_deleted, (struct stat * st));
_PROTOTYPE(char *is_linked, (struct stat * st));
_PROTOTYPE(void clear_header, (void));
_PROTOTYPE(void adjust_boundary, (void));
_PROTOTYPE(void mread, (int fd, char *address, int bytes));
_PROTOTYPE(void mwrite, (int fd, char *address, int bytes));
_PROTOTYPE(int bread, (int fd, char *address, int bytes));
_PROTOTYPE(int bwrite, (int fd, char *address, int bytes));
_PROTOTYPE(void print, (char *str));
_PROTOTYPE(char *num_out, (long number));
_PROTOTYPE(void string_print, (char *buffer, char *fmt,...));

void error(s1, s2)
char *s1, *s2;
{
  string_print(NIL_PTR, "%s %s\n", s1, s2 ? s2 : "");
  flush();
  exit(1);
}

int main(argc, argv)
int argc;
register char *argv[];
{
  register char *mem_name;
  register char *ptr;
  struct stat st;
  int i;

  if (argc < 3) error(usage, NIL_PTR);

  for (ptr = argv[1]; *ptr; ptr++) {
	switch (*ptr) {
	    case 'c':	creat_fl = TRUE;	break;
	    case 'x':	ext_fl = TRUE;	break;
	    case 't':	show_fl = TRUE;	break;
	    case 'v':		/* verbose output  -Dal */
		verbose_flag = !verbose_flag;
		break;
	    case 'o':		/* chown/chgrp files  -Dal */
		chown_flag = TRUE;
		break;
	    case 'F':		/* IGNORE ERRORS  -Dal */
		force_flag = TRUE;
		break;
	    case 'f':		/* standard U*IX usage -KS */
		break;
	    case 'p':		/* restore file modes right, ignore umask. */
		(void) umask(0);
		break;
	    case 'D':		/* do not recursively add directories. */
		norec_flag = TRUE;
		break;
	    default:	error(usage, NIL_PTR);
	}
  }

  if (creat_fl + ext_fl + show_fl != 1) error(usage, NIL_PTR);

  if (strcmp(argv[2], "-") == 0)/* only - means stdin/stdout - KS */
	tar_fd = creat_fl ? 1 : 0;	/* '-' means used
					 * stdin/stdout  -Dal */
  else
	tar_fd = creat_fl ? creat(argv[2], 0666) : open(argv[2], O_RDONLY);

  if (tar_fd < 0) error("Cannot open ", argv[2]);

  if (geteuid()) {		/* check if super-user */
	int save_umask;
	save_umask = umask(0);
	u_mask = ~save_umask;
	umask(save_umask);
	chown_flag = TRUE;	/* normal user can't chown */
  } else
	u_mask = ~0;		/* don't restrict if 'privileged utility' */

  ar_dev = -1;			/* impossible device nr */
  if (creat_fl) {
	if (tar_fd > 1 && fstat(tar_fd, &st) < 0)
		error("Can't stat ", argv[2]);	/* will never be here,
						 * right? */
	else {			/* get archive inode & device	 */
		ar_inode = st.st_ino;	/* save files inode	 */
		ar_dev = st.st_dev;	/* save files device	 */
	}			/* marks@mgse Mon Sep 25 11:30:45 CDT 1989 */

	for (i = 3; i < argc; i++) {
		add_file(argv[i]);
		path[0] = '\0';
	}
	adjust_boundary();
  } else if (ext_fl) {
	/* Extraction code moved here from tarfile() MSP */
	while (get_header()) {
		mem_name = header.member.m_name;
		if (is_dir(mem_name)) {
			for (ptr = mem_name; *ptr; ptr++);
			*(ptr - 1) = '\0';
			header.dbuf.typeflag = '5';
		}
		for (i = 3; i < argc; i++)
			if (!strncmp(argv[i], mem_name, strlen(argv[i])))
				break;
		if (argc == 3 || (i < argc)) {
			extract(mem_name);
		} else if (header.dbuf.typeflag == '0' ||
			   header.dbuf.typeflag == 0 ||
			   header.dbuf.typeflag == ' ')
			skip_entry();
		flush();
	}
  } else
	tarfile();		/* tarfile() justs prints info. now MSP */

  flush();
  return(0);
}

BOOL get_header()
{
  register int check;

  mread(tar_fd, (char *) &header, sizeof(header));
  if (header.member.m_name[0] == '\0') return FALSE;

  if (force_flag)		/* skip checksum verification  -Dal */
	return TRUE;

  check = (int) convert(header.member.m_checksum, INT_TYPE);

  if (check != checksum()) error("Tar: header checksum error.", NIL_PTR);

  return TRUE;
}

/* Tarfile() just lists info about archive now; as of the t flag. */
/* Extraction has been moved into main() as that needs access to argv[] */

void tarfile()
{
  register char *mem_name;

  while (get_header()) {
	mem_name = header.member.m_name;
	string_print(NIL_PTR, "%s%s", mem_name,
		     (verbose_flag ? " " : "\n"));
	switch (header.dbuf.typeflag) {
	    case '1':
		verb_print("linked to", header.dbuf.linkname);
		break;
	    case '2':
		verb_print("symbolic link to", header.dbuf.linkname);
		break;
	    case '6':	verb_print("", "fifo");	break;
	    case '3':
	    case '4':
		if (verbose_flag) {
			char sizebuf[TSIZLEN + 1];

			strncpy(sizebuf, header.dbuf.size, (size_t) TSIZLEN);
			sizebuf[TSIZLEN] = 0;
			string_print(NIL_PTR,
			      "%s special file major %s minor %s\n",
				     (header.dbuf.typeflag == '3' ?
				      "character" : "block"),
				     header.dbuf.devmajor,
				     header.dbuf.devminor,
				     sizebuf);
		}
		break;
	    case '0':		/* official POSIX */
	    case 0:		/* also mentioned in POSIX */
	    case ' ':		/* ofetn used */
		if (!is_dir(mem_name)) {
			if (verbose_flag)
				string_print(NIL_PTR, "%d tape blocks\n",
					     block_size());
			skip_entry();
			break;
		} else		/* FALL TROUGH */
	    case '5':
			verb_print("", "directory");
		break;
	    default:
		string_print(NIL_PTR, "not recogised item %d\n",
			     header.dbuf.typeflag);
	}
	flush();
  }
}

void skip_entry()
{
  register int blocks = block_size();

  while (blocks--) (void) bread(tar_fd, io_buffer, TBLOCK_SIZE);
}

void extract(file)
register char *file;
{
  register int fd, r;
  char *pd1, *pd2;		/* walk thru failed directory path */

  switch (header.dbuf.typeflag) {
      case '1':			/* Link */
	delete(file);
	if (link(header.member.m_link, file) < 0)
		string_print(NIL_PTR, "Cannot link %s to %s: %s\n",
			     header.member.m_link, file, strerror(errno));
	else if (verbose_flag)
		string_print(NIL_PTR, "Linked %s to %s\n",
			     header.member.m_link, file);
	return;
      case '5':			/* directory */
	if (!(file[0] == '.' && file[1] == '\0')) delete(file);
	if ((file[0] == '.' && file[1] == '\0') || mkdir(file, 0700) == 0) {
		do_chown(file);
		verb_print("created directory", file);
	} else {
		string_print(NIL_PTR, "Can't make directory %s: %s\n",
				file, strerror(errno));
	}
	return;
      case '3':			/* character special */
      case '4':			/* block special */
	{
		int dmajor, dminor, mode;

		dmajor = (int) convert(header.dbuf.devmajor, INT_TYPE);
		dminor = (int) convert(header.dbuf.devminor, INT_TYPE);
		mode = (header.dbuf.typeflag == '3' ? S_IFCHR : S_IFBLK);
		delete(file);
		if (mknod(file, mode, (dmajor << 8 | dminor)) == 0) {
			if (verbose_flag) string_print(NIL_PTR,
				     "made %s special file major %s minor %s\n",
				      (header.dbuf.typeflag == '3' ?
				       "character" : "block"),
					     header.dbuf.devmajor,
					     header.dbuf.devminor);
			do_chown(file);
		}
		else 
		{
			string_print(NIL_PTR,
				     "cannot make %s special file major %s minor %s: %s\n",
				      (header.dbuf.typeflag == '3' ?
				       "character" : "block"),
					     header.dbuf.devmajor,
					     header.dbuf.devminor,
					     strerror(errno));
		}
		return;
	}
      case '2':			/* symbolic link */
#ifdef HAVE_SYMLINK
	delete(file);
	if (symlink(header.member.m_link, file) < 0)
		string_print(NIL_PTR, "Cannot make symbolic link %s to %s: %s\n",
			     header.member.m_link, file, strerror(errno));
	else if (verbose_flag)
		string_print(NIL_PTR, "Symbolic link %s to %s\n",
			     header.member.m_link, file);
	return;
#endif
      case '7':			/* contiguous file -- what is this (KS) */
	print("Not implemented file type\n");
	return;			/* not implemented, but break out */
#ifdef HAVE_FIFO
      case '6':			/* fifo */
	delete(file);
	if (mkfifo(file, 0) == 0) {	/* is chmod'ed in do_chown */
		do_chown(file);
		verb_print("made fifo", file);
	} else
		string_print(NIL_PTR, "Can't make fifo %s: %s\n",
			file, strerror(errno));
	return;
#endif
  }

  /* Create regular file.  If failure, try to make missing directories. */
  if ((fd = creat(file, 0600)) < 0) {
	pd1 = file;
	while ((pd2 = index(pd1, '/')) > (char *) 0) {
		*pd2 = '\0';
		if (access(file, 1) < 0)
			if (mkdir(file, 0777) < 0) {
				string_print(NIL_PTR, "Cannot mkdir %s: %s\n",
					file, strerror(errno));
				return;
			} else
				string_print(NIL_PTR, "Made directory %s\n", file);
		*pd2 = '/';
		pd1 = ++pd2;
	}
	if ((fd = creat(file, 0600)) < 0) {
		string_print(NIL_PTR, "Cannot create %s: %s\n",
			file, strerror(errno));
		return;
	}
  }
  copy(file, tar_fd, fd, convert(header.member.m_size, LONG_TYPE));
  (void) close(fd);

  do_chown(file);
}

void delete(file)
char *file;
{
  /* remove a file or an empty directory */
  struct stat stbuf;

  if (stat(file, &stbuf) < 0) return;

  if (S_ISDIR(stbuf.st_mode)) (void) rmdir(file); else (void) unlink(file);
  /* leave error reporting to the create following soon. */
}

void do_chown(file)
char *file;
{
  int uid = -1, gid = -1;	/* these are illegal ??? -- KS */

  if (!chown_flag) {		/* set correct owner and group  -Dal */
	if (header.dbuf.magic[TMAGLEN] == ' ')
		header.dbuf.magic[TMAGLEN] = '\0';	/* some tars out there
							 * ... */
	if (strncmp(TMAGIC, header.dbuf.magic, (size_t) TMAGLEN)) {
		struct passwd *pwd;
		struct group *grp;

		pwd = getpwnam(header.dbuf.uname);
		if (pwd != NULL) uid = pwd->pw_uid;
		grp = getgrnam(header.dbuf.gname);
		if (grp != NULL) gid = grp->gr_gid;
	}
	if (uid == -1) uid = (int) convert(header.member.m_uid, INT_TYPE);
	if (gid == -1) gid = (int) convert(header.member.m_gid, INT_TYPE);
	chown(file, uid, gid);
  }
  chmod(file, u_mask & (int) convert(header.member.m_mode, INT_TYPE));

  /* Should there be a timestamp if the chown failes? -- KS */
  timestamp(file);

}

void timestamp(file)
char *file;
{
  struct utimbuf buf;

  buf.modtime = buf.actime = convert(header.dbuf.mtime, LONG_TYPE);
  utime(file, &buf);
}

void copy(file, from, to, bytes)
char *file;
int from, to;
register long bytes;
{
  register int rest;
  int blocks = (int) ((bytes + (long) TBLOCK_SIZE - 1) / (long) TBLOCK_SIZE);

  if (verbose_flag)
	string_print(NIL_PTR, "%s, %d tape blocks\n", file, blocks);

  while (blocks--) {
	(void) bread(from, io_buffer, TBLOCK_SIZE);
	rest = (bytes > (long) TBLOCK_SIZE) ? TBLOCK_SIZE : (int) bytes;
	mwrite(to, io_buffer, (to == tar_fd) ? TBLOCK_SIZE : rest);
	bytes -= (long) rest;
  }
}

long convert(str, type)
char str[];
int type;
{
  register long ac = 0L;
  register int i;

  for (i = 0; i < type; i++) {
	if (str[i] >= '0' && str[i] <= '7') {
		ac <<= 3;
		ac += (long) (str[i] - '0');
	}
  }

  return ac;
}

int checksum()
{
  register char *ptr = header.member.m_checksum;
  register int ac = 0;

  while (ptr < &header.member.m_checksum[INT_TYPE]) *ptr++ = ' ';

  ptr = header.hdr_block;
  while (ptr < &header.hdr_block[TBLOCK_SIZE]) ac += *ptr++;

  return ac;
}

int is_dir(file)
register char *file;
{
  while (*file++ != '\0');

  return(*(file - 2) == '/');
}


char *path_name(file)
register char *file;
{

  string_print(pathname, "%s%s", path, file);
  return pathname;
}

void add_path(name)
register char *name;
{
  register char *path_ptr = path;

  while (*path_ptr) path_ptr++;

  if (name == NIL_PTR) {
	while (*path_ptr-- != '/');
	while (*path_ptr != '/' && path_ptr != path) path_ptr--;
	if (*path_ptr == '/') path_ptr++;
	*path_ptr = '\0';
  } else {
	while (*name) {
		if (path_ptr == &path[NAME_SIZE])
			error("Pathname too long", NIL_PTR);
		*path_ptr++ = *name++;
	}
	*path_ptr++ = '/';
	*path_ptr = '\0';
  }
}

/*
 *	add a file to the archive
*/
void add_file(file)
register char *file;
{
  struct stat st;
  char *linkname;
  register int fd = -1;
  char namebuf[16];		/* -Dal */
  char cwd[129];		/* -KS */

#ifdef HAVE_SYMLINK
  if (lstat(file, &st) < 0) {
#else
  if (stat(file, &st) < 0) {
#endif
	string_print(NIL_PTR, "%s: %s\n", file, strerror(errno));
	return;
  }
  if (st.st_dev == ar_dev && st.st_ino == ar_inode) {
	string_print(NIL_PTR, "Cannot tar current archive file (%s)\n", file);
	return;
  }				/* marks@mgse Mon Sep 25 12:06:28 CDT 1989 */
  if ((fd = add_open(file, &st)) < 0) {
	string_print(NIL_PTR, "Cannot open %s\n", file);
	return;
  }
  make_header(path_name(file), &st);
  if ((linkname = is_linked(&st)) != NULL) {
	strncpy(header.dbuf.linkname, linkname, (size_t) NAMSIZ);
	header.dbuf.typeflag = '1';
	if (verbose_flag) string_print(NIL_PTR, "linked %s to %s\n",
			     header.dbuf.linkname, file);
	string_print(header.member.m_checksum, "%I ", checksum());
	mwrite(tar_fd, (char *) &header, sizeof(header));
  } else {
	is_added(&st, file);
	switch (st.st_mode & S_IFMT) {
	    case S_IFREG:
		header.dbuf.typeflag = '0';
		string_print(header.member.m_checksum, "%I ", checksum());
		mwrite(tar_fd, (char *) &header, sizeof(header));
		copy(path_name(file), fd, tar_fd, (long) st.st_size);
		break;
	    case S_IFDIR:
		header.dbuf.typeflag = '5';
		string_print(header.member.m_checksum, "%I ", checksum());
		mwrite(tar_fd, (char *) &header, sizeof(header));
		verb_print("read directory", file);
		if (norec_flag) break;
		if (NULL == getcwd(cwd, (int) sizeof cwd))
			string_print(NIL_PTR, "Error: cannot getcwd()\n");
		else if (chdir(file) < 0)
			string_print(NIL_PTR, "Cannot chdir to %s: %s\n",
				file, strerror(errno));
		else {
			add_path(file);
#ifdef	DIRECT_3
			{
				DIR *dirp;
				struct direct *dp;
				struct stat dst;

				add_close(fd);
				fd= 0;
				dirp = opendir(".");
				while (NULL != (dp = readdir(dirp)))
					if (strcmp(dp->d_name, ".") == 0)
						is_linked(&st);
					else if (strcmp(dp->d_name, "..") == 0) {
						if (stat("..", &dst) == 0)
							is_linked(&dst);
					} else {
						strcpy(namebuf, dp->d_name);
						add_file(namebuf);
					}
				closedir(dirp);
			}
#else
			{
				int i;
				struct direct dir;
				struct stat dst;

				for (i = 0; i < 2; i++) {	/* . and .. */
					mread(fd, &dir, sizeof(dir));
					if (strcmp(dir.d_name, ".") == 0)
						is_linked(&st);
					else if (strcmp(dir.d_name, "..") == 0) {
						if (stat("..", &dst) == 0)
							is_linked(&dst);
					} else
						break;
				}
				while (bread(fd, &dir, sizeof(dir)) == sizeof(dir))
					if (dir.d_ino) {
						strncpy(namebuf, dir.d_name,
						   (size_t) DIRSIZ);
						namebuf[DIRSIZ] = '\0';
						add_file(namebuf);
					}
			}
#endif
			chdir(cwd);
			add_path(NIL_PTR);
			*file = 0;
		}
		break;
#ifdef HAVE_SYMLINK
	    case S_IFLNK:
		{
			int i;

			header.dbuf.typeflag = '2';
			verb_print("read symlink", file);
			i = readlink(file,
				     header.dbuf.linkname,
				  sizeof(header.dbuf.linkname) - 1);
			if (i < 0) {
				string_print(NIL_PTR,
					"Cannot read symbolic link %s: %s\n",
					file, strerror(errno));
				return;
			}
			header.dbuf.linkname[i] = 0;
			string_print(header.member.m_checksum, "%I ", checksum());
			mwrite(tar_fd, (char *) &header, sizeof(header));
			break;
		}
#endif
#ifdef HAVE_FIFO
	    case S_IFIFO:
		header.dbuf.typeflag = '6';
		verb_print("read fifo", file);
		string_print(header.member.m_checksum, "%I ", checksum());
		mwrite(tar_fd, (char *) &header, sizeof(header));
		break;
#endif
	    case S_IFBLK:
		header.dbuf.typeflag = '4';
		if (verbose_flag) {
			char sizebuf[TSIZLEN + 1];

			strncpy(sizebuf, header.dbuf.size, (size_t) TSIZLEN);
			sizebuf[TSIZLEN] = 0;
			string_print(NIL_PTR,
			 "read block device %s major %s minor %s\n",
				     file, header.dbuf.devmajor, header.dbuf.devminor, sizebuf);
		}
		string_print(header.member.m_checksum, "%I ", checksum());
		mwrite(tar_fd, (char *) &header, sizeof(header));
		break;
	    case S_IFCHR:
		header.dbuf.typeflag = '3';
		if (verbose_flag) string_print(NIL_PTR,
				     "read character device %s major %s minor %s\n",
				     file, header.dbuf.devmajor, header.dbuf.devminor);
		string_print(header.member.m_checksum, "%I ", checksum());
		mwrite(tar_fd, (char *) &header, sizeof(header));
		break;
	    default:
		is_deleted(&st);
		string_print(NIL_PTR, "Tar: %s unknown file type. Not added.\n", file);
		*file = 0;
	}
  }

  flush();
  add_close(fd);
}

void verb_print(s1, s2)
char *s1, *s2;
{
  if (verbose_flag) string_print(NIL_PTR, "%s: %s\n", s1, s2);
}

void add_close(fd)
int fd;
{
  if (fd != 0) close(fd);
}

/*
 *	open file 'file' to be added to archive, return file descriptor
*/
int add_open(file, st)
char *file;
struct stat *st;
{
  int fd;
  if (((st->st_mode & S_IFMT) != S_IFREG) &&
      ((st->st_mode & S_IFMT) != S_IFDIR))
	return 0;
  fd = open(file, O_RDONLY);
  if (fd == -1)
  	fprintf(stderr, "open failed: %s\n", strerror(errno));
  return fd;
}

void make_header(file, st)
char *file;
register struct stat *st;
{
  register char *ptr = header.member.m_name;
  struct passwd *pwd;
  struct group *grp;

  clear_header();

  while (*ptr++ = *file++);

  if ((st->st_mode & S_IFMT) == S_IFDIR) {	/* fixed test  -Dal */
	*(ptr - 1) = '/';
  }
  string_print(header.member.m_mode, "%I ", st->st_mode & 07777);
  string_print(header.member.m_uid, "%I ", st->st_uid);
  string_print(header.member.m_gid, "%I ", st->st_gid);
  if ((st->st_mode & S_IFMT) == S_IFREG)
	string_print(header.member.m_size, "%L ", st->st_size);
  else
	strncpy(header.dbuf.size, "0", (size_t) TSIZLEN);
  string_print(header.member.m_time, "%L ", st->st_mtime);
  strncpy(header.dbuf.magic, TMAGIC, (size_t) TMAGLEN);
  header.dbuf.version[0] = 0;
  header.dbuf.version[1] = 0;
  pwd = getpwuid(st->st_uid);
  strncpy(header.dbuf.uname,
	(pwd != NULL ? pwd->pw_name : "nobody"), TUNMLEN);
  grp = getgrgid(st->st_gid);
  strncpy(header.dbuf.gname,
	(grp != NULL ? grp->gr_name : "nobody"), TGNMLEN);
  if (st->st_mode & (S_IFBLK | S_IFCHR)) {
	string_print(header.dbuf.devmajor, "%I ", (st->st_rdev >> 8));
	string_print(header.dbuf.devminor, "%I ", (st->st_rdev & 0xFF));
  }
  header.dbuf.prefix[0] = 0;
}

void is_added(st, file)
struct stat *st;
char *file;
{
  struct link *new;
  char *name;

  if ((*file == 0) || (st->st_nlink == 1)) return;
  name = path_name(file);
  new = (struct link *) malloc(sizeof(struct link) + strlen(name));
  if (new == NULL) {
	print("Out of memory\n");
	return;
  }
  new->next = link_top;
  new->dev = st->st_dev;
  new->ino = st->st_ino;
  new->nlink = st->st_nlink - 1;
  strcpy(new->name, name);
  link_top = new;
}

void is_deleted(st)
struct stat *st;
{
  struct link *old;

  if ((old = link_top) != NULL) {
	link_top = old->next;
	free(old);
  }
}

char *is_linked(st)
struct stat *st;
{
  struct link *cur = link_top;
  struct link **pre = &link_top;
  static char name[NAMSIZ];

  while (cur != NULL)
	if ((cur->dev != st->st_dev) || (cur->ino != st->st_ino)) {
		pre = &cur->next;
		cur = cur->next;
	} else {
		if (--cur->nlink == 0) {
			*pre = cur->next;
			strncpy(name, cur->name, NAMSIZ);
			return name;
		}
		return cur->name;
	}
  return NULL;
}

void clear_header()
{
  register char *ptr = header.hdr_block;

  while (ptr < &header.hdr_block[TBLOCK_SIZE]) *ptr++ = '\0';
}

void adjust_boundary()
{
  clear_header();
  mwrite(tar_fd, (char *) &header, sizeof(header));
#ifndef POSIX_COMP
  while (total_blocks++ < BLOCK_BOUNDARY)
	mwrite(tar_fd, (char *) &header, sizeof(header));
#else
  mwrite(tar_fd, (char *) &header, sizeof(header));
#endif
  (void) close(tar_fd);
}

void mread(fd, address, bytes)
int fd, bytes;
char *address;
{
  if (bread(fd, address, bytes) != bytes) error("Tar: read error.", NIL_PTR);
}

void mwrite(fd, address, bytes)
int fd, bytes;
char *address;
{
  if (bwrite(fd, address, bytes) != bytes) error("Tar: write error.", NIL_PTR);

  total_blocks++;
}

int bread(fd, address, bytes)
int fd, bytes;
char *address;
{
  int n = 0, r;

  while (n < bytes) {
	if ((r = read(fd, address + n, bytes - n)) <= 0) {
		if (r < 0) return r;
		break;
	}
	n += r;
  }
  return n;
}

int bwrite(fd, address, bytes)
int fd, bytes;
char *address;
{
  int n = 0, r;

  while (n < bytes) {
	if ((r = write(fd, address + n, bytes - n)) <= 0) {
		if (r < 0) return r;
		break;
	}
	n += r;
  }
  return n;
}

char output[TBLOCK_SIZE];
void print(str)
register char *str;
{
  int fd = (tar_fd == 1 ? 2 : 1);
  static int indx = 0;

  if (str == NIL_PTR) {
	write(fd, output, indx);
	indx = 0;
	return;
  }
  while (*str) {
	output[indx++] = *str++;
	if (indx == TBLOCK_SIZE) {
		write(fd, output, TBLOCK_SIZE);
		indx = 0;
	}
  }
}

char *num_out(number)
register long number;
{
  static char num_buf[12];
  register int i;

  for (i = 11; i--;) {
	num_buf[i] = (number & 07) + '0';
	number >>= 3;
  }

  return num_buf;
}

/*VARARGS2*/
#if __STDC__
void string_print(char *buffer, char *fmt,...)
#else
void string_print(buffer, fmt)
char *buffer;
char *fmt;
#endif
{
  va_list args;
  register char *buf_ptr;
  char *scan_ptr;
  char buf[STRING_SIZE];
  BOOL pr_fl, i;

  if (pr_fl = (buffer == NIL_PTR)) buffer = buf;

  va_start(args, fmt);
  buf_ptr = buffer;
  while (*fmt) {
	if (*fmt == '%') {
		fmt++;
		switch (*fmt++) {
		    case 's':
			scan_ptr = (char *) (va_arg(args, char *));
			break;
		    case 'I':
			scan_ptr = num_out((long) (va_arg(args, int)));
			for (i = 0; i < 5; i++) scan_ptr++;
			break;
		    case 'L':
			scan_ptr = num_out((long) va_arg(args, long));
			break;
		    case 'd':
			scan_ptr = num_out((long) va_arg(args, int));
			while (*scan_ptr == '0') scan_ptr++;
			scan_ptr--;
			break;
		    default:	scan_ptr = "";
		}
		while (*buf_ptr++ = *scan_ptr++);
		buf_ptr--;
	} else
		*buf_ptr++ = *fmt++;
  }
  *buf_ptr = '\0';

  if (pr_fl) print(buffer);
  va_end(args);
}
