/* test31: mkfifo()		Author: Jan-Mark Wams (jms@cs.vu.nl) */

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

#define ITERATIONS     10


#define System(cmd)	if (system(cmd) != 0) printf("``%s'' failed\n", cmd)
#define Chdir(dir)	if (chdir(dir) != 0) printf("Can't goto %s\n", dir)
#define Stat(a,b)	if (stat(a,b) != 0) printf("Can't stat %s\n", a)

int superuser;
char *MaxName;			/* Name of maximum length */
char MaxPath[PATH_MAX];		/* Same for path */
char *ToLongName;		/* Name of maximum +1 length */
char ToLongPath[PATH_MAX + 1];	/* Same for path, both too long */

void test31a(void);
void test31b(void);
void test31c(void);
void makelongnames(void);

int main(int argc, char *argv[])
{
  int i, m = 0xFFFF;

  sync();
  if (argc == 2) m = atoi(argv[1]);
  umask(0000);
  start(31);
  makelongnames();
  superuser = (geteuid() == 0);


  for (i = 0; i < ITERATIONS; i++) {
	if (m & 0001) test31a();
	if (m & 0002) test31b();
	if (m & 0004) test31c();
  }
  quit();
  return 1;
}

void test31a()
{				/* Test normal operation. */

#define BUF_SIZE 1024

  int fd;
  char buf[BUF_SIZE];
  struct stat st, dirst;
  time_t time1, time2;
  int stat_loc, cnt;

  subtest = 1;

  System("rm -rf ../DIR_31/*");

  /* Check if the file status information is updated correctly */
  cnt = 0;
  Stat(".", &dirst);
  time(&time1);
  while (time1 == time((time_t *)0))
	;

  do {
  	unlink("fifo");
	time(&time1);
	if (mkfifo("fifo", 0644) != 0) e(1);
	Stat("fifo", &st);
	time(&time2);
  } while (time1 != time2 && cnt++ < 100);

  if (cnt >= 100) e(2);
  if (st.st_uid != geteuid()) e(3);	/* Uid should be set. */
#if defined(NGROUPS_MAX) && NGROUPS_MAX == 0
  if (st.st_gid != getegid()) e(4);
#endif /* defined(NGROUPS_MAX) && NGROUPS_MAX == 0 */
  if (!S_ISFIFO(st.st_mode)) e(5);
  if ((st.st_mode & 0777) != 0644) e(6);
  if (st.st_nlink != 1) e(7);
  if (st.st_ctime != time1) e(8);
  if (st.st_atime != time1) e(9);
  if (st.st_mtime != time1) e(10);
  if (st.st_size != 0) e(11);	/* File should be empty. */

  /* Check if status for "." is updated. */
  Stat(".", &st);
  if (st.st_ctime <= dirst.st_ctime) e(12);
  if (st.st_mtime <= dirst.st_mtime) e(13);

  /* Basic checking if a fifo file created with mkfifo() is a pipe. */
  alarm(10);		/* in case fifo hangs */
  switch (fork()) {
      case -1:	printf("Can't fork\n");	break;
      case 0:
	if ((fd = open("fifo", O_RDONLY)) != 3) e(14);
	if (read(fd, buf, BUF_SIZE) != 7) e(15);
	if (strcmp(buf, "banana") != 0) e(16);
	if (close(fd) != 0) e(17);
	if ((fd = open("fifo", O_WRONLY)) != 3) e(18);
	if (write(fd, "thanks", 7) != 7) e(19);
	if (close(fd) != 0) e(20);
	exit(0);

      default:
	if ((fd = open("fifo", O_WRONLY)) != 3) e(21);
	if (write(fd, "banana", 7) != 7) e(22);
	if (close(fd) != 0) e(23);
	if ((fd = open("fifo", O_RDONLY)) != 3) e(24);
	if (read(fd, buf, BUF_SIZE) != 7) e(25);
	if (strcmp(buf, "thanks") != 0) e(26);
	if (close(fd) != 0) e(27);
	wait(&stat_loc);
	if (stat_loc != 0) e(28);	/* Alarm? */
  }
  alarm(0);
}

void test31b()
{
  subtest = 2;

  System("rm -rf ../DIR_31/*");

  /* Test maximal file name length. */
  if (mkfifo(MaxName, 0777) != 0) e(1);
  if (unlink(MaxName) != 0) e(2);
  MaxPath[strlen(MaxPath) - 2] = '/';
  MaxPath[strlen(MaxPath) - 1] = 'a';	/* make ././.../a */
  if (mkfifo(MaxPath, 0777) != 0) e(3);
  if (unlink(MaxPath) != 0) e(4);
  MaxPath[strlen(MaxPath) - 1] = '/';	/* make ././.../a */
}

void test31c()
{
  int does_truncate;
  subtest = 3;

  System("rm -rf ../DIR_31/*");

  /* Check if mkfifo() removes, files, fifos, dirs. */
  if (mkfifo("fifo", 0777) != 0) e(1);
  System("mkdir dir; > file");
  if (mkfifo("fifo", 0777) != -1) e(2);
  if (errno != EEXIST) e(3);
  if (mkfifo("dir", 0777) != -1) e(4);
  if (errno != EEXIST) e(5);
  if (mkfifo("file", 0777) != -1) e(6);
  if (errno != EEXIST) e(7);

  /* Test empty path. */
  if (mkfifo("", 0777) != -1) e(8);
  if (errno != ENOENT) e(9);
  if (mkfifo("/tmp/noway/out", 0777) != -1) e(10);
  if (errno != ENOENT) e(11);

  /* Test if path prefix is a directory. */
  if (mkfifo("/etc/passwd/nono", 0777) != -1) e(12);
  if (errno != ENOTDIR) e(13);

  mkdir("bar", 0777);		/* make bar */

  /* Check if no access on part of path generates the correct error. */
  System("chmod 577 bar");	/* r-xrwxrwx */
  if (!superuser) {
	if (mkfifo("bar/nono", 0666) != -1) e(14);
	if (errno != EACCES) e(15);
  }
  if (superuser) {
	if (mkfifo("bar/nono", 0666) != 0) e(14);
	if (unlink("bar/nono") != 0) e(666);
  }
  System("chmod 677 bar");	/* rw-rwxrwx */
  if (!superuser) {
	if (mkfifo("bar/../nono", 0666) != -1) e(16);
	if (errno != EACCES) e(17);
  }
  if (unlink("nono") != -1) e(18);

  /* Clean up bar. */
  System("rm -rf bar");

  /* Test ToLongName and ToLongPath */
  does_truncate = does_fs_truncate();
  if (does_truncate) {
	if (mkfifo(ToLongName, 0777) != 0) e(19);
  } else {
	if (mkfifo(ToLongName, 0777) != -1) e(20);
	if (errno != ENAMETOOLONG) e(21);
  }

  ToLongPath[PATH_MAX - 2] = '/';
  ToLongPath[PATH_MAX - 1] = 'a';
  if (mkfifo(ToLongPath, 0777) != -1) e(22);
  if (errno != ENAMETOOLONG) e(23);
  ToLongPath[PATH_MAX - 1] = '/';
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
