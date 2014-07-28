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
#include <limits.h>
#include <assert.h>
#include <sys/uio.h>

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
#define MAXOPEN  (OPEN_MAX-3)	/* maximum number of extra open files */
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
#define WRITEV  "writev"
#define READ    "read"
#define READV   "readv"
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

int max_error = 2;
#include "common.h"


/* "decl.c", created by Rene Montsma and Menno Wilcke */

/* Used in open_alot, close_alot */
char *file[MAXOPEN];
char *fnames[8] = {"---", "--x", "-w-", "-wx", "r--", "r-x", "rw-", "rwx"},
	*dir[8] = {"d---", "d--x", "d-w-", "d-wx", "dr--", "dr-x", "drw-", "drwx"};

 /* Needed for easy creating and deleting of directories */

/* "test.c", created by Rene Montsma and Menno Wilcke */


int main(int argc, char **argv);
void test(void);
void test01(void);
void test02(void);
void test03(void);
void write_standards(int filedes, char a []);
void test04(void);
void read_standards(int filedes, char a []);
void read_more(int filedes, char a []);
void test05(void);
void try_open(char *fname, int mode, int test);
void test06(void);
void test07(void);
void access_standards(void);
void test08(void);
static int iovec_is_equal(struct iovec *x, struct iovec *y, size_t
	size);
static size_t iovec_setup(int pattern, struct iovec *iovec, char
	*buffer, int count);
static int power(int base, int exponent);
void try_access(char *fname, int mode, int test);
void make_and_fill_dirs(void);
void put_file_in_dir(char *dirname, int mode);
void init_array(char *a);
void clear_array(char *b);
int comp_array(char *a, char *b, int range);
void try_close(int filedes, char *name);
void try_unlink(char *fname);
void Remove(int fdes, char *fname);
int get_mode(char *name);
void check(char *scall, int number);
void put(int nr);
int open_alot(void);
int close_alot(int number);
void clean_up_the_mess(void);
void chmod_8_dirs(int sw);

/*****************************************************************************
 *                              TEST                                         *
 ****************************************************************************/
int main(int argc, char **argv)
{
  int n, i;
  pid_t child;

  start(18);

  /* Create filenames for MAXOPEN files, the *file[] array. */
  for(i = 0; i < MAXOPEN; i++) {
        if(asprintf(&file[i], "file%d", i) == -1) {
                fprintf(stderr, "asprintf failed\n");
                quit();
        }
  }

  subtest = 0;
  child = fork();
  if (child == -1) {
	e(1);
	quit();
  } else if (child == 0) {
	test();
	return(0);
  } else {
	wait(&n);
	clean_up_the_mess();
	quit();
  }

  return(-1);
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
  test08();
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

  subtest = 1;

  if ((oldvalue = umask(0777)) != 0) e(1);

  /* Special test: only the lower 9 bits (protection bits) may part- *
   * icipate. ~0777 means: 111 000 000 000. Giving this to umask must*
   * not change any value.                                           */

  if ((newvalue = umask(~0777)) != 0777) e(2);
  if (oldvalue == newvalue) e(3);

  if ((tempvalue = umask(0)) != 0) e(4);

  /* Now test all possible modes of umask on a file */
  for (newvalue = MASK; newvalue >= 0; newvalue -= 0111) {
	tempvalue = umask(newvalue);
	if (tempvalue != oldvalue) {
		e(5);
		break;		/* no use trying more */
	} else if ((nr = creat("file01", 0777)) < 0)
		e(6);
	else {
		try_close(nr, "'file01'");
		if (get_mode("file01") != (MASK & ~newvalue)) e(7);
		try_unlink("file01");
	}
	oldvalue = newvalue;
  }

  /* The loop has terminated with umask(0) */
  if ((tempvalue = umask(0)) != 0) e(8);
}				/* test01 */

/*****************************************************************************
 *                              test CREAT                                   *
 ****************************************************************************/
