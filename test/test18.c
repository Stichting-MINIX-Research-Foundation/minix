/* test 18 */

/* Comment on usage and program: ark!/mnt/rene/prac/os/unix/comment.changes */

/* "const.h", created by Rene Montsma and Menno Wilcke */

#include <sys/types.h>		/* needed in struct stat */
#include <sys/stat.h>		/* struct stat */
#include <sys/wait.h>
#include <errno.h>		/* the error-numbers */
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#define NOCRASH 1		/* test11(), 2nd pipe */
#define PDPNOHANG  1		/* test03(), write_standards() */
#define MAXERR 5

#define USER_ID   12
#define GROUP_ID   1
#define FF        3		/* first free filedes. */
#define USER      1		/* uid */
#define GROUP     0		/* gid */

#define ARSIZE   256		/* array size */
#define PIPESIZE 3584		/* maxnumber of bytes to be written on pipe */
#define MAXOPEN  17		/* maximum number of extra open files */
#define MAXLINK 0177		/* maximum number of links per file */
#define MASK    0777		/* selects lower nine bits */
#define READ_EOF 0		/* returned by read-call at eof */

#define OK      0
#define FAIL   -1

#define R       0		/* read (open-call) */
#define W       1		/* write (open-call) */
#define RW      2		/* read & write (open-call) */

#define RWX     7		/* read & write & execute (mode) */

#define NIL     ""
#define UMASK   "umask"
#define CREAT   "creat"
#define WRITE   "write"
#define READ    "read"
#define OPEN    "open"
#define CLOSE   "close"
#define LSEEK   "lseek"
#define ACCESS  "access"
#define CHDIR   "chdir"
#define CHMOD   "chmod"
#define LINK    "link"
#define UNLINK  "unlink"
#define PIPE    "pipe"
#define STAT    "stat"
#define FSTAT   "fstat"
#define DUP     "dup"
#define UTIME   "utime"

int errct;

char *file[];
char *fnames[];
char *dir[];

/* "decl.c", created by Rene Montsma and Menno Wilcke */

/* Used in open_alot, close_alot */
char *file[20] = {"f0", "f1", "f2", "f3", "f4", "f5", "f6",
	  "f7", "f8", "f9", "f10", "f11", "f12", "f13",
	  "f14", "f15", "f16", "f17", "f18", "f19"}, *fnames[8] = {"---", "--x", "-w-", "-wx", "r--",
								   "r-x", "rw-", "rwx"}, *dir[8] = {"d---", "d--x", "d-w-", "d-wx", "dr--", "dr-x",
						    "drw-", "drwx"};
 /* Needed for easy creating and deleting of directories */

/* "test.c", created by Rene Montsma and Menno Wilcke */

_PROTOTYPE(int main, (void));
_PROTOTYPE(void test, (void));
_PROTOTYPE(void test01, (void));
_PROTOTYPE(void test02, (void));
_PROTOTYPE(void test03, (void));
_PROTOTYPE(void write_standards, (int filedes, char a []));
_PROTOTYPE(void test04, (void));
_PROTOTYPE(void read_standards, (int filedes, char a []));
_PROTOTYPE(void read_more, (int filedes, char a []));
_PROTOTYPE(void test05, (void));
_PROTOTYPE(void try_open, (char *fname, int mode, int test));
_PROTOTYPE(void test06, (void));
_PROTOTYPE(void test07, (void));
_PROTOTYPE(void access_standards, (void));
_PROTOTYPE(void try_access, (char *fname, int mode, int test));
_PROTOTYPE(void e, (char *string));
_PROTOTYPE(void nlcr, (void));
_PROTOTYPE(void str, (char *s));
_PROTOTYPE(void err, (int number, char *scall, char *name));
_PROTOTYPE(void make_and_fill_dirs, (void));
_PROTOTYPE(void put_file_in_dir, (char *dirname, int mode));
_PROTOTYPE(void init_array, (char *a));
_PROTOTYPE(void clear_array, (char *b));
_PROTOTYPE(int comp_array, (char *a, char *b, int range));
_PROTOTYPE(void try_close, (int filedes, char *name));
_PROTOTYPE(void try_unlink, (char *fname));
_PROTOTYPE(void Remove, (int fdes, char *fname));
_PROTOTYPE(int get_mode, (char *name));
_PROTOTYPE(void check, (char *scall, int number));
_PROTOTYPE(void put, (int nr));
_PROTOTYPE(int open_alot, (void));
_PROTOTYPE(int close_alot, (int number));
_PROTOTYPE(void clean_up_the_mess, (void));
_PROTOTYPE(void chmod_8_dirs, (int sw));
_PROTOTYPE(void quit, (void));

/*****************************************************************************
 *                              TEST                                         *
 ****************************************************************************/
