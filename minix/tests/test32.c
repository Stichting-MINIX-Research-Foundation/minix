/* test32: rename()		Author: Jan-Mark Wams (jms@cs.vu.nl) */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <limits.h>
#include <errno.h>
#include <time.h>
#include <stdio.h>

int max_error = 	4;
#include "common.h"

#define ITERATIONS      2

#define System(cmd)	if (system(cmd) != 0) printf("``%s'' failed\n", cmd)
#define Chdir(dir)	if (chdir(dir) != 0) printf("Can't goto %s\n", dir)
#define Stat(a,b)	if (stat(a,b) != 0) printf("Can't stat %s\n", a)
#define Creat(f)	if (close(creat(f,0777))!=0) printf("Can't creat %s\n",f)


int superuser;
char *MaxName;			/* Name of maximum length */
char MaxPath[PATH_MAX];		/* Same for path */
char *ToLongName;		/* Name of maximum +1 length */
char ToLongPath[PATH_MAX + 1];	/* Same for path, both too long */

void test32a(void);
void test32b(void);
void test32c(void);
void makelongnames(void);

int main(int argc, char *argv[])
{
  int i, m = 0xFFFF;

  start(32);

  if (argc == 2) m = atoi(argv[1]);
  makelongnames();
  superuser = (geteuid() == 0);

  for (i = 0; i < ITERATIONS; i++) {
	if (m & 0001) test32a();
	if (m & 0002) test32b();
	if (m & 0004) test32c();
  }
  quit();

  return(-1);	/* Unreachable */
}

#define BUF_SIZE 1024

void test32a()
{				/* Test normal operation. */
  struct stat st1, st2;
  int fd1, fd2;
  time_t time1, time2, time3;
  char buf[BUF_SIZE];

  subtest = 1;
  System("rm -rf ../DIR_32/*");

  /* Test normal file renamal. */
  System("echo haha > old");
  Stat("old", &st1);
  if (rename("old", "new") != 0) e(1);
  Stat("new", &st2);

  /* The status of new should be the same as old. */
  if (st1.st_dev != st2.st_dev) e(2);
  if (st1.st_ino != st2.st_ino) e(3);
  if (st1.st_mode != st2.st_mode) e(4);
  if (st1.st_nlink != st2.st_nlink) e(5);
  if (st1.st_uid != st2.st_uid) e(6);
  if (st1.st_gid != st2.st_gid) e(7);
  if (st1.st_rdev != st2.st_rdev) e(8);
  if (st1.st_size != st2.st_size) e(9);
  if (st1.st_atime != st2.st_atime) e(10);
  if (st1.st_mtime != st2.st_mtime) e(11);
  if (st1.st_ctime != st2.st_ctime) e(12);

  /* If new exists, it should be removed. */
  System("ln new new2");
  System("echo foobar > old");
  Stat("old", &st1);
  if (rename("old", "new") != 0) e(13);
  Stat("new", &st2);

  /* The status of new should be the same as old. */
  if (st1.st_dev != st2.st_dev) e(14);
  if (st1.st_ino != st2.st_ino) e(15);
  if (st1.st_mode != st2.st_mode) e(16);
  if (st1.st_nlink != st2.st_nlink) e(17);
  if (st1.st_uid != st2.st_uid) e(18);
  if (st1.st_gid != st2.st_gid) e(19);
  if (st1.st_rdev != st2.st_rdev) e(20);
  if (st1.st_size != st2.st_size) e(21);
  if (st1.st_atime != st2.st_atime) e(22);
  if (st1.st_mtime != st2.st_mtime) e(23);
  if (st1.st_ctime != st2.st_ctime) e(24);

  /* The link count on new2 should be one since the old new is removed. */
  Stat("new2", &st1);
  if (st1.st_nlink != 1) e(25);

  /* Check if status for "." is updated. */
  System("> OLD");
  Stat(".", &st1);
  time(&time1);
  while (time1 == time((time_t *)0))
	;
  time(&time2);
  rename("OLD", "NEW");
  Stat(".", &st2);
  time(&time3);
  while (time3 == time((time_t *)0))
	;
  time(&time3);
  if (st1.st_ctime >= st2.st_ctime) e(26);
  if (st1.st_mtime >= st2.st_mtime) e(27);
  if (st1.st_ctime > time1) e(28);
  if (st1.st_mtime > time1) e(29);
  if (st1.st_ctime >= time2) e(30);
  if (st1.st_mtime >= time2) e(31);
  if (st2.st_ctime < time2) e(32);
  if (st2.st_mtime < time2) e(33);
  if (st2.st_ctime >= time3) e(34);
  if (st2.st_mtime >= time3) e(35);

  /* If the new file is removed while it's open it should still be
   * readable. */
  System("rm -rf new NEW old OLD");
  if ((fd1 = creat("new", 0644)) != 3) e(36);
  if (write(fd1, "Hi there! I am Sammy the string", 33) != 33) e(37);
  if (close(fd1) != 0) e(38);
  if ((fd1 = creat("old", 0644)) != 3) e(39);
  if (write(fd1, "I need a new name", 18) != 18) e(40);
  if (close(fd1) != 0) e(41);
  if ((fd1 = open("new", O_RDONLY)) != 3) e(42);
  if ((fd2 = open("new", O_RDONLY)) != 4) e(43);
  if (rename("old", "new") != 0) e(44);
  if (stat("old", &st1) == 0) e(45);
  if (close(fd1) != 0) e(46);
  if ((fd1 = open("new", O_RDONLY)) != 3) e(47);
  if (read(fd2, buf, BUF_SIZE) != 33) e(48);
  if (strcmp(buf, "Hi there! I am Sammy the string") != 0) e(49);
  if (read(fd1, buf, BUF_SIZE) != 18) e(50);
  if (strcmp(buf, "I need a new name") != 0) e(51);
  if (close(fd1) != 0) e(52);
  if (close(fd2) != 0) e(53);
}