void test02()
{
  int n, n1, mode;
  char a[ARSIZE], b[ARSIZE];
  struct stat stbf1;

  subtest = 2;
  mode = 0;
  /* Create MAXOPEN files, check filedes */
  for (n = 0; n < MAXOPEN; n++) {
	if (creat(file[n], mode) != FF + n)
		e(1);
	else {
		if (get_mode(file[n]) != mode) e(2);

		/* Change  mode of file to standard mode, we want to *
		 * use a lot (20) of files to be opened later, see   *
		 * open_alot(), close_alot().                        */
		if (chmod(file[n], 0700) != OK) e(3);

	}
	mode = (mode + 0100) % 01000;
  }

  /* Already twenty files opened; opening another has to fail */
  if (creat("file02", 0777) != FAIL) e(4);
  else
	if (errno != EMFILE) e(5);;


  /* Close all files: seems blunt, but it isn't because we've  *
   * checked all fd's already                                  */
  if ((n = close_alot(MAXOPEN)) < MAXOPEN) e(6);

  /* Creat 1 file twice; check */
  if ((n = creat("file02", 0777)) < 0) e(7);
  else {
	init_array(a);
	if (write(n, a, ARSIZE) != ARSIZE) e(8);

	if ((n1 = creat("file02", 0755)) < 0) e(9);
	else {
		/* Fd should be at the top after recreation */
		if (lseek(n1, 0L, SEEK_END) != 0) e(10);
		else {
			/* Try to write on recreated file */
			clear_array(b);

			if (lseek(n1, 0L, SEEK_SET) != 0) e(11);
			if (write(n1, a, ARSIZE) != ARSIZE) e(12);

			/* In order to read we've to close and open again */
			try_close(n1, "'file02'  (2nd creation)");
			if ((n1 = open("file02", RW)) < 0) e(13);

			/* Continue */
			if (lseek(n1, 0L, SEEK_SET) != 0) e(14);
			if (read(n1, b, ARSIZE) != ARSIZE) e(15);

			if (comp_array(a, b, ARSIZE) != OK) e(16);
		}
		if (get_mode("file02") != 0777) e(17);
		try_close(n1, "recreated 'file02'");

	}
	Remove(n, "file02");
  }

  /* Give 'creat' wrong input: dir not searchable */
  if (creat("drw-/file02", 0777) != FAIL) e(18);
  else
	if (errno != EACCES) e(19);

  /* Dir not writable */
  if (creat("dr-x/file02", 0777) != FAIL) e(20);
  else
	if (errno != EACCES) e(21);

  /* File not writable */
  if (creat("drwx/r-x", 0777) != FAIL) e(22);
  else
  	if (errno != EACCES) e(23);

  /* Try to creat a dir */
  if ((n = creat("dir", 040777)) != FAIL) {
	if (fstat(n, &stbf1) != OK) e(24);
	else if (stbf1.st_mode != (mode_t) 0100777)
				/* Cast because mode is negative :-(.
				 * HACK DEBUG FIXME: this appears to duplicate
				 * code in test17.c.
				 */
		e(25);
	Remove(n, "dir");
  }

  /* We don't consider it to be a bug when creat * does not accept
   * tricky modes                */

  /* File is an existing dir */
  if (creat("drwx", 0777) != FAIL) e(26);
  else
	if (errno != EISDIR) e(27);
}				/* test02 */

/*****************************************************************************
 *                              test WRITE                                   *
 ****************************************************************************/
void test03()
{
  int n, n1;
  int fd[2];
  char a[ARSIZE];

  subtest = 3;
  init_array(a);

  /* Test write after a CREAT */
  if ((n = creat("file03", 0700)) != FF) e(1);
  else {
	write_standards(n, a);	/* test simple writes, wrong input too */
	try_close(n, "'file03'");
  }

  /* Test write after an OPEN */
  if ((n = open("file03", W)) < 0) e(2);
  else
	write_standards(n, a);	/* test simple writes, wrong input too */

  /* Test write after a DUP */
  if ((n1 = dup(n)) < 0) e(3);
  else {
	write_standards(n1, a);
	try_close(n1, "duplicated fd 'file03'");
  }

  /* Remove testfile */
  Remove(n, "file03");

  /* Test write after a PIPE */
  if (pipe(fd) < 0) e(4);
  else {
	write_standards(fd[1], a);
	try_close(fd[0], "'fd[0]'");
	try_close(fd[1], "'fd[1]'");
  }

  /* Last test: does write check protections ? */
  if ((n = open("drwx/r--", R)) < 0) e(5);
  else {
	if (write(n, a, ARSIZE) != FAIL) e(6);
	else
		if (errno != EBADF) e(7);
	try_close(n, "'drwx/r--'");
  }
}				/* test03 */