int main()
{
  int n;

  if (geteuid() == 0 || getuid() == 0) {
	printf("Test 18 cannot run as root; test aborted\n");
	exit(1);
  }

  system("rm -rf DIR_18; mkdir DIR_18");
  chdir("DIR_18");

  if (fork()) {
	printf("Test 18 ");
	fflush(stdout);		/* have to flush for child's benefit */

	wait(&n);
	clean_up_the_mess();
	quit();
  } else {
	test();
	exit(0);
  }

  return(0);
}

void test()
{
  umask(0);			/* not honest, but i always forget */

  test01();
  make_and_fill_dirs();
  test02();
  test03();
  test04();
  test05();
  test06();
  test07();
  umask(022);
}				/* test */

/* "t1.c" created by Rene Montsma and Menno Wilcke */

/*****************************************************************************
 *                              test UMASK                                   *
 ****************************************************************************/
void test01()
{
  int oldvalue, newvalue, tempvalue;
  int nr;

  if ((oldvalue = umask(0777)) != 0) err(0, UMASK, NIL);

  /* Special test: only the lower 9 bits (protection bits) may part- *
   * icipate. ~0777 means: 111 000 000 000. Giving this to umask must*
   * not change any value.                                           */

  if ((newvalue = umask(~0777)) != 0777) err(1, UMASK, "illegal");
  if (oldvalue == newvalue) err(11, UMASK, "not change mask");

  if ((tempvalue = umask(0)) != 0) err(2, UMASK, "values");

  /* Now test all possible modes of umask on a file */
  for (newvalue = MASK; newvalue >= 0; newvalue -= 0111) {
	tempvalue = umask(newvalue);
	if (tempvalue != oldvalue) {
		err(1, UMASK, "illegal");
		break;		/* no use trying more */
	} else if ((nr = creat("file01", 0777)) < 0)
		err(5, CREAT, "'file01'");
	else {
		try_close(nr, "'file01'");
		if (get_mode("file01") != (MASK & ~newvalue))
			err(7, UMASK, "mode computed");
		try_unlink("file01");
	}
	oldvalue = newvalue;
  }

  /* The loop has terminated with umask(0) */
  if ((tempvalue = umask(0)) != 0)
	err(7, UMASK, "umask may influence rest of tests!");
}				/* test01 */

/*****************************************************************************
 *                              test CREAT                                   *
 ****************************************************************************/
void test02()
{
  int n, n1, mode;
  char a[ARSIZE], b[ARSIZE];
  struct stat stbf1;

  mode = 0;
  /* Create twenty files, check filedes */
  for (n = 0; n < MAXOPEN; n++) {
	if (creat(file[n], mode) != FF + n)
		err(13, CREAT, file[n]);
	else {
		if (get_mode(file[n]) != mode)
			err(7, CREAT, "mode set while creating many files");

		/* Change  mode of file to standard mode, we want to *
		 * use a lot (20) of files to be opened later, see   *
		 * open_alot(), close_alot().                        */
		if (chmod(file[n], 0700) != OK) err(5, CHMOD, file[n]);

	}
	mode = (mode + 0100) % 01000;
  }

  /* Already twenty files opened; opening another has to fail */
  if (creat("file02", 0777) != FAIL)
	err(9, CREAT, "created");
  else
	check(CREAT, EMFILE);

  /* Close all files: seems blunt, but it isn't because we've  *
   * checked all fd's already                                  */
  if ((n = close_alot(MAXOPEN)) < MAXOPEN) err(5, CLOSE, "MAXOPEN files");

  /* Creat 1 file twice; check */
  if ((n = creat("file02", 0777)) < 0)
	err(5, CREAT, "'file02'");
  else {
	init_array(a);
	if (write(n, a, ARSIZE) != ARSIZE) err(1, WRITE, "bad");

	if ((n1 = creat("file02", 0755)) < 0)	/* receate 'file02' */
		err(5, CREAT, "'file02' (2nd time)");
	else {
		/* Fd should be at the top after recreation */
		if (lseek(n1, 0L, SEEK_END) != 0)
			err(11, CREAT, "not truncate file by recreation");
		else {
			/* Try to write on recreated file */
			clear_array(b);

			if (lseek(n1, 0L, SEEK_SET) != 0)
				err(5, LSEEK, "to top of 2nd fd 'file02'");
			if (write(n1, a, ARSIZE) != ARSIZE)
				err(1, WRITE, "(2) bad");

			/* In order to read we've to close and open again */
			try_close(n1, "'file02'  (2nd creation)");
			if ((n1 = open("file02", RW)) < 0)
				err(5, OPEN, "'file02'  (2nd recreation)");

			/* Continue */
			if (lseek(n1, 0L, SEEK_SET) != 0)
				err(5, LSEEK, "to top 'file02'(2nd fd) (2)");
			if (read(n1, b, ARSIZE) != ARSIZE)
				err(1, READ, "wrong");

			if (comp_array(a, b, ARSIZE) != OK) err(11, CREAT,
				    "not really truncate file by recreation");
		}
		if (get_mode("file02") != 0777)
			err(11, CREAT, "not maintain mode by recreation");
		try_close(n1, "recreated 'file02'");

	}
	Remove(n, "file02");
  }

  /* Give 'creat' wrong input: dir not searchable */
  if (creat("drw-/file02", 0777) != FAIL)
	err(4, CREAT, "'drw-'");
  else
	check(CREAT, EACCES);

  /* Dir not writable */
  if (creat("dr-x/file02", 0777) != FAIL)
	err(12, CREAT, "'dr-x/file02'");
  else
	check(CREAT, EACCES);

  /* File not writable */
  if (creat("drwx/r-x", 0777) != FAIL)
	err(11, CREAT, "recreate non-writable file");
  else
	check(CREAT, EACCES);

  /* Try to creat a dir */
  if ((n = creat("dir", 040777)) != FAIL) {
	if (fstat(n, &stbf1) != OK)
		err(5, FSTAT, "'dir'");
	else if (stbf1.st_mode != (mode_t) 0100777)
				/* Cast because mode is negative :-(.
				 * HACK DEBUG FIXME: this appears to duplicate
				 * code in test17.c.
				 */
		err(11, CREAT, "'creat' a new directory");
	Remove(n, "dir");
  }

  /* We don't consider it to be a bug when creat * does not accept
   * tricky modes                */

  /* File is an existing dir */
  if (creat("drwx", 0777) != FAIL)
	err(11, CREAT, "create an existing dir!");
  else
	check(CREAT, EISDIR);
}				/* test02 */