void test32b()
{
  char MaxPath2[PATH_MAX];	/* Same for path */
  char MaxName2[NAME_MAX + 1];	/* Name of maximum length */
  int fd, i;
  int stat_loc;
  struct stat st;

  subtest = 2;
  System("rm -rf ../DIR_32/*");

  /* Test maximal file name length. */
  if ((fd = creat(MaxName, 0777)) != 3) e(1);
  if (close(fd) != 0) e(2);
  strcpy(MaxName2, MaxName);
  MaxName2[strlen(MaxName2) - 1] ^= 1;
  if (rename(MaxName, MaxName2) != 0) e(3);
  if (rename(MaxName2, MaxName) != 0) e(4);
  MaxName2[strlen(MaxName2) - 1] ^= 2;
  if (rename(MaxName, MaxName2) != 0) e(5);
  MaxPath[strlen(MaxPath) - 2] = '/';
  MaxPath[strlen(MaxPath) - 1] = 'a';	/* make ././.../a */
  if ((fd = creat(MaxPath, 0777)) != 3) e(6);
  if (close(fd) != 0) e(7);
  strcpy(MaxPath2, MaxPath);
  MaxPath2[strlen(MaxPath2) - 1] ^= 1;
  if (rename(MaxPath, MaxPath2) != 0) e(8);
  if (rename(MaxPath2, MaxPath) != 0) e(9);
  MaxPath2[strlen(MaxPath2) - 1] ^= 2;
  if (rename(MaxPath, MaxPath2) != 0) e(10);
  MaxPath[strlen(MaxPath) - 1] = '/';	/* make ././.../a */

  /* Test if linked files are renamable. */
  System("> foo; ln foo bar");
  if (rename("foo", "bar") != 0) e(11);
  if (rename("bar", "foo") != 0) e(12);
  System("ln foo foobar");
  if (rename("foo", "foobar") != 0) e(13);
  if (rename("bar", "foobar") != 0) e(14);

  /* Since the same files have the same links.... */
  if (rename("bar", "bar") != 0) e(15);
  if (rename("foo", "foo") != 0) e(16);
  if (rename("foobar", "foobar") != 0) e(17);

  /* In ``rename(old, new)'' with new existing, there is always an new
   * entry. */
  for (i = 0; i < 5; i++) {
	System("echo old > old");
	System("echo news > new");
	switch (fork()) {
	    case -1:	printf("Can't fork\n");	break;
	    case 0:
		alarm(20);
		sleep(1);
		rename("old", "new");
		exit(0);
	    default:
		while (stat("old", &st) == 0)
			if (stat("new", &st) != 0) e(18);
		wait(&stat_loc);
		if (stat_loc != 0) e(19);	/* Alarm? */
	}
  }

}

