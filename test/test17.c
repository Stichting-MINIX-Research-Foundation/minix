/* Comment on usage and program: ark!/mnt/rene/prac/os/unix/comment.changes */

/* "const.h", created by Rene Montsma and Menno Wilcke */

#include <sys/types.h>		/* type defs */
#include <sys/stat.h>		/* struct stat */
#include <sys/wait.h>
#include <errno.h>		/* the error-numbers */
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <utime.h>
#include <stdio.h>
#include <limits.h>
#include <assert.h>

#define NOCRASH 1		/* test11(), 2nd pipe */
#define PDPNOHANG  1		/* test03(), write_standards() */
#define MAX_ERROR 2

#define USER_ID   12
#define GROUP_ID   1
#define FF        3		/* first free filedes. */
#define USER      1		/* uid */
#define GROUP     0		/* gid */

#define ARSIZE   256		/* array size */
#define PIPESIZE 3584		/* max number of bytes to be written on pipe */
#define MAXOPEN  (OPEN_MAX-3)		/* maximum number of extra open files */
#define MAXLINK 0177		/* maximum number of links per file */
#define LINKCOUNT 5
#define MASK    0777		/* selects lower nine bits */
#define END_FILE     0		/* returned by read-call at eof */

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

/* "decl.c", created by Rene Montsma and Menno Wilcke */

/* Used in open_alot, close_alot */
char *filenames[MAXOPEN];

#define MODES 8
char *mode_fnames[MODES] = {"---", "--x", "-w-", "-wx", "r--", "r-x", "rw-", "rwx"},
*mode_dir[MODES] = {"d---", "d--x", "d-w-", "d-wx", "dr--", "dr-x", "drw-", "drwx"};

 /* Needed for easy creating and deleting of directories */

/* "test.c", created by Rene Montsma and Menno Wilcke */

#include "common.c"

int main(int argc, char *argv []);
void test(int mask);
void test01(void);
void test02(void);
void test08(void);
void test09(void);
void test10(void);
int link_alot(char *bigboss);
int unlink_alot(int number);
void get_new(char name []);
void test11(void);
void nlcr(void);
void str(char *s);
void test03(void);
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
void quit(void);

/*****************************************************************************
 *                              TEST                                         *
 ****************************************************************************/
int main(argc, argv)
int argc;
char *argv[];
{
  int n, mask, i;
  pid_t child;

  start(17);

  /* Create filenames for MAXOPEN files, the *filenames[] array. */
  for(i = 0; i < MAXOPEN; i++) {
	if(asprintf(&filenames[i], "file%d", i) == -1) {
		fprintf(stderr, "asprintf failed\n");
		quit();
	}
  }


  mask = (argc == 2 ? atoi(argv[1]) : 0xFFFF);
  subtest = 0;
  child = fork();
  if (child == -1) {
  	e(1);
  	quit();
  } else if (child == 0) {
  	test(mask);
  	return(0);
  } else {
  	wait(&n);
	clean_up_the_mess();
	quit();
  }
  return(-1);			/* impossible */
}