/*****************************************************************************
 *                              test WRITE                                   *
 ****************************************************************************/
void test03()
{
  int n, n1;
  int fd[2];
  char a[ARSIZE];

  init_array(a);

  /* Test write after a CREAT */
  if ((n = creat("file03", 0700)) != FF)	/* 'file03' only open file */
	err(13, CREAT, "'file03'");
  else {
	write_standards(n, a);	/* test simple writes, wrong input too */
	try_close(n, "'file03'");
  }

  /* Test write after an OPEN */
  if ((n = open("file03", W)) < 0)
	err(5, OPEN, "'file03'");
  else
	write_standards(n, a);	/* test simple writes, wrong input too */

  /* Test write after a DUP */
  if ((n1 = dup(n)) < 0)
	err(5, DUP, "'file03'");
  else {
	write_standards(n1, a);
	try_close(n1, "duplicated fd 'file03'");
  }

  /* Remove testfile */
  Remove(n, "file03");

  /* Test write after a PIPE */
  if (pipe(fd) < 0)
	err(5, PIPE, NIL);
  else {
	write_standards(fd[1], a);
	try_close(fd[0], "'fd[0]'");
	try_close(fd[1], "'fd[1]'");
  }

  /* Last test: does write check protections ? */
  if ((n = open("drwx/r--", R)) < 0)
	err(5, OPEN, "'drwx/r--'");
  else {
	if (write(n, a, ARSIZE) != FAIL)
		err(11, WRITE, "write on non-writ. file");
	else
		check(WRITE, EBADF);
	try_close(n, "'drwx/r--'");
  }
}				/* test03 */

void write_standards(filedes, a)
int filedes;
char a[];
{

  /* Write must return written account of numbers */
  if (write(filedes, a, ARSIZE) != ARSIZE) err(1, WRITE, "bad");

  /* Try giving 'write' wrong input */
  /* Wrong filedes */
  if (write(-1, a, ARSIZE) != FAIL)
	err(2, WRITE, "filedes");
  else
	check(WRITE, EBADF);

  /* Wrong length (illegal) */
#ifndef PDPNOHANG
  if (write(filedes, a, -ARSIZE) != FAIL)
	err(2, WRITE, "length");
  else
	check(WRITE, EINVAL);	/* EFAULT on vu45 */
#endif
}				/* write_standards */

/* "t2.c", created by Rene Montsma and Menno Wilcke */

/*****************************************************************************
 *                              test READ                                    *
 ****************************************************************************/
