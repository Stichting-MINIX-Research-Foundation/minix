/* test23: chdir(), getcwd()	Author: Jan-Mark Wams (jms@cs.vu.nl) */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/dir.h>
#include <limits.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>

#define MAX_ERROR 4
#define ITERATIONS 3

#define System(cmd)	if (system(cmd) != 0) printf("``%s'' failed\n", cmd)
#define Chdir(dir)	if (chdir(dir) != 0) printf("Can't goto %s\n", dir)

int errct;
int subtest;
int superuser;			/* True if we are root. */

char cwd[PATH_MAX];		/* Space for path names. */
char cwd2[PATH_MAX];
char buf[PATH_MAX];
char MaxName[NAME_MAX + 1];	/* Name of maximum length */
char MaxPath[PATH_MAX];		/* Same for path */
char ToLongName[NAME_MAX + 2];	/* Name of maximum +1 length */
char ToLongPath[PATH_MAX + 1];	/* Same for path, both too long */

_PROTOTYPE(void main, (int argc, char *argv[]));
_PROTOTYPE(void test23a, (void));
_PROTOTYPE(void test23b, (void));
_PROTOTYPE(void test23c, (void));
_PROTOTYPE(void makelongnames, (void));	/* Fill MaxName etc. */
_PROTOTYPE(char *last_index, (char *string, int ch));
_PROTOTYPE(char *my_getcwd, (char *buf, int size));
_PROTOTYPE(void e, (int number));
_PROTOTYPE(void quit, (void));

void main(argc, argv)
int argc;
char *argv[];
{
  int i, m = 0xFFFF;

  sync();
  if (argc == 2) m = atoi(argv[1]);
  printf("Test 23 ");
  fflush(stdout);
  System("rm -rf DIR_23; mkdir DIR_23");
  Chdir("DIR_23");
  makelongnames();
  superuser = (geteuid() == 0);

  for (i = 0; i < ITERATIONS; i++) {
	if (m & 0001) test23a();	/* Test normal operation */
	if (m & 0002) test23b();	/* Test critical operation */
	if (m & 0004) test23c();	/* Test error operation */
  }

  quit();
}

void test23a()
{				/* Test normal operation. */
  register int i;

  subtest = 1;

  System("rm -rf ../DIR_23/*");

  /* Let's do some fiddeling with path names. */
  if (getcwd(cwd, PATH_MAX) != cwd) e(1);
  if (chdir(cwd) != 0) e(2);
  if (getcwd(buf, PATH_MAX) != buf) e(3);
  if (strcmp(buf, cwd) != 0) e(4);
  if (chdir(".") != 0) e(5);
  if (getcwd(buf, PATH_MAX) != buf) e(6);
  if (strcmp(buf, cwd) != 0) e(7);
  if (chdir("./././.") != 0) e(8);
  if (getcwd(buf, PATH_MAX) != buf) e(9);
  if (strcmp(buf, cwd) != 0) e(10);

  /* Creat a working dir named "foo", remove any previous residues. */
  System("rm -rf foo");
  if (mkdir("foo", 0777) != 0) e(11);

  /* Do some more fiddeling with path names. */
  if (chdir("foo/.././foo/..") != 0) e(12);	/* change to cwd */
  if (getcwd(buf, PATH_MAX) != buf) e(13);
  if (strcmp(buf, cwd) != 0) e(13);
  if (chdir("foo") != 0) e(14);	/* change to foo */
  if (chdir("..") != 0) e(15);	/* and back again */
  if (getcwd(buf, PATH_MAX) != buf) e(16);
  if (strcmp(buf, cwd) != 0) e(17);

  /* Make 30 sub dirs, eg. ./bar/bar/bar/bar/bar...... */
  System("rm -rf bar");		/* get ridd of bar */
  for (i = 0; i < 30; i++) {
	if (mkdir("bar", 0777) != 0) e(18);
	if (chdir("bar") != 0) e(19);	/* change to bar */
  }
  for (i = 0; i < 30; i++) {
	if (chdir("..") != 0) e(20);	/* and back again */
	if (rmdir("bar") != 0) e(21);
  }

  /* Make sure we are back where we started. */
  if (getcwd(buf, PATH_MAX) != buf) e(22);
  if (strcmp(buf, cwd) != 0) e(23);
  System("rm -rf bar");		/* just incase */

  /* Do some normal checks on `Chdir()' and `getcwd()' */
  if (chdir("/") != 0) e(24);
  if (getcwd(buf, PATH_MAX) != buf) e(25);
  if (strcmp(buf, "/") != 0) e(26);
  if (chdir("..") != 0) e(27);	/* go to parent of / */
  if (getcwd(buf, PATH_MAX) != buf) e(28);
  if (strcmp(buf, "/") != 0) e(29);
  if (chdir(cwd) != 0) e(30);
  if (getcwd(buf, PATH_MAX) != buf) e(31);
  if (strcmp(buf, cwd) != 0) e(32);
  if (chdir("/etc") != 0) e(33);	/* /etc might be on RAM */
  if (getcwd(buf, PATH_MAX) != buf) e(34);	/* might make a difference */
  if (strcmp(buf, "/etc") != 0) e(35);
  if (chdir(cwd) != 0) e(36);
  if (getcwd(buf, PATH_MAX) != buf) e(37);
  if (strcmp(buf, cwd) != 0) e(38);
  if (chdir(".//.//") != 0) e(39);	/* .//.// == current dir */
  if (getcwd(buf, PATH_MAX) != buf) e(40);
  if (strcmp(buf, cwd) != 0) e(41);	/* we might be at '/' */
#ifdef _MINIX
  /* XXX - my_getcwd() is old rubbish.  It reads the directory directly instead
   * of through the directory library.  It uses a fixed size buffer instead of
   * a size related to PATH_MAX, NAME_MAX or the size required.
   */
  if (my_getcwd(buf, PATH_MAX) != buf) e(42);	/* get cwd my way */
  if (strcmp(cwd, buf) != 0) e(43);
#endif
  System("rm -rf foo");
}