void write_standards(filedes, a)
int filedes;
char a[];
{

  /* Write must return written account of numbers */
  if (write(filedes, a, ARSIZE) != ARSIZE) e(80);

  /* Try giving 'write' wrong input */
  /* Wrong filedes */
  if (write(-1, a, ARSIZE) != FAIL) e(81);
  else
	if (errno != EBADF) e(82);

  /* Wrong length (illegal) */
#ifndef PDPNOHANG
  if (write(filedes, a, -ARSIZE) != FAIL) e(83);
  else
	if (errno != EINVAL) e(84);
#endif
}				/* write_standards */


/*****************************************************************************
 *                              test READ                                    *
 ****************************************************************************/
void test04()
{
  int n, n1, fd[2];
  char a[ARSIZE];

  subtest = 4;

  /* Test read after creat */
  if ((n = creat("file04", 0700)) != FF) e(1);
  else {
	/* Closing and opening needed before writing */
	try_close(n, "'file04'");
	if ((n = open("file04", RW)) < 0) e(2);

	init_array(a);

	if (write(n, a, ARSIZE) != ARSIZE) e(3);
	else {
		if (lseek(n, 0L, SEEK_SET) != 0) e(4);
		read_standards(n, a);
		read_more(n, a);
	}
	try_close(n, "'file04'");
  }

  /* Test read after OPEN */
  if ((n = open("file04", R)) < 0) e(5);
  else {
	read_standards(n, a);
	read_more(n, a);
	try_close(n, "'file04'");
  }

  /* Test read after DUP */
  if ((n = open("file04", R)) < 0) e(6);
  if ((n1 = dup(n)) < 0) e(7);
  else {
	read_standards(n1, a);
	read_more(n1, a);
	try_close(n1, "duplicated fd 'file04'");
  }

  /* Remove testfile */
  Remove(n, "file04");

  /* Test read after pipe */
  if (pipe(fd) < 0) e(8);
  else {
	if (write(fd[1], a, ARSIZE) != ARSIZE) {
		e(9);
		try_close(fd[1], "'fd[1]'");
	} else {
		try_close(fd[1], "'fd[1]'");
		read_standards(fd[0], a);
	}
	try_close(fd[0], "'fd[0]'");
  }

  /* Last test: try to read a read-protected file */
  if ((n = open("drwx/-wx", W)) < 0) e(10);
  else {
	if (read(n, a, ARSIZE) != FAIL) e(11);
	else
		if (errno != EBADF) e(12);
	try_close(n, "'/drwx/-wx'");
  }
}				/* test04 */

void read_standards(filedes, a)
int filedes;
char a[];
{
  char b[ARSIZE];

  clear_array(b);
  if (read(filedes, b, ARSIZE) != ARSIZE) e(85);
  else if (comp_array(a, b, ARSIZE) != OK) e(86);
  else if (read(filedes, b, ARSIZE) != READ_EOF) e(87);

  /* Try giving read wrong input: wrong filedes */
  if (read(FAIL, b, ARSIZE) != FAIL) e(88);
  else
	if (errno != EBADF) e(89);

  /* Wrong length */
  if (read(filedes, b, -ARSIZE) != FAIL) e(90);
  else
	if (errno != EINVAL) e(91);
}				/* read_standards */