void test04()
{
  int n, n1, fd[2];
  char a[ARSIZE];

  /* Test read after creat */
  if ((n = creat("file04", 0700)) != FF)	/* no other open files may be
					 * left */
	err(13, CREAT, "'file04'");
  else {
	/* Closing and opening needed before writing */
	try_close(n, "'file04'");
	if ((n = open("file04", RW)) < 0) err(5, OPEN, "'file04'");

	init_array(a);

	if (write(n, a, ARSIZE) != ARSIZE)
		err(1, WRITE, "bad");
	else {
		if (lseek(n, 0L, SEEK_SET) != 0) err(5, LSEEK, "'file04'");
		read_standards(n, a);
		read_more(n, a);
	}
	try_close(n, "'file04'");
  }

  /* Test read after OPEN */
  if ((n = open("file04", R)) < 0)
	err(5, OPEN, "'file04'");
  else {
	read_standards(n, a);
	read_more(n, a);
	try_close(n, "'file04'");
  }

  /* Test read after DUP */
  if ((n = open("file04", R)) < 0) err(5, OPEN, "'file04'");
  if ((n1 = dup(n)) < 0)
	err(5, DUP, "'file04'");
  else {
	read_standards(n1, a);
	read_more(n1, a);
	try_close(n1, "duplicated fd 'file04'");
  }

  /* Remove testfile */
  Remove(n, "file04");

  /* Test read after pipe */
  if (pipe(fd) < 0)
	err(5, PIPE, NIL);
  else {
	if (write(fd[1], a, ARSIZE) != ARSIZE) {
		err(5, WRITE, "'fd[1]'");
		try_close(fd[1], "'fd[1]'");
	} else {
		try_close(fd[1], "'fd[1]'");
		read_standards(fd[0], a);
	}
	try_close(fd[0], "'fd[0]'");
  }

  /* Last test: try to read a read-protected file */
  if ((n = open("drwx/-wx", W)) < 0)
	err(5, OPEN, "'drwx/-wx'");
  else {
	if (read(n, a, ARSIZE) != FAIL)
		err(11, READ, "read a non-read. file");
	else
		check(READ, EBADF);
	try_close(n, "'/drwx/-wx'");
  }
}				/* test04 */

void read_standards(filedes, a)
int filedes;
char a[];
{
  char b[ARSIZE];

  clear_array(b);
  if (read(filedes, b, ARSIZE) != ARSIZE)
	err(1, READ, "bad");
  else if (comp_array(a, b, ARSIZE) != OK)
	err(7, "read/write", "values");
  else if (read(filedes, b, ARSIZE) != READ_EOF)
	err(11, READ, "read beyond endoffile");

  /* Try giving read wrong input: wrong filedes */
  if (read(FAIL, b, ARSIZE) != FAIL)
	err(2, READ, "filedes");
  else
	check(READ, EBADF);

  /* Wrong length */
  if (read(filedes, b, -ARSIZE) != FAIL)
	err(2, READ, "length");
  else
	check(READ, EINVAL);
}				/* read_standards */

void read_more(filedes, a)
int filedes;
char a[];
 /* Separated from read_standards() because the PIPE test * would fail.                                           */
{
  int i;
  char b[ARSIZE];

  if (lseek(filedes, (long) (ARSIZE / 2), SEEK_SET) != ARSIZE / 2)
	err(5, LSEEK, "to location ARSIZE/2");

  clear_array(b);
  if (read(filedes, b, ARSIZE) != ARSIZE / 2) err(1, READ, "bad");

  for (i = 0; i < ARSIZE / 2; i++)
	if (b[i] != a[(ARSIZE / 2) + i])
		err(7, READ, "from location ARSIZE/2");
}

/*****************************************************************************
 *                              test OPEN/CLOSE                              *
 ****************************************************************************/