void test23b()
{				/* Test critical values. */
  subtest = 2;

  System("rm -rf ../DIR_23/*");

  /* Fiddle with the size (2nt) parameter of `getcwd ()'. */
  if (getcwd(cwd, PATH_MAX) != cwd) e(1);	/* get cwd */
  if (getcwd(buf, strlen(cwd)) != (char *) 0) e(2);   /* size 1 to small */
  if (errno != ERANGE) e(3);
  if (getcwd(buf, PATH_MAX) != buf) e(4);
  if (strcmp(buf, cwd) != 0) e(5);
  Chdir(cwd);			/* getcwd might cd / */
  if (getcwd(buf, strlen(cwd) + 1) != buf) e(6);	/* size just ok */
  if (getcwd(buf, PATH_MAX) != buf) e(7);
  if (strcmp(buf, cwd) != 0) e(8);

  /* Let's see how "MaxName" and "ToLongName" are handled. */
  if (mkdir(MaxName, 0777) != 0) e(9);
  if (chdir(MaxName) != 0) e(10);
  if (chdir("..") != 0) e(11);
  if (rmdir(MaxName) != 0) e(12);
  if (getcwd(buf, PATH_MAX) != buf) e(13);
  if (strcmp(buf, cwd) != 0) e(14);
  if (chdir(MaxPath) != 0) e(15);
  if (getcwd(buf, PATH_MAX) != buf) e(16);
  if (strcmp(buf, cwd) != 0) e(17);

  if (chdir(ToLongName) != -1) e(18);
#ifdef _POSIX_NO_TRUNC
# if _POSIX_NO_TRUNC - 0 != -1
  if (errno != ENAMETOOLONG) e(20);
# else
  if (errno != ENOENT) e(20);
# endif
#else
# include "error, this case requires dynamic checks and is not handled"
#endif

  if (getcwd(buf, PATH_MAX) != buf) e(21);
  if (strcmp(buf, cwd) != 0) e(22);
  if (chdir(ToLongPath) != -1) e(23);
  if (errno != ENAMETOOLONG) e(24);
  if (getcwd(buf, PATH_MAX) != buf) e(25);
  if (strcmp(buf, cwd) != 0) e(26);
}