void read_more(filedes, a)
int filedes;
char a[];
 /* Separated from read_standards() because the PIPE test * would fail.                                           */
{
  int i;
  char b[ARSIZE];

  if (lseek(filedes, (long) (ARSIZE / 2), SEEK_SET) != ARSIZE / 2) e(92);

  clear_array(b);
  if (read(filedes, b, ARSIZE) != ARSIZE / 2) e(93);

  for (i = 0; i < ARSIZE / 2; i++)
	if (b[i] != a[(ARSIZE / 2) + i]) e(94);
}

/*****************************************************************************
 *                              test OPEN/CLOSE                              *
 ****************************************************************************/
void test05()
{
  int n, n1, mode, fd[2];
  char b[ARSIZE];

  subtest = 5;
  /* Test open after CREAT */
  if ((n = creat("file05", 0700)) != FF) e(1);
  else {
	if ((n1 = open("file05", RW)) != FF + 1) e(2);
	try_close(n1, "'file05' (open after creation)");

	try_close(n, "'file05'");
	if ((n = open("file05", R)) != FF) e(3);
	else
		try_close(n, "'file05' (open after closing)");

	/* Remove testfile */
	try_unlink("file05");
  }

  /* Test all possible modes, try_open not only opens file (sometimes) *
   * but closes files too (when opened)                                */
  if ((n = creat("file05", 0700)) < 0) e(6);
  else {
	try_close(n, "file05");
	for (mode = 0; mode <= 0700; mode += 0100) {
		if (chmod("file05", mode) != OK) e(7);

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
  if ((n = open("drwx/rwx", R)) < 0) e(8);
  else {			/* test close after DUP */
	if ((n1 = dup(n)) < 0) e(9);
	else {
		try_close(n1, "duplicated fd 'drwx/rwx'");

		if (read(n1, b, ARSIZE) != FAIL) e(10);
		else
			if (errno != EBADF) e(11);

		if (read(n, b, ARSIZE) == FAIL)	e(12);/* should read an eof */
	}
	try_close(n, "'drwx/rwx'");
  }

  /* Test close after PIPE */
  if (pipe(fd) < 0) e(13);
  else {
	try_close(fd[1], "duplicated fd 'fd[1]'");

	/* Fd[1] really should be closed now; check */
	clear_array(b);
	if (read(fd[0], b, ARSIZE) != READ_EOF) e(14);
	try_close(fd[0], "duplicated fd 'fd[0]'");
  }

  /* Try to open a non-existing file */
  if (open("non-file", R) != FAIL) e(15);
  else
	if (errno != ENOENT) e(16);

  /* Dir does not exist */
  if (open("dzzz/file05", R) != FAIL) e(17);
  else
	if (errno !=  ENOENT) e(18);

  /* Dir is not searchable */
  if ((n = open("drw-/rwx", R)) != FAIL) e(19);
  else
	if (errno != EACCES) e(20);

  /* Unlink testfile */
  try_unlink("file05");

  /* File is not readable */
  if (open("drwx/-wx", R) != FAIL) e(21);
  else
	if (errno != EACCES) e(22);

  /* File is not writable */
  if (open("drwx/r-x", W) != FAIL) e(23);
  else
	if (errno != EACCES) e(24);

  /* Try opening more than MAXOPEN  ('extra' (19-8-85)) files */
  if ((n = open_alot()) != MAXOPEN) e(25);
  else
	/* Maximum # of files opened now, another open should fail
	 * because * all filedescriptors have already been used.                      */
  if (open("drwx/rwx", RW) != FAIL) e(26);
  else
	if (errno != EMFILE) e(27);
  if (close_alot(n) != n) e(28);

  /* Can close make mistakes ? */
  if (close(-1) != FAIL) e(29);
  else
	if (errno != EBADF) e(30);
}				/* test05 */

void try_open(fname, mode, test)
int mode, test;
char *fname;
{
  int n;

  if ((n = open(fname, mode)) != test) e(95);
  if (n != FAIL) try_close(n, fname);	/* cleanup */
}				/* try_open */

/*****************************************************************************
 *                              test LSEEK                                   *
 ****************************************************************************/
void test06()
{
  char a[ARSIZE], b[ARSIZE];
  int fd;

  subtest = 6;

  if ((fd = open("drwx/rwx", RW)) != FF) e(1);
  else {
	init_array(a);
	if (write(fd, a, 10) != 10) e(2);
	else {
		/* Lseek back to begin file */
		if (lseek(fd, 0L, SEEK_SET) != 0) e(3);
		else if (read(fd, b, 10) != 10) e(4);
		else if (comp_array(a, b, 10) != OK) e(5);

		/* Lseek to endoffile */
		if (lseek(fd, 0L, SEEK_END) != 10) e(6);
		else if (read(fd, b, 1) != READ_EOF) e(7);

		/* Lseek beyond file */
		if (lseek(fd, 10L, SEEK_CUR) != 20) e(8);
		else if (write(fd, a, 10) != 10) e(9);
		else {
			/* Lseek to begin second write */
			if (lseek(fd, 20L, SEEK_SET) != 20) e(10);
			if (read(fd, b, 10) != 10) e(11);
			else if (comp_array(a, b, 10) != OK) e(12);
		}
	}

	/* Lseek to position before begin of file */
	if (lseek(fd, -1L, 0) != FAIL) e(13);

	try_close(fd, "'drwx/rwx'");
  }

  /* Lseek on invalid filediscriptor */
  if (lseek(-1, 0L, SEEK_SET) != FAIL) e(14);
  else
	if (errno != EBADF) e(15);

}

/*****************************************************************************
 *                              test ACCESS                                  *
 ****************************************************************************/
void test07()
{
  subtest = 7;

  /* Check with proper parameters */
  if (access("drwx/rwx", RWX) != OK) e(1);

  if (access("./././drwx/././rwx", 0) != OK) e(2);

  /* Check 8 files with 8 different modes on 8 accesses  */
  if (chdir("drwx") != OK) e(3);

  access_standards();

  if (chdir("..") != OK) e(4);

  /* Check several wrong combinations */
  /* File does not exist */
  if (access("non-file", 0) != FAIL) e(5);
  else
	if (errno != ENOENT) e(6);

  /* Non-searchable dir */
  if (access("drw-/rwx", 0) != FAIL) e(7);
  else
	if (errno != EACCES) e(8);

  /* Searchable dir, but wrong file-mode */
  if (access("drwx/--x", RWX) != FAIL) e(9);
  else
	if (errno != EACCES) e(10);

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
  if (access(fname, mode) != test) e(96);
}				/* try_access */


/* Err, make_and_fill_dirs, init_array, clear_array, comp_array,
   try_close, try_unlink, Remove, get_mode, check, open_alot,
   close_alot, clean_up_the_mess.
*/

/*****************************************************************************
 *                              test READV/WRITEV                            *
 ****************************************************************************/
#define TEST8_BUFSZCOUNT 3
#define TEST8_BUFSZMAX 65536
#define TEST8_IOVCOUNT 4

void test08()
{
  char buffer_read[TEST8_IOVCOUNT * TEST8_BUFSZMAX];
  char buffer_write[TEST8_IOVCOUNT * TEST8_BUFSZMAX];
  struct iovec iovec_read[TEST8_IOVCOUNT];
  struct iovec iovec_write[TEST8_IOVCOUNT];
  int fd, i, j, k, l, m;
  ssize_t sz_read, sz_write;
  size_t sz_read_exp, sz_read_sum, sz_write_sum;

  subtest = 8;

  /* try various combinations of buffer sizes */
  for (i = 0; i <= TEST8_IOVCOUNT; i++)
  for (j = 0; j < power(TEST8_BUFSZCOUNT, i); j++)
  for (k = 0; k <= TEST8_IOVCOUNT; k++)
  for (l = 0; l < power(TEST8_BUFSZCOUNT, k); l++)
  {
	/* put data in the buffers */
	for (m = 0; m < sizeof(buffer_write); m++)
	{
		buffer_write[m] = m ^ (m >> 8);
		buffer_read[m] = ~buffer_write[m];
	} 

	/* set up the vectors to point to the buffers */
	sz_read_sum = iovec_setup(j, iovec_read, buffer_read, i);
	sz_write_sum = iovec_setup(l, iovec_write, buffer_write, k);
	sz_read_exp = (sz_read_sum < sz_write_sum) ? 
		sz_read_sum : sz_write_sum;

	/* test reading and writing */
	if ((fd = open("file08", O_RDWR | O_CREAT | O_TRUNC, 0644)) < 0) e(1);
	else {
		sz_write = writev(fd, iovec_write, k);
		if (sz_write != sz_write_sum) e(2);
		if (lseek(fd, 0, SEEK_SET) != 0) e(3);
		sz_read = readv(fd, iovec_read, i);
		if (sz_read != sz_read_exp)  e(4);
		else {
			if (!iovec_is_equal(iovec_read, iovec_write, sz_read)) 
				e(5);
		}

  		/* Remove testfile */
 		Remove(fd, "file08");
	}
  }
}				/* test08 */

static int iovec_is_equal(struct iovec *x, struct iovec *y, size_t size)
{
  int xpos = 0, xvec = 0, ypos = 0, yvec = 0;

  /* compare byte by byte */
  while (size-- > 0)
  {
	/* skip over zero-byte buffers and those that have been completed */
	while (xpos >= x[xvec].iov_len)
	{
		xpos -= x[xvec++].iov_len;
		assert(xvec < TEST8_IOVCOUNT);
	}
	while (ypos >= y[yvec].iov_len)
	{
		ypos -= y[yvec++].iov_len;
		assert(yvec < TEST8_IOVCOUNT);
	}

	/* compare */
	if (((char *) x[xvec].iov_base)[xpos++] != 
		((char *) y[yvec].iov_base)[ypos++])
		return 0;
  }

  /* no difference found */
  return 1;
}

static size_t iovec_setup(int pattern, struct iovec *iovec, char *buffer, int count)
{
	static const size_t bufsizes[TEST8_BUFSZCOUNT] = { 0, 1, TEST8_BUFSZMAX };
	int i;
	size_t sum = 0;

	/* the pattern specifies each buffer */
	for (i = 0; i < TEST8_IOVCOUNT; i++)
	{
		iovec->iov_base = buffer;
		sum += iovec->iov_len = bufsizes[pattern % TEST8_BUFSZCOUNT];

		iovec++;
		buffer += TEST8_BUFSZMAX;
		pattern /= TEST8_BUFSZCOUNT;
	}

	return sum;
}

static int power(int base, int exponent)
{
	int result = 1;

	/* compute base^exponent */
	while (exponent-- > 0)
		result *= base;

	return result;
}


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

  if (chdir(dirname) != OK) e(97);
  else {
	/* Creat the file */
	if ((nr = creat(fnames[mode], mode * 0100)) < 0) e(98);
	else
		try_close(nr, fnames[mode]);

	if (chdir("..") != OK) e(99);
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
  assert(range >= 0 && range <= ARSIZE);

  while (range-- && (*a++ == *b++));
  if (*--a == *--b)
	return(OK);
  else
	return(FAIL);
}				/* comp_array */

void try_close(filedes, name)
int filedes;
char *name;
{
  if (close(filedes) != OK) e(100);
}				/* try_close */

void try_unlink(fname)
char *fname;
{
  if (unlink(fname) != 0) e(101);
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
	e(102);
	return(stbf1.st_mode);	/* return a mode which will cause *
				 * error in the calling function  *
				 * (file/dir bit)                 */
  } else
	return(stbf1.st_mode & 07777);	/* take last 4 bits */
}				/* get_mode */


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

  if (i == 0)
  	e(103);
  return(i);
}				/* open_alot */

int close_alot(number)
int number;
{
  int i, count = 0;

  if (number > MAXOPEN) e(104);
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
  if (chdir("drwx") != OK) e(105);
  else {
	for (i = 0; i < 8; i++) try_unlink(fnames[i]);
	if (chdir("..") != OK) e(106);
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