void test05()
{
  int n, n1, mode, fd[2];
  char b[ARSIZE];

  /* Test open after CREAT */
  if ((n = creat("file05", 0700)) != FF)	/* no other open files left */
	err(13, CREAT, "'file05'");
  else {
	if ((n1 = open("file05", RW)) != FF + 1)
		err(13, OPEN, "'file05' after creation");
	try_close(n1, "'file05' (open after creation)");

	try_close(n, "'file05'");
	if ((n = open("file05", R)) != FF)
		err(13, OPEN, "after closing");
	else
		try_close(n, "'file05' (open after closing)");

	/* Remove testfile */
	try_unlink("file05");
  }

  /* Test all possible modes, try_open not only opens file (sometimes) *
   * but closes files too (when opened)                                */
  if ((n = creat("file05", 0700)) < 0)	/* no other files left */
	err(5, CREAT, "'file05' (2nd time)");
  else {
	try_close(n, "file05");
	for (mode = 0; mode <= 0700; mode += 0100) {
		if (chmod("file05", mode) != OK) err(5, CHMOD, "'file05'");

		if (mode <= 0100) {
			try_open("file05", R, FAIL);
			try_open("file05", W, FAIL);
			try_open("file05", RW, FAIL);
		} else if (mode >= 0200 && mode <= 0300) {
			try_open("file05", R, FAIL);
			try_open("file05", W, FF);
			try_open("file05", RW, FAIL);
		} else if (mode >= 0400 && mode <= 0500) {
			try_open("file05", R, FF);
			try_open("file05", W, FAIL);
			try_open("file05", RW, FAIL);
		} else {
			try_open("file05", R, FF);
			try_open("file05", W, FF);
			try_open("file05", RW, FF);
		}
	}
  }

  /* Test opening existing file */
  if ((n = open("drwx/rwx", R)) < 0)
	err(13, OPEN, "existing file");
  else {			/* test close after DUP */
	if ((n1 = dup(n)) < 0)
		err(13, DUP, "'drwx/rwx'");
	else {
		try_close(n1, "duplicated fd 'drwx/rwx'");

		if (read(n1, b, ARSIZE) != FAIL)
			err(11, READ, "on closed dupped fd 'drwx/rwx'");
		else
			check(READ, EBADF);

		if (read(n, b, ARSIZE) == FAIL)	/* should read an eof */
			err(13, READ, "on fd '/drwx/rwx'");
	}
	try_close(n, "'drwx/rwx'");
  }

  /* Test close after PIPE */
  if (pipe(fd) < 0)
	err(13, PIPE, NIL);
  else {
	try_close(fd[1], "duplicated fd 'fd[1]'");

	/* Fd[1] really should be closed now; check */
	clear_array(b);
	if (read(fd[0], b, ARSIZE) != READ_EOF)
		err(11, READ, "read on empty pipe (and fd[1] was closed)");
	try_close(fd[0], "duplicated fd 'fd[0]'");
  }

  /* Try to open a non-existing file */
  if (open("non-file", R) != FAIL)
	err(11, OPEN, "open non-executable file");
  else
	check(OPEN, ENOENT);

  /* Dir does not exist */
  if (open("dzzz/file05", R) != FAIL)
	err(11, OPEN, "open in an non-searchable dir");
  else
	check(OPEN, ENOENT);

  /* Dir is not searchable */
  if (n = open("drw-/rwx", R) != FAIL)
	err(11, OPEN, "open in an non-searchabledir");
  else
	check(OPEN, EACCES);

  /* Unlink testfile */
  try_unlink("file05");

  /* File is not readable */
  if (open("drwx/-wx", R) != FAIL)
	err(11, OPEN, "open unreadable file for reading");
  else
	check(OPEN, EACCES);

  /* File is not writable */
  if (open("drwx/r-x", W) != FAIL)
	err(11, OPEN, "open unwritable file for writing");
  else
	check(OPEN, EACCES);

  /* Try opening more than MAXOPEN  ('extra' (19-8-85)) files */
  if ((n = open_alot()) != MAXOPEN)
	err(13, OPEN, "MAXOPEN files");
  else
	/* Maximum # of files opened now, another open should fail
	 * because * all filedescriptors have already been used.                      */
  if (open("drwx/rwx", RW) != FAIL)
	err(9, OPEN, "open");
  else
	check(OPEN, EMFILE);
  if (close_alot(n) != n) err(5, CLOSE, "all opened files");

  /* Can close make mistakes ? */
  if (close(-1) != FAIL)
	err(2, CLOSE, "filedes");
  else
	check(CLOSE, EBADF);
}				/* test05 */

void try_open(fname, mode, test)
int mode, test;
char *fname;
{
  int n;

  if ((n = open(fname, mode)) != test)
	err(11, OPEN, "break through filepermission with an incorrect mode");
  if (n != FAIL) try_close(n, fname);	/* cleanup */
}				/* try_open */

/*****************************************************************************
 *                              test LSEEK                                   *
 ****************************************************************************/
void test06()
{
  char a[ARSIZE], b[ARSIZE];
  int fd;

  if ((fd = open("drwx/rwx", RW)) != FF)	/* there should be no */
	err(13, OPEN, "'drwx/rwx'");	/* other open files   */
  else {
	init_array(a);
	if (write(fd, a, 10) != 10)
		err(1, WRITE, "bad");
	else {
		/* Lseek back to begin file */
		if (lseek(fd, 0L, SEEK_SET) != 0)
			err(5, LSEEK, "to begin file");
		else if (read(fd, b, 10) != 10)
			err(1, READ, "bad");
		else if (comp_array(a, b, 10) != OK)
			err(7, LSEEK, "values r/w after lseek to begin");
		/* Lseek to endoffile */
		if (lseek(fd, 0L, SEEK_END) != 10)
			err(5, LSEEK, "to end of file");
		else if (read(fd, b, 1) != READ_EOF)
			err(7, LSEEK, "read at end of file");
		/* Lseek beyond file */
		if (lseek(fd, 10L, SEEK_CUR) != 20)
			err(5, LSEEK, "beyond end of file");
		else if (write(fd, a, 10) != 10)
			err(1, WRITE, "bad");
		else {
			/* Lseek to begin second write */
			if (lseek(fd, 20L, SEEK_SET) != 20)
				err(5, LSEEK, "'/drwx/rwx'");
			if (read(fd, b, 10) != 10)
				err(1, READ, "bad");
			else if (comp_array(a, b, 10) != OK)
				err(7, LSEEK,
				 "values read after lseek MAXOPEN");
		}
	}

	/* Lseek to position before begin of file */
	if (lseek(fd, -1L, 0) != FAIL)
		err(11, LSEEK, "lseek before beginning of file");

	try_close(fd, "'drwx/rwx'");
  }

  /* Lseek on invalid filediscriptor */
  if (lseek(-1, 0L, SEEK_SET) != FAIL)
	err(2, LSEEK, "filedes");
  else
	check(LSEEK, EBADF);

}

