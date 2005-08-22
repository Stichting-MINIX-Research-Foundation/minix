/* Test24: opendir, readdir, rewinddir, closedir       Author: Jan-Mark Wams */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <time.h>
#include <stdio.h>

_PROTOTYPE(void main, (int argc, char *argv[]));
_PROTOTYPE(void chk_dir, (DIR * dirpntr));
_PROTOTYPE(void test24a, (void));
_PROTOTYPE(void test24b, (void));
_PROTOTYPE(void test24c, (void));
_PROTOTYPE(void makelongnames, (void));
_PROTOTYPE(void e, (int number));
_PROTOTYPE(void quit, (void));

#define OVERFLOW_DIR_NR	(OPEN_MAX + 1)
#define MAX_ERROR	4
#define ITERATIONS 5

#define DIRENT0	((struct dirent *) NULL)
#define System(cmd)	if (system(cmd) != 0) printf("``%s'' failed\n", cmd)
#define Chdir(dir)	if (chdir(dir) != 0) printf("Can't goto %s\n", dir)

int errct = 0;
int subtest = 1;
int superuser;

char MaxName[NAME_MAX + 1];	/* Name of maximum length */
char MaxPath[PATH_MAX];		/* Same for path */
char ToLongName[NAME_MAX + 2];	/* Name of maximum +1 length */
char ToLongPath[PATH_MAX + 1];	/* Same for path, both too long */

void main(argc, argv)
int argc;
char *argv[];
{
  int i, m = 0xFFFF;

  sync();
  if (argc == 2) m = atoi(argv[1]);
  printf("Test 24 ");
  fflush(stdout);
  System("rm -rf DIR_24; mkdir DIR_24");
  Chdir("DIR_24");
  makelongnames();
  superuser = (geteuid() == 0);

  for (i = 0; i < ITERATIONS; i++) {
	if (m & 0001) test24a();
	if (m & 0002) test24b();
	if (m & 0004) test24c();
  }
  quit();
}

void test24a()
{				/* Test normal operations. */
  int fd3, fd4, fd5;
  DIR *dirp;
  int j, ret, fd, flags;
  struct stat st1, st2;
  int stat_loc;
  time_t time1;

  subtest = 1;

  System("rm -rf ../DIR_24/*");

  if ((fd = dup(0)) != 3) e(1);	/* dup stdin */
  close(fd);			/* free the fd again */
  dirp = opendir("/");		/* open "/" */
  if (dirp == ((DIR *) NULL)) e(2);	/* has to succseed */
  if ((fd = dup(0)) <= 2) e(3);	/* dup stdin */
  if (fd > 3) {			/* if opendir() uses fd 3 */
	flags = fcntl(3, F_GETFD);	/* get fd fags of 3 */
	if (!(flags & FD_CLOEXEC)) e(4);	/* it should be closed on */
  }				/* exec..() calls */
  close(fd);			/* free the fd again */
  ret = closedir(dirp);		/* close, we don't need it */
  if (ret == -1) e(5);		/* closedir () unsucces full */
  if (ret != 0) e(6);		/* should be 0 or -1 */
  if ((fd = dup(0)) != 3) e(7);	/* see if next fd is same */
  close(fd);			/* free the fd again */

  System("rm -rf foo; mkdir foo");
  Chdir("foo");
  System("touch f1 f2 f3 f4 f5");	/* make f1 .. f5 */
  System("rm f[24]");		/* creat `holes' in entrys */
  Chdir("..");

  if ((dirp = opendir("foo")) == ((DIR *) NULL)) e(8);	/* open foo */
  chk_dir(dirp);		/* test if foo's ok */
  for (j = 0; j < 10; j++) {
	errno = j * 47 % 7;	/* there should */
	if (readdir(dirp) != DIRENT0) e(9);	/* be nomore dir */
	if (errno != j * 47 % 7) e(10);	/* entrys */
  }
  rewinddir(dirp);		/* rewind foo */
  chk_dir(dirp);		/* test foosok */
  for (j = 0; j < 10; j++) {
	errno = j * 23 % 7;	/* there should */
	if (readdir(dirp) != DIRENT0) e(11);	/* be nomore dir */
	if (errno != j * 23 % 7) e(12);	/* entrys */
  }
  if ((fd4 = creat("foo/f4", 0666)) <= 2) e(13);	/* Open a file. */
  System("rm foo/f4");		/* Kill entry. */
  rewinddir(dirp);		/* Rewind foo. */
  if ((fd3 = open("foo/f3", O_WRONLY)) <= 2) e(14);	/* Open more files. */
  if ((fd5 = open("foo/f5", O_WRONLY)) <= 2) e(15);
  if (write(fd3, "Hello", 6) != 6) e(16);
  if (write(fd4, "Hello", 6) != 6) e(17);	/* write some data */
  if (close(fd5) != 0) e(18);
  chk_dir(dirp);
  for (j = 0; j < 10; j++) {
	errno = j * 101 % 7;	/* there should */
	if (readdir(dirp) != DIRENT0) e(19);	/* be nomore dir */
	if (errno != j * 101 % 7) e(20);	/* entrys */
  }
  if (close(fd4) != 0) e(21);	/* shouldn't matter */
  if (close(fd3) != 0) e(22);	/* when we do this */
  if (closedir(dirp) != 0) e(23);	/* close foo again */

  Chdir("foo");
  if ((dirp = opendir(".//")) == ((DIR *) NULL)) e(24);	/* open foo again */
  Chdir("..");
  chk_dir(dirp);		/* foosok? */
  for (j = 0; j < 10; j++) {
	errno = (j * 101) % 7;	/* there should */
	if (readdir(dirp) != DIRENT0) e(25);	/* be nomore dir */
	if (errno != (j * 101) % 7) e(26);	/* entrys */
  }

  if (closedir(dirp) != 0) e(27);	/* It should be closable */

  stat("foo", &st1);		/* get stat */
  time(&time1);
  while (time1 >= time((time_t *)0))
	;
  if ((dirp = opendir("foo")) == ((DIR *) NULL)) e(28);	/* open, */
  if (readdir(dirp) == DIRENT0) e(29);	/* read and */
  stat("foo", &st2);		/* get new stat */
  if (st1.st_atime > st2.st_atime) e(30);	/* st_atime check */

  switch (fork()) {
      case -1:	printf("Can't fork\n");	break;
      case 0:
	rewinddir(dirp);	/* rewind childs dirp */
	if (readdir(dirp) == DIRENT0) e(31);	/* read should be ok */
	if (closedir(dirp) != 0) e(32);	/* close child'd foo */
	exit(0);		/* 0 stops here */
      default:
	if (wait(&stat_loc) == -1) e(33);	/* PARENT wait()'s */
	break;
  }
  if (closedir(dirp) != 0) e(34);	/* close parent's foo */
}

