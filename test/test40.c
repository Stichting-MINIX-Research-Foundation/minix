/* test40: link() unlink()	Aithor: Jan-Mark Wams (jms@cs.vu.nl) */

/*
 * Not tested readonly file systems
 * Not tested fs full
 * Not tested unlinking bussy files
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <time.h>
#include <stdio.h>

int errct = 0;
int subtest = 1;
int superuser;
char MaxName[NAME_MAX + 1];	/* Name of maximum length */
char MaxPath[PATH_MAX];		/* Same for path */
char ToLongName[NAME_MAX + 2];	/* Name of maximum +1 length */
char ToLongPath[PATH_MAX + 1];	/* Same for path, both too long */

#define MAX_ERROR 4
#define ITERATIONS 2		/* LINK_MAX is high, so time consuming. */

#define System(cmd)	if (system(cmd) != 0) printf("``%s'' failed\n", cmd)
#define Chdir(dir)	if (chdir(dir) != 0) printf("Can't goto %s\n", dir)
#define Stat(a,b)	if (stat(a,b) != 0) printf("Can't stat %s\n", a)

_PROTOTYPE(void main, (int argc, char *argv[]));
_PROTOTYPE(void test40a, (void));
_PROTOTYPE(void test40b, (void));
_PROTOTYPE(void test40c, (void));
_PROTOTYPE(int stateq, (struct stat *stp1, struct stat *stp2));
_PROTOTYPE(void makelongnames, (void));
_PROTOTYPE(void e, (int __n));
_PROTOTYPE(void quit, (void));

void main(argc, argv)
int argc;
char *argv[];
{
  int i, m = 0xFFFF;

  sync();
  if (argc == 2) m = atoi(argv[1]);
  printf("Test 40 ");
  fflush(stdout);
  System("rm -rf DIR_40; mkdir DIR_40");
  Chdir("DIR_40");
  superuser = (getuid() == 0);
  makelongnames();

  for (i = 0; i < ITERATIONS; i++) {
	if (m & 0001) test40a();
	if (m & 0002) test40b();
	if (m & 0004) test40c();
  }
  quit();
}

void test40a()
{				/* Test normal operation. */
  struct stat st1, st2, st3;
  time_t time1;

  subtest = 1;

  /* Clean up any residu. */
  System("rm -rf ../DIR_40/*");

  System("touch foo");		/* make source file */
  Stat("foo", &st1);		/* get info of foo */
  Stat(".", &st2);		/* and the cwd */
  time(&time1);
  while (time1 >= time((time_t *)0))
	;			/* wait a sec */
  if (link("foo", "bar") != 0) e(1);	/* link foo to bar */
  Stat("foo", &st3);		/* get new status */
  if (st1.st_nlink + 1 != st3.st_nlink) e(2);	/* link count foo up 1 */
#ifndef V1_FILESYSTEM
  if (st1.st_ctime >= st3.st_ctime) e(3);	/* check stattime changed */
#endif
  Stat(".", &st1);		/* get parend dir info */
  if (st2.st_ctime >= st1.st_ctime) e(4);	/* ctime and mtime */
  if (st2.st_mtime >= st1.st_mtime) e(5);	/* should be updated */
  Stat("bar", &st2);		/* get info of bar */
  if (st2.st_nlink != st3.st_nlink) e(6);	/* link count foo == bar */
  if (st2.st_ino != st3.st_ino) e(7);	/* ino should be same */
  if (st2.st_mode != st3.st_mode) e(8);	/* check mode same */
  if (st2.st_uid != st3.st_uid) e(9);	/* check uid same */
  if (st2.st_gid != st3.st_gid) e(10);	/* check gid same */
  if (st2.st_size != st3.st_size) e(11);	/* check size */
  if (st2.st_ctime != st3.st_ctime) e(12);	/* check ctime */
  if (st2.st_atime != st3.st_atime) e(13);	/* check atime */
  if (st2.st_mtime != st3.st_mtime) e(14);	/* check mtime */
  Stat("foo", &st1);		/* get fooinfo */
  Stat(".", &st2);		/* get dir info */
  time(&time1);
  while (time1 >= time((time_t *)0))
	;			/* wait a sec */
  if (unlink("bar") != 0) e(15);/* rm bar */
  if (stat("bar", &st2) != -1) e(16);	/* it's gone */
  Stat("foo", &st3);		/* get foo again */
  if (st1.st_nlink != st3.st_nlink + 1) e(17);	/* link count back to normal */
#ifndef V1_FILESYSTEM
  if (st1.st_ctime >= st3.st_ctime) e(18);	/* check ctime */
#endif
  Stat(".", &st3);		/* get parend dir info */
  if (st2.st_ctime >= st3.st_ctime) e(19);	/* ctime and mtime */
  if (st2.st_mtime >= st3.st_mtime) e(20);	/* should be updated */
}