/* "t3.c", created by Rene Montsma and Menno Wilcke */

/*****************************************************************************
 *                              test ACCESS                                  *
 ****************************************************************************/
void test07()
{
  /* Check with proper parameters */
  if (access("drwx/rwx", RWX) != OK) err(5, ACCESS, "accessible file");

  if (access("./././drwx/././rwx", 0) != OK)
	err(5, ACCESS, "'/./.(etc)/drwx///rwx'");

  /* Check 8 files with 8 different modes on 8 accesses  */
  if (chdir("drwx") != OK) err(5, CHDIR, "'drwx'");

  access_standards();

  if (chdir("..") != OK) err(5, CHDIR, "'..'");

  /* Check several wrong combinations */
  /* File does not exist */
  if (access("non-file", 0) != FAIL)
	err(11, ACCESS, "access non-existing file");
  else
	check(ACCESS, ENOENT);

  /* Non-searchable dir */
  if (access("drw-/rwx", 0) != FAIL)
	err(4, ACCESS, "'drw-'");
  else
	check(ACCESS, EACCES);

  /* Searchable dir, but wrong file-mode */
  if (access("drwx/--x", RWX) != FAIL)
	err(11, ACCESS, "a non accessible file");
  else
	check(ACCESS, EACCES);

}				/* test07 */

void access_standards()
{
  int i, mode = 0;

  for (i = 0; i < 8; i++)
	if (i == 0)
		try_access(fnames[mode], i, OK);
	else
		try_access(fnames[mode], i, FAIL);
  mode++;

  for (i = 0; i < 8; i++)
	if (i < 2)
		try_access(fnames[mode], i, OK);
	else
		try_access(fnames[mode], i, FAIL);
  mode++;

  for (i = 0; i < 8; i++)
	if (i == 0 || i == 2)
		try_access(fnames[mode], i, OK);
	else
		try_access(fnames[mode], i, FAIL);
  mode++;

  for (i = 0; i < 8; i++)
	if (i < 4)
		try_access(fnames[mode], i, OK);
	else
		try_access(fnames[mode], i, FAIL);
  mode++;

  for (i = 0; i < 8; i++)
	if (i == 0 || i == 4)
		try_access(fnames[mode], i, OK);
	else
		try_access(fnames[mode], i, FAIL);
  mode++;

  for (i = 0; i < 8; i++)
	if (i == 0 || i == 1 || i == 4 || i == 5)
		try_access(fnames[mode], i, OK);
	else
		try_access(fnames[mode], i, FAIL);
  mode++;

  for (i = 0; i < 8; i++)
	if (i % 2 == 0)
		try_access(fnames[mode], i, OK);
	else
		try_access(fnames[mode], i, FAIL);
  mode++;

  for (i = 0; i < 8; i++) try_access(fnames[mode], i, OK);
}				/* access_standards */

void try_access(fname, mode, test)
int mode, test;
char *fname;
{
  if (access(fname, mode) != test)
	err(100, ACCESS, "incorrect access on a file (try_access)");
}				/* try_access */

/* "support.c", created by Rene Montsma and Menno Wilcke */

/* Err, make_and_fill_dirs, init_array, clear_array, comp_array,
   try_close, try_unlink, Remove, get_mode, check, open_alot,
   close_alot, clean_up_the_mess.
*/

/***********************************************************************
 *				EXTENDED FIONS			       *
 **********************************************************************/
/* First extended functions (i.e. not oldfashioned monixcalls.
   e(), nlcr(), octal.*/

void e(string)
char *string;
{
  printf("Test program error: %s\n", string);
  errct++;
}

void nlcr()
{
  printf("\n");
}

void str(s)
char *s;
{
  printf(s);
}

/*****************************************************************************
*                                                                            *
*                               ERR(or) messages                             *
*                                                                            *
*****************************************************************************/
void err(number, scall, name)
 /* Give nice error messages */

char *scall, *name;
int number;