void test24b()
{
/* See what happens with too many dir's open.  Check if file size seems ok, 
 * and independency.
 */

  int i, j;			/* i = highest open dir count */
  DIR *dirp[OVERFLOW_DIR_NR], *dp;
  struct dirent *dep, *dep1, *dep2;
  char name[NAME_MAX + 2];	/* buffer for file name, and count */
  int dot = 0, dotdot = 0;

  subtest = 2;

  System("rm -rf ../DIR_24/*");

  for (i = 0; i < OVERFLOW_DIR_NR; i++) {
	dirp[i] = opendir("/");
	if (dirp[i] == ((DIR *) NULL)) {
		if (errno != EMFILE) e(1);
		break;
	}
  }
  if (i <= 4) e(2);		/* sounds resanable */
  if (i >= OVERFLOW_DIR_NR) e(3);	/* might be to small */
  for (j = 0; j < i; j++) {
	if (closedir(dirp[(j + 5) % i]) != 0) e(4);	/* neat! */
  }

  /* Now check if number of bytes in d_name can go up till NAME_MAX */
  System("rm -rf foo; mkdir foo");
  Chdir("foo");
  name[0] = 0;
  for (i = 0; i <= NAME_MAX; i++) {
	if (strcat(name, "X") != name) e(5);
	close(creat(name, 0666));	/* fails once on */
  }				/* XX..XX, 1 too long */
  Chdir("..");
  /* Now change i-th X to Y in name buffer record file of length i. */
  if ((dp = opendir("foo")) == ((DIR *) NULL)) e(6);
  while ((dep = readdir(dp)) != DIRENT0) {
	if (strcmp("..", dep->d_name) == 0)
		dotdot++;
	else if (strcmp(".", dep->d_name) == 0)
		dot++;
	else
		name[strlen(dep->d_name)] += 1;	/* 'X' + 1 == 'Y' */
  }
  if (closedir(dp) != 0) e(7);
  for (i = 1; i <= NAME_MAX; i++) {	/* Check if every length */
	if (name[i] != 'Y') e(8);	/* has been seen once. */
  }

  /* Check upper and lower bound. */
  if (name[0] != 'X') e(9);
  if (name[NAME_MAX + 1] != '\0') e(10);

  /* Now check if two simultaniouse open dirs do the same */
  if ((dirp[1] = opendir("foo")) == ((DIR *) NULL)) e(11);
  if ((dirp[2] = opendir("foo")) == ((DIR *) NULL)) e(12);
  if ((dep1 = readdir(dirp[1])) == DIRENT0) e(13);
  if ((dep2 = readdir(dirp[2])) == DIRENT0) e(14);
  if (dep1->d_name == dep2->d_name) e(15);	/* 1 & 2 Should be */
  strcpy(name, dep2->d_name);	/* differand buffers */
  if (strcmp(dep1->d_name, name) != 0) e(16);	/* But hold the same */
  if ((dep1 = readdir(dirp[1])) == DIRENT0) e(17);
  if ((dep1 = readdir(dirp[1])) == DIRENT0) e(18);	/* lose some entries */
  if ((dep1 = readdir(dirp[1])) == DIRENT0) e(19);	/* Using dirp 1 has */
  if (dep1->d_name == dep2->d_name) e(20);	/* no effect on 2 */
  if (strcmp(dep2->d_name, name) != 0) e(21);
  rewinddir(dirp[1]);		/* Rewinding dirp 1 */
  if ((dep2 = readdir(dirp[2])) == DIRENT0) e(22);	/* can't effect 2 */
  if (strcmp(dep2->d_name, name) == 0) e(23);	/* Must be next */
  if (closedir(dirp[1]) != 0) e(24);	/* Closing dirp 1 */
  if ((dep2 = readdir(dirp[2])) == DIRENT0) e(25);	/* can't effect 2 */
  if (strcmp(dep2->d_name, name) == 0) e(26);	/* Must be next */
  if (closedir(dirp[2]) != 0) e(27);
}