void test40b()
{
  register int nlink;
  char bar[30];
  struct stat st, st2;

  subtest = 2;

  /* Clean up any residu. */
  System("rm -rf ../DIR_40/*");

  /* Test what happens if we make LINK_MAX number of links. */
  System("touch foo");
  for (nlink = 2; nlink <= LINK_MAX; nlink++) {
  	sprintf(bar, "bar.%d", nlink);
	if (link("foo", bar) != 0) e(2);
	Stat(bar, &st);
	if (st.st_nlink != nlink) e(3);
	Stat("foo", &st);
	if (st.st_nlink != nlink) e(4);
  }

  /* Check if we have LINK_MAX links that are all the same. */
  Stat("foo", &st);
  if (st.st_nlink != LINK_MAX) e(5);
  for (nlink = 2; nlink <= LINK_MAX; nlink++) {
  	sprintf(bar, "bar.%d", nlink);
	Stat(bar, &st2);
	if (!stateq(&st, &st2)) e(6);
  }

  /* Test no more links are possible. */
  if (link("foo", "nono") != -1) e(7);
  if (stat("nono", &st) != -1) e(8);
  Stat("foo", &st);
  if (st.st_nlink != LINK_MAX) e(9);	/* recheck the number of links */

  /* Now unlink() the bar.### files */
  for (nlink = LINK_MAX; nlink >= 2; nlink--) {
  	sprintf(bar, "bar.%d", nlink);
	Stat(bar, &st);
	if (st.st_nlink != nlink) e(10);
	Stat("foo", &st2);
	if (!stateq(&st, &st2)) e(11);
	if (unlink(bar) != 0) e(12);
  }
  Stat("foo", &st);
  if (st.st_nlink != 1) e(13);	/* number of links back to 1 */

  /* Test max path ed. */
  if (link("foo", MaxName) != 0) e(14);	/* link to MaxName */
  if (unlink(MaxName) != 0) e(15);	/* and remove it */
  MaxPath[strlen(MaxPath) - 2] = '/';
  MaxPath[strlen(MaxPath) - 1] = 'a';	/* make ././.../a */
  if (link("foo", MaxPath) != 0) e(16);	/* it should be */
  if (unlink(MaxPath) != 0) e(17);	/* (un)linkable */

  System("rm -f ../DIR_40/*");	/* clean cwd */
}