void test23c()
{				/* Check reaction to errors */
  subtest = 3;

  System("rm -rf ../DIR_23/*");

  if (getcwd(cwd, PATH_MAX) != cwd) e(1);	/* get cwd */

  /* Creat a working dir named "foo", remove any previous residues. */
  System("rm -rf foo; mkdir foo");

  /* Check some obviouse errors. */
  if (chdir("") != -1) e(2);
  if (errno != ENOENT) e(3);
  if (getcwd(buf, PATH_MAX) != buf) e(4);
  if (strcmp(buf, cwd) != 0) e(5);
  if (getcwd(buf, 0) != (char *) 0) e(6);
  if (errno != EINVAL) e(7);
  if (getcwd(buf, PATH_MAX) != buf) e(8);
  if (strcmp(buf, cwd) != 0) e(9);
  if (getcwd(buf, 0) != (char *) 0) e(10);
  if (errno != EINVAL) e(11);
  if (getcwd(buf, PATH_MAX) != buf) e(12);
  if (strcmp(buf, cwd) != 0) e(13);
  if (chdir(cwd) != 0) e(14);	/* getcwd might be buggy. */

  /* Change the mode of foo, and check the effect. */
  if (chdir("foo") != 0) e(15);	/* change to foo */
  if (mkdir("bar", 0777) != 0) e(16);	/* make a new dir bar */
  if (getcwd(cwd2, PATH_MAX) != cwd2) e(17);	/* get the new cwd */
  if (getcwd(buf, 3) != (char *) 0) e(18);	/* size is too small */
  if (errno != ERANGE) e(19);
  if (getcwd(buf, PATH_MAX) != buf) e(20);
  if (strcmp(buf, cwd2) != 0) e(21);
  Chdir(cwd2);			/* getcwd() might cd / */
  System("chmod 377 .");	/* make foo unreadable */
  if (getcwd(buf, PATH_MAX) != buf) e(22);	/* dir not readable */
  if (getcwd(buf, PATH_MAX) != buf) e(23);
  if (strcmp(buf, cwd2) != 0) e(24);
  if (chdir("bar") != 0) e(25);	/* at .../foo/bar */
  if (!superuser) {
	if (getcwd(buf, PATH_MAX) != (char *) 0) e(26);
	if (errno != EACCES) e(27);
  }
  if (superuser) {
	if (getcwd(buf, PATH_MAX) != buf) e(28);
  }
  if (chdir(cwd2) != 0) e(29);
  if (getcwd(buf, PATH_MAX) != buf) e(30);
  if (strcmp(buf, cwd2) != 0) e(31);
  System("chmod 677 .");	/* make foo inaccessable */
  if (!superuser) {
	if (getcwd(buf, PATH_MAX) != (char *) 0) e(32);	/* try to get cwd */
	if (errno != EACCES) e(33);	/* but no access */
	if (chdir("..") != -1) e(34);	/* try to get back */
	if (errno != EACCES) e(35);	/* again no access */
	if (chdir(cwd) != 0) e(36);	/* get back to cwd */
	/* `Chdir()' might do path optimizing, it shouldn't. */
	if (chdir("foo/..") != -1) e(37);	/* no op */
	if (chdir("foo") != -1) e(38);	/* try to cd to foo */
	if (errno != EACCES) e(39);	/* no have access */
	if (getcwd(buf, PATH_MAX) != buf) e(40);
	if (strcmp(buf, cwd) != 0) e(41);
  }
  if (superuser) {
	if (getcwd(buf, PATH_MAX) != buf) e(42);
	if (strcmp(buf, cwd2) != 0) e(43);
	if (chdir("..") != 0) e(44);	/* get back to cwd */
	if (chdir("foo") != 0) e(45);	/* get back to foo */
	if (chdir(cwd) != 0) e(46);	/* get back to cwd */
  }
  if (getcwd(buf, PATH_MAX) != buf) e(47);	/* check we are */
  if (strcmp(buf, cwd) != 0) e(48);	/* back at cwd. */
  Chdir(cwd);			/* just in case... */

  if (chdir("/etc/passwd") != -1) e(49);	/* try to change to a file */
  if (errno != ENOTDIR) e(50);
  if (getcwd(buf, PATH_MAX) != buf) e(51);
  if (strcmp(buf, cwd) != 0) e(52);
  if (chdir("/notexist") != -1) e(53);
  if (errno != ENOENT) e(54);
  if (getcwd(buf, PATH_MAX) != buf) e(55);
  if (strcmp(buf, cwd) != 0) e(56);
  System("chmod 777 foo");
  if (chdir("foo") != 0) e(57);

  /* XXX - this comment was botched by 'pretty'. */
  /* * Since `foo' is the cwd, it should not be removeable but * if it
   * were, this code would be found here; *
   * 
   *  System ("cd .. ; rm -rf foo");	remove foo *  if (chdir (".")
   * != -1) e();		try go to. *  if (errno != ENOENT) e();
   * hould not be an entry *  if (chdir ("..") != -1) e();	try
   * to get back *  if (errno != ENOENT) e();		should not be
   * an entry *  if (getcwd (buf, PATH_MAX) != (char *)0) e(); don't
   * know where we are *
   * 
   * What should errno be now ? The cwd might be gone if te superuser *
   * removed the cwd. (Might even have linked it first.) But this *
   * testing should be done by the test program for `rmdir()'. */
  if (chdir(cwd) != 0) e(58);
}