void test(mask)
int mask;
{
  umask(0);			/* not honest, but i always forget */

  if (mask & 00001) test01();
  if (mask & 00002) test03();
  if (mask & 00004) test02();
  if (mask & 00010) test08();
  if (mask & 00020) test09();
  if (mask & 00040) test10();
  if (mask & 00100) test11();
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
		if (get_mode("file01") != (MASK & ~newvalue))
			e(7);
		try_unlink("file01");
	}
	oldvalue = newvalue;
  }

  /* The loop has terminated with umask(0) */
  if ((tempvalue = umask(0)) != 0)
	e(8);
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

  for (n = 0; n < MAXOPEN; n++) {
	if (creat(filenames[n], mode) != FF + n)
		e(1);
	else {
		if (get_mode(filenames[n]) != mode)
			e(2);

		/* Change  mode of file to standard mode, we want to *
		 * use a lot (20) of files to be opened later, see   *
		 * open_alot(), close_alot().                        */
		if (chmod(filenames[n], 0700) != OK) e(3);
	}
	mode = (mode + 0100) % 01000;
  }

  /* Already twenty files opened; opening another has to fail */
  if (creat("file02", 0777) != FAIL)
	e(4);
  else
  	if (errno != EMFILE) e(5);

  /* Close all files: seems blunt, but it isn't because we've  *
   * checked all fd's already                                  */
  if ((n = close_alot(MAXOPEN)) < MAXOPEN) e(6);

  /* Creat 1 file twice; check */
  if ((n = creat("file02", 0777)) < 0)
	e(7);
  else {
	init_array(a);
	if (write(n, a, ARSIZE) != ARSIZE) e(8);

	if ((n1 = creat("file02", 0755)) < 0)	/* receate 'file02' */
		e(9);
	else {
		/* Fd should be at the top after recreation */
		if (lseek(n1, 0L, SEEK_END) != 0)
			e(10);
		else {
			/* Try to write on recreated file */
			clear_array(b);

			if (lseek(n1, 0L, SEEK_SET) != 0) e(11);
			if (write(n1, a, ARSIZE) != ARSIZE) e(12);

			/* In order to read we've to close and open again */
			try_close(n1, "'file02'  (2nd creation)");
			if ((n1 = open("file02", RW)) < 0)
				e(13);

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
				/* cast because mode is negative :-( */
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

void test08()
{
  subtest = 8;

  /* Test chdir to searchable dir */
  if (chdir("drwx") != OK) e(1);
  else if (chdir("..") != OK) e(2);

  /* Check the chdir(".") and chdir("..") mechanism */
  if (chdir("drwx") != OK) e(3);
  else {
	if (chdir(".") != OK) e(4);

	/* If we still are in 'drwx' , we should be able to access *
	 * file 'rwx'.                                              */
	if (access("rwx", 0) != OK) e(5);

	/* Try to return to previous dir ('/' !!) */
	if (chdir("././../././d--x/../d--x/././..") != OK) e(6);

	/* Check whether we are back in '/' */
	if (chdir("d--x") != OK) e(7);
  }  /* Return to '..' */
  if (chdir("..") != OK) e(8);

  if (chdir("././././drwx") != OK) e(9);
  else if (chdir("././././..") != OK) e(10);

  /* Try giving chdir wrong parameters */
  if (chdir("drwx/rwx") != FAIL) e(11);
  else
	if (errno != ENOTDIR) e(12);

  if (chdir("drw-") != FAIL) e(13);
  else
	if (errno != EACCES) e(14);

}				/* test08 */

/*****************************************************************************
 *                              test CHMOD                                   *
 ****************************************************************************/
void test09()
{
  int n;

  subtest = 9;

  /* Prepare file09 */
  if ((n = creat("drwx/file09", 0644)) != FF) e(1);

  try_close(n, "'file09'");

  /* Try to chmod a file, check and restore old values, check */
  if (chmod("drwx/file09", 0700) != OK) e(2);
  else {
	/* Check protection */
	if (get_mode("drwx/file09") != 0700) e(3);

	/* Test if chmod accepts just filenames too */
	if (chdir("drwx") != OK) e(4);
	else if (chmod("file09", 0177) != OK) e(5);
	else
		/* Check if value has been restored */
	if (get_mode("../drwx/file09") != 0177) e(6);
  }

  /* Try setuid and setgid */
  if ((chmod("file09", 04777) != OK) || (get_mode("file09") != 04777))
	e(7);
  if ((chmod("file09", 02777) != OK) || (get_mode("file09") != 02777))
	e(8);

  /* Remove testfile */
  try_unlink("file09");

  if (chdir("..") != OK) e(9);

  /* Try to chmod directory */
  if (chmod("d---", 0777) != OK) e(10);
  else {
	if (get_mode("d---") != 0777) e(11);
	if (chmod("d---", 0000) != OK) e(12);

	/* Check if old value has been restored */
	if (get_mode("d---") != 0000) e(13);
  }

  /* Try to make chmod failures */

  /* We still are in dir root */
  /* Wrong filename */
  if (chmod("non-file", 0777) != FAIL) e(14);
  else
	if (errno != ENOENT) e(15);

}				/* test 09 */


/* "t4.c", created by Rene Montsma and Menno Wilcke */

/*****************************************************************************
 *                              test LINK/UNLINK                             *
 ****************************************************************************/
void test10()
{
  int n, n1;
  char a[ARSIZE], b[ARSIZE], *f, *lf;

  subtest = 10;

  f = "anotherfile10";
  lf = "linkfile10";

  if ((n = creat(f, 0702)) != FF) e(1);	/* no other open files */
  else {
	/* Now link correctly */
	if (link(f, lf) != OK) e(2);
	else if ((n1 = open(lf, RW)) < 0) e(3);
	else {
		init_array(a);
		clear_array(b);

		/* Write on 'file10' means being able to    * read
		 * through linked filedescriptor       */
		if (write(n, a, ARSIZE) != ARSIZE) e(4);
		if (read(n1, b, ARSIZE) != ARSIZE) e(5);
		if (comp_array(a, b, ARSIZE) != OK) e(6);

		/* Clean up: unlink and close (twice): */
		Remove(n, f);
		try_close(n1, "'linkfile10'");

		/* Check if "linkfile" exists and the info    * on it
		 * is correct ('file' has been deleted) */
		if ((n1 = open(lf, R)) < 0) e(7);
		else {
			/* See if 'linkfile' still contains 0..511 ? */

			clear_array(b);
			if (read(n1, b, ARSIZE) != ARSIZE) e(8);
			if (comp_array(a, b, ARSIZE) != OK) e(9);

			try_close(n1, "'linkfile10' 2nd time");
			try_unlink(lf);
		}
	}
  }

  /* Try if unlink fails with incorrect parameters */
  /* File does not exist: */
  if (unlink("non-file") != FAIL) e(10);
  else
  	if (errno != ENOENT) e(11);

  /* Dir can't be written */
  if (unlink("dr-x/rwx") != FAIL) e(12);
  else
  	if (errno != EACCES) e(13);

  /* Try to unlink a dir being user */
  if (unlink("drwx") != FAIL) e(14);
  else
	if (errno != EPERM) e(15);

  /* Try giving link wrong input */

  /* First try if link fails with incorrect parameters * name1 does not
   * exist.                             */
  if (link("non-file", "linkfile") != FAIL) e(16);
  else
	if (errno != ENOENT) e(17);

  /* Name2 exists already */
  if (link("drwx/rwx", "drwx/rw-") != FAIL) e(18);
  else
	if (errno != EEXIST) e(19);

  /* Directory of name2 not writable:  */
  if (link("drwx/rwx", "dr-x/linkfile") != FAIL) e(20);
  else
	if (errno != EACCES) e(21);

  /* Try to link a dir, being a user */
  if (link("drwx", "linkfile") != FAIL) e(22);
  else
	if (errno != EPERM) e(23);

  /* File has too many links */
  if ((n = link_alot("drwx/rwx")) != LINKCOUNT - 1)	/* file already has one
							 * link */
	e(24);
  if (unlink_alot(n) != n) e(25);

}				/* test10 */

int link_alot(bigboss)
char *bigboss;
{
  int i;
  static char employee[6] = "aaaaa";

  /* Every file has already got 1 link, so link 0176 times */
  for (i = 1; i < LINKCOUNT; i++) {
	if (link(bigboss, employee) != OK)
		break;
	else
		get_new(employee);
  }

  return(i - 1);		/* number of linked files */
}				/* link_alot */

int unlink_alot(number)
int number;			/* number of files to be unlinked */
{
  int j;
  static char employee[6] = "aaaaa";

  for (j = 0; j < number; j++) {
	if (unlink(employee) != OK)
		break;
	else
		get_new(employee);
  }

  return(j);			/* return number of unlinked files */
}				/* unlink_alot */

void get_new(name)
char name[];
 /* Every call changes string 'name' to a string alphabetically          *
  * higher. Start with "aaaaa", next value: "aaaab" .                    *
  * N.B. after "aaaaz" comes "aaabz" and not "aaaba" (not needed).       *
  * The last possibility will be "zzzzz".                                *
  * Total # possibilities: 26+25*4 = 126 = MAXLINK -1 (exactly needed)   */
{
  int i;

  for (i = 4; i >= 0; i--)
	if (name[i] != 'z') {
		name[i]++;
		break;
	}
}				/* get_new */


/*****************************************************************************
 *                              test PIPE                                    *
 ****************************************************************************/
void test11()
{
  int n, fd[2];
  char a[ARSIZE], b[ARSIZE];

  subtest = 11;

  if (pipe(fd) != OK) e(1);
  else {
	/* Try reading and writing on a pipe */
	init_array(a);
	clear_array(b);

	if (write(fd[1], a, ARSIZE) != ARSIZE) e(2);
	else if (read(fd[0], b, (ARSIZE / 2)) != (ARSIZE / 2)) e(3);
	else if (comp_array(a, b, (ARSIZE / 2)) != OK) e(4);
	else if (read(fd[0], b, (ARSIZE / 2)) != (ARSIZE / 2)) e(5);
	else if (comp_array(&a[ARSIZE / 2], b, (ARSIZE / 2)) != OK) e(6);

	/* Try to let the pipe make a mistake */
	if (write(fd[0], a, ARSIZE) != FAIL) e(7);
	if (read(fd[1], b, ARSIZE) != FAIL) e(8);

	try_close(fd[1], "'fd[1]'");

	/* Now we shouldn't be able to read, because fd[1] has been closed */
	if (read(fd[0], b, ARSIZE) != END_FILE) e(9);

	try_close(fd[0], "'fd[0]'");
  }
  if (pipe(fd) < 0) e(10);
  else {
	/* Test lseek on a pipe: should fail */
	if (write(fd[1], a, ARSIZE) != ARSIZE) e(11);
	if (lseek(fd[1], 10L, SEEK_SET) != FAIL) e(12);
	else
		if (errno != ESPIPE) e(13);

	/* Eat half of the pipe: no writing should be possible */
	try_close(fd[0], "'fd[0]'  (2nd time)");

	/* This makes UNIX crash: omit it if pdp or VAX */
#ifndef NOCRASH
	if (write(fd[1], a, ARSIZE) != FAIL) e(14);
	else
		if (errno != EPIPE) e(15);
#endif
	try_close(fd[1], "'fd[1]'  (2nd time)");
  }

  /* BUG :                                                            *
   * Here we planned to test if we could write 4K bytes on a pipe.    *
   * However, this was not possible to implement, because the whole   *
   * Monix system crashed when we tried to write more then 3584 bytes *
   * (3.5K) on a pipe. That's why we try to write only 3.5K in the    *
   * folowing test.                                                   */
  if (pipe(fd) < 0) e(16);
  else {
	for (n = 0; n < (PIPESIZE / ARSIZE); n++)
		if (write(fd[1], a, ARSIZE) != ARSIZE) e(17);
	try_close(fd[1], "'fd[1]' (3rd time)");

	for (n = 0; n < (PIPESIZE / ARSIZE); n++)
		if (read(fd[0], b, ARSIZE) != ARSIZE) e(18);
	try_close(fd[0], "'fd[0]' (3rd time)");
  }

  /* Test opening a lot of files */
  if ((n = open_alot()) != MAXOPEN) e(19);
  if (pipe(fd) != FAIL) e(20);
  else
	if (errno != EMFILE) e(21);
  if (close_alot(n) != n) e(22);
}				/* test11 */


/* Err, test03, init_array, clear_array, comp_array,
   try_close, try_unlink, Remove, get_mode, check, open_alot,
   close_alot, clean_up_the_mess.
*/


/*****************************************************************************
*                                                                            *
*                          MAKE_AND_FILL_DIRS                                *
*                                                                            *
*****************************************************************************/

void test03()
 /* Create 8 dir.'s: "d---", "d--x", "d-w-", "d-wx", "dr--", "dr-x",     *
  * "drw-", "drwx".                                     * Then create 8 files
  * in "drwx", and some needed files in other dirs.  */
{
  int mode, i;
  subtest = 3;
  for (i = 0; i < MODES; i++) {
	mkdir(mode_dir[i], 0700);
	chown(mode_dir[i], USER_ID, GROUP_ID);
  }
  setuid(USER_ID);
  setgid(GROUP_ID);

  for (mode = 0; mode < 8; mode++) put_file_in_dir("drwx", mode);

  put_file_in_dir("d-wx", RWX);
  put_file_in_dir("dr-x", RWX);
  put_file_in_dir("drw-", RWX);

  chmod_8_dirs(8);		/* 8 means; 8 different modes */

}				/* test03 */

void put_file_in_dir(dirname, mode)
char *dirname;
int mode;
 /* Fill directory 'dirname' with file with mode 'mode'.   */
{
  int nr;

  if (chdir(dirname) != OK) e(1);
  else {
	/* Creat the file */
	assert(mode >= 0 && mode < MODES);
	if ((nr = creat(mode_fnames[mode], mode * 0100)) < 0) e(2);
	else {
		try_close(nr, mode_fnames[mode]);
	}

	if (chdir("..") != OK) e(3);
  }
}				/* put_file_in_dir */

/*****************************************************************************
*                                                                            *
*                               MISCELLANEOUS                                *
*                                                                            *
*(all about arrays, 'try_close', 'try_unlink', 'Remove', 'get_mode')*
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
  if (close(filedes) != OK) e(90);
}				/* try_close */

void try_unlink(fname)
char *fname;
{
  if (unlink(fname) != 0) e(91);
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
	e(92);
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
	if (open(filenames[i], R) == FAIL) break;
  if (i == 0) e(93);
  return(i);
}				/* open_alot */

int close_alot(number)
int number;
{
  int i, count = 0;

  if (number > MAXOPEN)
	e(94);
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

  /* First remove 'a lot' files */
  for (i = 0; i < MAXOPEN; i++) {
	try_unlink(filenames[i]);
}

  /* Unlink the files in dir 'drwx' */
  if (chdir("drwx") != OK)
	e(95);
  else {
	for (i = 0; i < MODES; i++) {
		try_unlink(mode_fnames[i]);
	}
	if (chdir("..") != OK) e(96);
  }

  /* Before unlinking files in some dirs, make them writable */
  chmod_8_dirs(RWX);

  /* Unlink files in other dirs */
  try_unlink("d-wx/rwx");
  try_unlink("dr-x/rwx");
  try_unlink("drw-/rwx");

  /* Unlink dirs */
  for (i = 0; i < MODES; i++) {
	strcpy(dirname, "d");
	strcat(dirname, mode_fnames[i]);

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

  for (i = 0; i < MODES; i++) {
	chmod(mode_dir[i], 040000 + mode * 0100);
	if (sw == 8) mode++;
  }
}