void test32c()
{				/* Test behavior under error contitions. */
  struct stat st1;
  int stat_loc;

  subtest = 3;
  System("rm -rf ../DIR_32/*");

  /* Test if we have access. */
  system("chmod 777 noacc nowrite > /dev/null 2>/dev/null");
  system("rm -rf noacc nowrite");

  System("mkdir noacc nowrite");
  System("> noacc/file");
  System("> nowrite/file");
  System("> file");
  System("chmod 677 noacc");
  System("chmod 577 nowrite");
  if (!superuser) {
	if (rename("noacc/file", "nono") != -1) e(1);
	if (errno != EACCES) e(2);
	if (rename("nowrite/file", "nono") != -1) e(3);
	if (errno != EACCES) e(4);
	if (rename("file", "noacc/file") != -1) e(5);
	if (errno != EACCES) e(6);
	if (rename("file", "nowrite/file") != -1) e(7);
	if (errno != EACCES) e(8);
  }
  if (superuser) {
	/* Super user heeft access. */
	if (rename("noacc/file", "noacc/yes") != 0) e(9);
	if (rename("nowrite/file", "nowrite/yes") != 0) e(10);
	if (rename("file", "yes") != 0) e(11);
	if (rename("noacc/yes", "noacc/file") != 0) e(12);
	if (rename("nowrite/yes", "nowrite/file") != 0) e(13);
	if (rename("yes", "file") != 0) e(14);
  }
  System("chmod 777 noacc nowrite");

  /* If rmdir() doesn't remove a directory, rename() shouldn't eighter. */
  System("mkdir newdir olddir");
  System("rm -rf /tmp/sema.11[ab]");
  switch (fork()) {
      case -1:	printf("Can't fork\n");	break;
      case 0:
	alarm(20);
	switch (fork()) {
	    case -1:	printf("Can't fork\n");	break;
	    case 0:
		/* Child A. */
		alarm(20);
		if (chdir("newdir") != 0) e(15);
		Creat("/tmp/sema.11a");
		while (stat("/tmp/sema.11a", &st1) == 0) sleep(1);
		exit(0);
	    default:
		wait(&stat_loc);
		if (stat_loc != 0) e(16);	/* Alarm? */
	}

	/* Child B. */
	if (chdir("olddir") != 0) e(17);
	Creat("/tmp/sema.11b");
	while (stat("/tmp/sema.11b", &st1) == 0) sleep(1);
	exit(0);
      default:
	/* Wait for child A. It will keep ``newdir'' bussy. */
	while (stat("/tmp/sema.11a", &st1) == -1) sleep(1);
	if (rmdir("newdir") == -1) {
		if (rename("olddir", "newdir") != -1) e(18);
		if (errno != EBUSY) e(19);
	}
	(void) unlink("/tmp/sema.11a");

	/* Wait for child B. It will keep ``olddir'' bussy. */
	while (stat("/tmp/sema.11b", &st1) == -1) sleep(1);
	if (rmdir("olddir") == -1) {
		if (rename("olddir", "newdir") != -1) e(20);
		if (errno != EBUSY) e(21);
	}
	(void) unlink("/tmp/sema.11b");
	wait(&stat_loc);
	if (stat_loc != 0) e(22);	/* Alarm? */
  }
}

void makelongnames()
{
  register int i;
  int max_name_length;

  max_name_length = name_max("."); /* Aka NAME_MAX, but not every FS supports
				    * the same length, hence runtime check */
  MaxName = malloc(max_name_length + 1);
  ToLongName = malloc(max_name_length + 1 + 1); /* Name of maximum +1 length */
  memset(MaxName, 'a', max_name_length);
  MaxName[max_name_length] = '\0';

  for (i = 0; i < PATH_MAX - 1; i++) {	/* idem path */
	MaxPath[i++] = '.';
	MaxPath[i] = '/';
  }
  MaxPath[PATH_MAX - 1] = '\0';

  strcpy(ToLongName, MaxName);	/* copy them Max to ToLong */
  strcpy(ToLongPath, MaxPath);

  ToLongName[max_name_length] = 'a';
  ToLongName[max_name_length+1] = '\0';/* extend ToLongName by one too many */
  ToLongPath[PATH_MAX - 1] = '/';
  ToLongPath[PATH_MAX] = '\0';	/* inc ToLongPath by one */
}