{
  errct++;
  if (errct > MAXERR) {
	printf("Too many errors; test aborted\n");
	chdir("..");
	system("rm -rf DIR*");
	quit();
  }
  e("");
  str("\t");
  switch (number) {
      case 0:
	str(scall);
	str(": illegal initial value.");
	break;
      case 1:
	str(scall);
	str(": ");
	str(name);
	str(" value returned.");
	break;
      case 2:
	str(scall);
	str(": accepting illegal ");
	str(name);
	str(".");
	break;
      case 3:
	str(scall);
	str(": accepting non-existing file.");
	break;
      case 4:
	str(scall);
	str(": could search non-searchable dir (");
	str(name);
	str(").");
	break;
      case 5:
	str(scall);
	str(": cannot ");
	str(scall);
	str(" ");
	str(name);
	str(".");
	break;
      case 7:
	str(scall);
	str(": incorrect ");
	str(name);
	str(".");
	break;
      case 8:
	str(scall);
	str(": wrong values.");
	break;
      case 9:
	str(scall);
	str(": accepting too many ");
	str(name);
	str(" files.");
	break;
      case 10:
	str(scall);
	str(": even a superuser can't do anything!");
	break;
      case 11:
	str(scall);
	str(": could ");
	str(name);
	str(".");
	break;
      case 12:
	str(scall);
	str(": could write in non-writable dir (");
	str(name);
	str(").");
	break;
      case 13:
	str(scall);
	str(": wrong filedes returned (");
	str(name);
	str(").");
	break;
      case 100:
	str(scall);		/* very common */
	str(": ");
	str(name);
	str(".");
	break;
      default:	str("errornumber does not exist!\n");
  }
  nlcr();
}				/* err */

/*****************************************************************************
*                                                                            *
*                          MAKE_AND_FILL_DIRS                                *
*                                                                            *
*****************************************************************************/

void make_and_fill_dirs()
 /* Create 8 dir.'s: "d---", "d--x", "d-w-", "d-wx", "dr--", "dr-x",     *
  * "drw-", "drwx".                                     * Then create 8 files
  * in "drwx", and some needed files in other dirs.  */
{
  int mode, i;

  for (i = 0; i < 8; i++) {
	mkdir(dir[i], 0700);
	chown(dir[i], USER_ID, GROUP_ID);
  }
  setuid(USER_ID);
  setgid(GROUP_ID);

  for (mode = 0; mode < 8; mode++) put_file_in_dir("drwx", mode);

  put_file_in_dir("d-wx", RWX);
  put_file_in_dir("dr-x", RWX);
  put_file_in_dir("drw-", RWX);

  chmod_8_dirs(8);		/* 8 means; 8 different modes */

}				/* make_and_fill_dirs */

void put_file_in_dir(dirname, mode)
char *dirname;
int mode;
 /* Fill directory 'dirname' with file with mode 'mode'.   */
{
  int nr;

  if (chdir(dirname) != OK)
	err(5, CHDIR, "to dirname (put_f_in_dir)");
  else {
	/* Creat the file */
	if ((nr = creat(fnames[mode], mode * 0100)) < 0)
		err(13, CREAT, fnames[mode]);
	else
		try_close(nr, fnames[mode]);

	if (chdir("..") != OK)
		err(5, CHDIR, "to previous dir (put_f_in_dir)");
  }
}				/* put_file_in_dir */

/*****************************************************************************
*                                                                            *
*                               MISCELLANEOUS                                *
*                                                                            *
*(all about arrays, 'try_close', 'try_unlink', 'Remove', 'get_mode')         *
*                                                                            *
*****************************************************************************/

void init_array(a)
char *a;
{
  int i;

  i = 0;
  while (i++ < ARSIZE) *a++ = 'a' + (i % 26);
}				/* init_array */

void clear_array(b)
char *b;
{
  int i;

  i = 0;
  while (i++ < ARSIZE) *b++ = '0';

}				/* clear_array */

int comp_array(a, b, range)
char *a, *b;
int range;
{
  if ((range < 0) || (range > ARSIZE)) {
	err(100, "comp_array", "illegal range");
	return(FAIL);
  } else {
	while (range-- && (*a++ == *b++));
	if (*--a == *--b)
		return(OK);
	else
		return(FAIL);
  }
}				/* comp_array */

void try_close(filedes, name)
int filedes;
char *name;
{
  if (close(filedes) != OK) err(5, CLOSE, name);
}				/* try_close */

void try_unlink(fname)
char *fname;
{
  if (unlink(fname) != 0) err(5, UNLINK, fname);
}				/* try_unlink */

void Remove(fdes, fname)
int fdes;
char *fname;
{
  try_close(fdes, fname);
  try_unlink(fname);
}				/* Remove */

int get_mode(name)
char *name;
{
  struct stat stbf1;

  if (stat(name, &stbf1) != OK) {
	err(5, STAT, name);
	return(stbf1.st_mode);	/* return a mode which will cause *
				 * error in the calling function  *
				 * (file/dir bit)                 */
  } else
	return(stbf1.st_mode & 07777);	/* take last 4 bits */
}				/* get_mode */