void makelongnames()
{
  register int i;

  memset(MaxName, 'a', NAME_MAX);
  MaxName[NAME_MAX] = '\0';
  for (i = 0; i < PATH_MAX - 1; i++) {	/* idem path */
	MaxPath[i++] = '.';
	MaxPath[i] = '/';
  }
  MaxPath[PATH_MAX - 1] = '\0';

  strcpy(ToLongName, MaxName);	/* copy them Max to ToLong */
  strcpy(ToLongPath, MaxPath);

  ToLongName[NAME_MAX] = 'a';
  ToLongName[NAME_MAX + 1] = '\0';	/* extend ToLongName by one too many */
  ToLongPath[PATH_MAX - 1] = '/';
  ToLongPath[PATH_MAX] = '\0';	/* inc ToLongPath by one */
}

/* The following code, is take from pwd written by Adri Koppes */

/* My_getcwd() helper. */
char *last_index(string, ch)
char *string;
char ch;
{
  register char *retval = '\0';

  while (*string != '\0') {
	if (*string == ch) retval = string;
	string++;
  }
  return(retval);
}

char *my_getcwd(buf, size)
char *buf;
int size;
{				/* should be like getcwd() */
  int sd;
  register int fd;
  register char *n;
  char name[128];
  struct stat s, st, sk;
  struct direct d;

  if (size <= 0) return(char *) 0;

  *buf = '\0';
  *name = '\0';
  stat(".", &s);
  do {
	if ((fd = open("..", O_RDONLY)) < 0) return(char *) 0;
	st.st_dev = s.st_dev;
	st.st_ino = s.st_ino;
	stat("..", &s);
	Chdir("..");
	sd = sizeof(struct direct);
	if (s.st_dev == st.st_dev) {
		do {
			if (read(fd, (char *) &d, sd) < sd) return(char *) 0;
		} while (d.d_ino != st.st_ino);
	} else {
		do {
			if (read(fd, (char *) &d, sd) < sd) return(char *) 0;
			stat(d.d_name, &sk);
		} while ((sk.st_dev != st.st_dev) ||
			 (sk.st_ino != st.st_ino));
	}
	close(fd);
	if (strcmp(".", d.d_name) != 0) {
		strcat(name, "/");
		strcat(name, d.d_name);
	}
  } while ((s.st_ino != st.st_ino) || (s.st_dev != st.st_dev));
  if (*name == '\0')
	strncat(buf, "/", size);
  else
	while ((n = last_index(name, '/')) != NULL) {
		n[NAME_MAX] = '\0';
		strncat(buf, n, size - strlen(buf));
		*n = '\0';
	}
  strncat(buf, name, size - strlen(buf));
  buf[size - 1] = '\0';
  Chdir(buf);			/* get back there */
  return buf;
}

void e(n)
int n;
{
  int err_num = errno;		/* Save in case printf clobbers it. */

  printf("Subtest %d,  error %d  errno=%d: ", subtest, n, errno);
  errno = err_num;
  perror("");
  if (errct++ > MAX_ERROR) {
	printf("Too many errors; test aborted\n");
	chdir("..");
	system("rm -rf DIR*");
	exit(1);
  }
  errno = 0;
}

void quit()
{
  Chdir("..");
  System("rm -rf DIR_23");

  if (errct == 0) {
	printf("ok\n");
	exit(0);
  } else {
	printf("%d errors\n", errct);
	exit(1);
  }
}