void test40c()
{
  subtest = 3;

  /* Clean up any residu. */
  System("rm -rf ../DIR_40/*");

  /* Check some simple things. */
  if (link("bar/nono", "nono") != -1) e(1);	/* nonexistent */
  if (errno != ENOENT) e(2);
  Chdir("..");
  System("touch DIR_40/foo");
  System("chmod 677 DIR_40");	/* make inaccesable */
  if (!superuser) {
	if (unlink("DIR_40/foo") != -1) e(3);
	if (errno != EACCES) e(4);
  }
  if (link("DIR_40/bar/nono", "DIR_40/nono") != -1) e(5); /* nono no be */
  if (superuser) {
	if (errno != ENOENT) e(6);	/* su has access */
  }
  if (!superuser) {
	if (errno != EACCES) e(7);	/* we don't ;-) */
  }
  System("chmod 577 DIR_40");	/* make unwritable */
  if (superuser) {
	if (link("DIR_40/foo", "DIR_40/nono") != 0) e(8);
	if (unlink("DIR_40/nono") != 0) e(9);
  }
  if (!superuser) {
	if (link("DIR_40/foo", "DIR_40/nono") != -1) e(10);
	if (errno != EACCES) e(11);
	if (unlink("DIR_40/foo") != -1) e(12);	/* try to rm foo/foo */
	if (errno != EACCES) e(13);
  }
  System("chmod 755 DIR_40");	/* back to normal */
  Chdir("DIR_40");

  /* Too-long path and name test */
  ToLongPath[strlen(ToLongPath) - 2] = '/';
  ToLongPath[strlen(ToLongPath) - 1] = 'a';	/* make ././.../a */
  if (link("foo", ToLongPath) != -1) e(18);	/* path is too long */
  if (errno != ENAMETOOLONG) e(19);
  if (unlink(ToLongPath) != -1) e(20);	/* path is too long */
  if (errno != ENAMETOOLONG) e(21);
  if (link("foo", "foo") != -1) e(22);	/* try linking foo to foo */
  if (errno != EEXIST) e(23);
  if (link("foo", "bar") != 0) e(24);	/* make a link to bar */
  if (link("foo", "bar") != -1) e(25);	/* try linking to bar again */
  if (errno != EEXIST) e(26);
  if (link("foo", "bar") != -1) e(27);	/* try linking to bar again */
  if (errno != EEXIST) e(28);
  if (unlink("nono") != -1) e(29);	/* try rm <not exist> */
  if (errno != ENOENT) e(30);
  if (unlink("") != -1) e(31);	/* try unlinking empty */
  if (errno != ENOENT) e(32);
  if (link("foo", "") != -1) e(33);	/* try linking to "" */
  if (errno != ENOENT) e(34);
  if (link("", "foo") != -1) e(35);	/* try linking "" */
  if (errno != ENOENT) e(36);
  if (link("", "") != -1) e(37);/* try linking "" to "" */
  if (errno != ENOENT) e(38);
  if (link("/foo/bar/foo", "a") != -1) e(39);	/* try no existing path */
  if (errno != ENOENT) e(40);
  if (link("foo", "/foo/bar/foo") != -1) e(41);	/* try no existing path */
  if (errno != ENOENT) e(42);
  if (link("/a/b/c", "/d/e/f") != -1) e(43);	/* try no existing path */
  if (errno != ENOENT) e(44);
  if (link("abc", "a") != -1) e(45);	/* try no existing file */
  if (errno != ENOENT) e(46);
  if (link("foo/bar", "bar") != -1) e(47);	/* foo is a file */
  if (errno != ENOTDIR) e(48);
  if (link("foo", "foo/bar") != -1) e(49);	/* foo is not a dir */
  if (errno != ENOTDIR) e(50);
  if (unlink("foo/bar") != -1) e(51);	/* foo still no dir */
  if (errno != ENOTDIR) e(52);
  if (!superuser) {
	if (link(".", "root") != -1) e(55);
	if (errno != EPERM) e(56);	/* noroot can't */
	if (unlink("root") != -1) e(57);
	if (errno != ENOENT) e(58);
  }
  if (mkdir("dir", 0777) != 0) e(59);
  if (superuser) {
	if (rmdir("dir") != 0) e(63);
  }
  if (!superuser) {
	if (unlink("dir") != -1) e(64);
	if (errno != EPERM) e(65);	/* that ain't w'rkn */
	if (rmdir("dir") != 0) e(66);	/* that's the way to do it */
  }
}

int stateq(stp1, stp2)
struct stat *stp1, *stp2;
{
  if (stp1->st_dev != stp2->st_dev) return 0;
  if (stp1->st_ino != stp2->st_ino) return 0;
  if (stp1->st_mode != stp2->st_mode) return 0;
  if (stp1->st_nlink != stp2->st_nlink) return 0;
  if (stp1->st_uid != stp2->st_uid) return 0;
  if (stp1->st_gid != stp2->st_gid) return 0;
  if (stp1->st_rdev != stp2->st_rdev) return 0;
  if (stp1->st_size != stp2->st_size) return 0;
  if (stp1->st_atime != stp2->st_atime) return 0;
  if (stp1->st_mtime != stp2->st_mtime) return 0;
  if (stp1->st_ctime != stp2->st_ctime) return 0;
  return 1;
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
  ToLongName[NAME_MAX + 1] = '\0';	/* extend ToLongName by one
					 * too many */
  ToLongPath[PATH_MAX - 1] = '/';
  ToLongPath[PATH_MAX] = '\0';	/* inc ToLongPath by one */
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
  chdir("..");
  system("rm -rf DIR_40");

  if (errct == 0) {
	printf("ok\n");
	exit(0);
  } else {
	printf("%d errors\n", errct);
	exit(1);
  }
}