void test24c()
{
/* Test whether wrong things go wrong right. */

  DIR *dirp;

  subtest = 3;

  System("rm -rf ../DIR_24/*");

  if (opendir("foo/bar/nono") != ((DIR *) NULL)) e(1);	/* nonexistent */
  if (errno != ENOENT) e(2);
  System("mkdir foo; chmod 677 foo");	/* foo inaccesable */
  if (opendir("foo/bar/nono") != ((DIR *) NULL)) e(3);
  if (superuser) {
	if (errno != ENOENT) e(4);	/* su has access */
	System("chmod 377 foo");
	if ((dirp = opendir("foo")) == ((DIR *) NULL)) e(5);
	if (closedir(dirp) != 0) e(6);
  }
  if (!superuser) {
	if (errno != EACCES) e(7);	/* we don't ;-) */
	System("chmod 377 foo");
	if (opendir("foo") != ((DIR *) NULL)) e(8);
  }
  System("chmod 777 foo");

  if (mkdir(MaxName, 0777) != 0) e(9);	/* make longdir */
  if ((dirp = opendir(MaxName)) == ((DIR *) NULL)) e(10);	/* open it */
  if (closedir(dirp) != 0) e(11);	/* close it */
  if (rmdir(MaxName) != 0) e(12);	/* then remove it */
  if ((dirp = opendir(MaxPath)) == ((DIR *) NULL)) e(13);	/* open '.'  */
  if (closedir(dirp) != 0) e(14);	/* close it */
#if 0 /* XXX - anything could happen with the bad pointer */
  if (closedir(dirp) != -1) e(15);	/* close it again */
  if (closedir(dirp) != -1) e(16);	/* and again */
#endif /* 0 */
  if (opendir(ToLongName) != ((DIR *) NULL)) e(17);	/* is too long */
#ifdef _POSIX_NO_TRUNC
# if _POSIX_NO_TRUNC - 0 != -1
  if (errno != ENAMETOOLONG) e(18);
# else
  if (errno != ENOENT) e(19);
# endif
#else
# include "error, this case requires dynamic checks and is not handled"
#endif
  if (opendir(ToLongPath) != ((DIR *) NULL)) e(20);	/* path is too long */
  if (errno != ENAMETOOLONG) e(21);
  System("touch foo/abc");	/* make a file */
  if (opendir("foo/abc") != ((DIR *) NULL)) e(22);	/* not a dir */
  if (errno != ENOTDIR) e(23);
}

void chk_dir(dirp)		/* dir should contain             */
DIR *dirp;			/* (`f1', `f3', `f5', `.', `..')  */
{				/* no more, no less               */
  int f1 = 0, f2 = 0, f3 = 0, f4 = 0, f5 = 0,	/* counters for all */
   other = 0, dot = 0, dotdot = 0;	/* possible entrys */
  int i;
  struct dirent *dep;
  char *fname;
  int oldsubtest = subtest;

  subtest = 4;

  for (i = 0; i < 5; i++) {	/* 3 files and `.' and `..' == 5 entrys */
	dep = readdir(dirp);
	if (dep == DIRENT0) {	/* not einough */
		if (dep == DIRENT0) e(1);
		break;
	}
	fname = dep->d_name;
	if (strcmp(fname, ".") == 0)
		dot++;
	else if (strcmp(fname, "..") == 0)
		dotdot++;
	else if (strcmp(fname, "f1") == 0)
		f1++;
	else if (strcmp(fname, "f2") == 0)
		f2++;
	else if (strcmp(fname, "f3") == 0)
		f3++;
	else if (strcmp(fname, "f4") == 0)
		f4++;
	else if (strcmp(fname, "f5") == 0)
		f5++;
	else
		other++;
  }				/* do next dir entry */

  if (dot != 1) e(2);		/* Check the entrys */
  if (dotdot != 1) e(3);
  if (f1 != 1) e(4);
  if (f3 != 1) e(5);
  if (f5 != 1) e(6);
  if (f2 != 0) e(7);
  if (f4 != 0) e(8);
  if (other != 0) e(9);

  subtest = oldsubtest;
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
  System("rm -rf DIR_24");

  if (errct == 0) {
	printf("ok\n");
	exit(0);
  } else {
	printf("%d errors\n", errct);
	exit(1);
  }
}