/*****************************************************************************
*                                                                            *
*                                  CHECK                                     *
*                                                                            *
*****************************************************************************/

void check(scall, number)
int number;
char *scall;
{
  if (errno != number) {
	e(NIL);
	str("\t");
	str(scall);
	str(": bad errno-value: ");
	put(errno);
	str(" should have been: ");
	put(number);
	nlcr();
  }
}				/* check */

void put(nr)
int nr;
{
  switch (nr) {
      case 0:	str("unused");	  	break;
      case 1:	str("EPERM");	  	break;
      case 2:	str("ENOENT");	  	break;
      case 3:	str("ESRCH");	  	break;
      case 4:	str("EINTR");	  	break;
      case 5:	str("EIO");	  	break;
      case 6:	str("ENXIO");	  	break;
      case 7:	str("E2BIG");	  	break;
      case 8:	str("ENOEXEC");	  	break;
      case 9:	str("EBADF");	  	break;
      case 10:	str("ECHILD");	  	break;
      case 11:	str("EAGAIN");	  	break;
      case 12:	str("ENOMEM");	  	break;
      case 13:	str("EACCES");	  	break;
      case 14:	str("EFAULT");	  	break;
      case 15:	str("ENOTBLK");	  	break;
      case 16:	str("EBUSY");	  	break;
      case 17:	str("EEXIST");	  	break;
      case 18:	str("EXDEV");	  	break;
      case 19:	str("ENODEV");	  	break;
      case 20:	str("ENOTDIR");	  	break;
      case 21:	str("EISDIR");	  	break;
      case 22:	str("EINVAL");	  	break;
      case 23:	str("ENFILE");	  	break;
      case 24:	str("EMFILE");	  	break;
      case 25:	str("ENOTTY");	  	break;
      case 26:	str("ETXTBSY");	  	break;
      case 27:	str("EFBIG");	  	break;
      case 28:	str("ENOSPC");	  	break;
      case 29:	str("ESPIPE");	  	break;
      case 30:	str("EROFS");	  	break;
      case 31:	str("EMLINK");	  	break;
      case 32:	str("EPIPE");	  	break;
      case 33:	str("EDOM");	  	break;
      case 34:	str("ERANGE");	  	break;
  }
}

/*****************************************************************************
*                                                                            *
*                                ALOT-functions                              *
*                                                                            *
*****************************************************************************/

int open_alot()
{
  int i;

  for (i = 0; i < MAXOPEN; i++)
	if (open(file[i], R) == FAIL) break;
  if (i == 0) err(5, "open_alot", "at all");
  return(i);
}				/* open_alot */

int close_alot(number)
int number;
{
  int i, count = 0;

  if (number > MAXOPEN)
	err(5, "close_alot", "accept this argument");
  else
	for (i = FF; i < number + FF; i++)
		if (close(i) != OK) count++;

  return(number - count);	/* return number of closed files */
}				/* close_alot */

/*****************************************************************************
*                                                                            *
*                         CLEAN UP THE MESS                                  *
*                                                                            *
*****************************************************************************/

void clean_up_the_mess()
{
  int i;
  char dirname[6];

  /* First remove 'alot' files */
  for (i = 0; i < MAXOPEN; i++) try_unlink(file[i]);

  /* Unlink the files in dir 'drwx' */
  if (chdir("drwx") != OK)
	err(5, CHDIR, "to 'drwx'");
  else {
	for (i = 0; i < 8; i++) try_unlink(fnames[i]);
	if (chdir("..") != OK) err(5, CHDIR, "to '..'");
  }

  /* Before unlinking files in some dirs, make them writable */
  chmod_8_dirs(RWX);

  /* Unlink files in other dirs */
  try_unlink("d-wx/rwx");
  try_unlink("dr-x/rwx");
  try_unlink("drw-/rwx");

  /* Unlink dirs */
  for (i = 0; i < 8; i++) {
	strcpy(dirname, "d");
	strcat(dirname, fnames[i]);
	/* 'dirname' contains the directoryname */
	rmdir(dirname);
  }

  /* FINISH */
}				/* clean_up_the_mess */

void chmod_8_dirs(sw)
int sw;				/* if switch == 8, give all different
			 * mode,else the same mode */
{
  int mode;
  int i;

  if (sw == 8)
	mode = 0;
  else
	mode = sw;

  for (i = 0; i < 8; i++) {
	chmod(dir[i], 040000 + mode * 0100);
	if (sw == 8) mode++;
  }
}

void quit()
{

  chdir("..");
  system("rm -rf DIR*");

  if (errct == 0) {
	printf("ok\n");
	exit(0);
  } else {
	printf("%d errors\n", errct);
	exit(1);
  }
}
