/* test30: creat() 		(p) Jan-Mark Wams. email: jms@cs.vu.nl */

/*
** Creat() should be equivalent to:
**	open(path, O_WRONLY | O_CREAT | O_TRUNC, mode);
** Since we can not look in the source, we can not assume creat() is
** a mere sysnonym (= a systemcall synonym).
*/

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

#define MAX_ERROR	4
#define ITERATIONS     10

#include "common.c"

#define System(cmd)	if (system(cmd) != 0) printf("``%s'' failed\n", cmd)
#define Chdir(dir)	if (chdir(dir) != 0) printf("Can't goto %s\n", dir)
#define Stat(a,b)	if (stat(a,b) != 0) printf("Can't stat %s\n", a)

int superuser;
char *MaxName;			/* Name of maximum length */
char MaxPath[PATH_MAX];		/* Same for path */
char *ToLongName;		/* Name of maximum +1 length */
char ToLongPath[PATH_MAX + 1];	/* Same for path, both too long */

void test30a(void);
void test30b(void);
void test30c(void);
void makelongnames(void);

int main(int argc, char *argv[])
{
  int i, m = 0xFFFF;

  sync();
  if (argc == 2) m = atoi(argv[1]);
  umask(0000);
  start(30);
  makelongnames();
  superuser = (geteuid() == 0);


  for (i = 0; i < ITERATIONS; i++) {
	if (m & 0001) test30a();
	if (m & 0002) test30b();
	if (m & 0004) test30c();
  }
  quit();

  return(-1);	/* Unreachable */
}

void test30a()
{				/* Test normal operation. */

#define BUF_SIZE 1024

  int fd1, fd2;
  char buf[BUF_SIZE];
  struct stat st, dirst;
  time_t time1, time2;
  int stat_loc, cnt;

  subtest = 1;

  System("rm -rf ../DIR_30/*");

  /* Check if processes have independent new fds */
  switch (fork()) {
      case -1:	printf("Can't fork\n");	break;
      case 0:
	alarm(20);
	if ((fd1 = creat("myfile", 0644)) != 3) e(1);
	if (close(fd1) != 0) e(2);
	exit(0);
      default:
	if ((fd1 = creat("myfile", 0644)) != 3) e(3);
	if (close(fd1) != 0) e(4);
	if (wait(&stat_loc) == -1) e(5);
	if (stat_loc != 0) e(6);
  }

  /* Save the dir status. */
  Stat(".", &dirst);
  time(&time1);
  while (time1 == time((time_t *)0))
	;

  /* Check if the file status information is updated correctly */
  cnt = 0;
  System("rm -rf myfile");
  do {
	time(&time1);
	if ((fd1 = creat("myfile", 0644)) != 3) e(7);
	Stat("myfile", &st);
	time(&time2);
  } while (time1 != time2 && cnt++ < 100);
  if (cnt >= 100) e(8);
  if (st.st_uid != geteuid()) e(9);	/* Uid should be set. */
#if defined(NGROUPS_MAX) && NGROUPS_MAX == 0
  if (st.st_gid != getegid()) e(10);
#endif /* defined(NGROUPS_MAX) && NGROUPS_MAX == 0 */
  if (!S_ISREG(st.st_mode)) e(11);
  if ((st.st_mode & 0777) != 0644) e(12);
  if (st.st_nlink != 1) e(13);
  if (st.st_ctime != time1) e(14);	/* All time fields should be updated */
  if (st.st_atime != time1) e(15);
  if (st.st_mtime != time1) e(16);
  if (st.st_size != 0) e(17);	/* File should be trunked. */

  /* Check if c and mtime for "." is updated. */
  Stat(".", &st);
  if (st.st_ctime <= dirst.st_ctime) e(18);
  if (st.st_mtime <= dirst.st_mtime) e(19);

  /* Let's see if cread fds are write only. */
  if (read(fd1, buf, 7) != -1) e(20);	/* we can't read */
  if (errno != EBADF) e(21);	/* a write only fd */
  if (write(fd1, "HELLO", 6) != 6) e(22);	/* but we can write */

  /* No O_APPEND flag should have been used. */
  if (lseek(fd1, (off_t) 1, SEEK_SET) != 1) e(23);
  if (write(fd1, "ello", 5) != 5) e(24);
  Stat("myfile", &st);
  if (st.st_size != 6) e(25);
  if (st.st_size == 11) e(26);	/* O_APPEND should make it 11. */
  if (close(fd1) != 0) e(27);

  /* A creat should set the file offset to 0. */
  if ((fd1 = creat("myfile", 0644)) != 3) e(28);
  if (lseek(fd1, (off_t) 0, SEEK_CUR) != 0) e(29);
  if (close(fd1) != 0) e(30);

  /* Test if file permission bits and the file ownership are unchanged. */
  /* So we will see if creat() is just an open() if the file exists. */
  if (superuser) {
	System("echo > bar; chmod 073 bar");	/* Make bar 073 */
	if (chown("bar", 1, 1) != 0) e(1137);
	fd1 = creat("bar", 0777);	/* knock knock */
	if (fd1 == -1) e(31);
	Stat("bar", &st);
	if (st.st_size != (size_t) 0) e(32);	/* empty file. */
	if (write(fd1, "foo", 3) != 3) e(33);	/* rewrite bar */
	if (close(fd1) != 0) e(34);
	Stat("bar", &st);
	if (st.st_uid != 1) e(35);
	if (st.st_gid != 1) e(36);
	if ((st.st_mode & 0777) != 073) e(37);	/* mode still is 077 */
	if (st.st_size != (size_t) 3) e(38);
  }

  /* Fifo's should be openable with creat(). */
  if (mkfifo("fifo", 0644) != 0) e(39);

  /* Creat() should have no effect on FIFO files. */
  switch (fork()) {
      case -1:	printf("Can't fork\n");	break;
      case 0:
	alarm(20);		/* Give child 20 seconds to live. */
	if ((fd1 = open("fifo", O_WRONLY)) != 3) e(40);
	if (write(fd1, "I did see Elvis.\n", 18) != 18) e(41);
	if ((fd2 = creat("fifo", 0644)) != 4) e(42);
	if (write(fd2, "I DID.\n", 8) != 8) e(43);
	if (close(fd2) != 0) e(44);
	if (close(fd1) != 0) e(45);
	exit(0);
      default:
	if ((fd1 = open("fifo", O_RDONLY)) != 3) e(46);
	if (read(fd1, buf, 18) != 18) e(47);
	if (strcmp(buf, "I did see Elvis.\n") != 0) e(48);
	if (strcmp(buf, "I DID.\n") == 0) e(49);
	if (read(fd1, buf, BUF_SIZE) != 8) e(50);
	if (strcmp(buf, "I DID.\n") != 0) e(51);
	if (close(fd1) != 0) e(52);
	if (wait(&stat_loc) == -1) e(53);
	if (stat_loc != 0) e(54);	/* The alarm went off? */
  }

  /* Creat() should have no effect on directroys. */
  System("mkdir dir; touch dir/f1 dir/f2 dir/f3");
  if ((fd1 = creat("dir", 0644)) != -1) e(55);
  if (errno != EISDIR) e(56);
  close(fd1);

  /* The path should contain only dirs in the prefix. */
  if ((fd1 = creat("dir/f1/nono", 0644)) != -1) e(57);
  if (errno != ENOTDIR) e(58);
  close(fd1);

  /* The path should contain only dirs in the prefix. */
  if ((fd1 = creat("", 0644)) != -1) e(59);
  if (errno != ENOENT) e(60);
  close(fd1);
  if ((fd1 = creat("dir/noso/nono", 0644)) != -1) e(61);
  if (errno != ENOENT) e(62);
  close(fd1);

}

void test30b()
{
  int fd;

  subtest = 2;

  System("rm -rf ../DIR_30/*");

  /* Test maximal file name length. */
  if ((fd = creat(MaxName, 0777)) != 3) e(1);
  if (close(fd) != 0) e(2);
  MaxPath[strlen(MaxPath) - 2] = '/';
  MaxPath[strlen(MaxPath) - 1] = 'a';	/* make ././.../a */
  if ((fd = creat(MaxPath, 0777)) != 3) e(3);
  if (close(fd) != 0) e(4);
  MaxPath[strlen(MaxPath) - 1] = '/';	/* make ././.../a */
}

void test30c()
{
  int fd, does_truncate;

  subtest = 3;

  System("rm -rf ../DIR_30/*");

  if (!superuser) {
	/* Test if creat is not usable to open files with the wrong mode */
	System("> nono; chmod 177 nono");
	fd = creat("nono", 0777);
	if (fd != -1) e(1);
	if (errno != EACCES) e(2);
  }
  if (mkdir("bar", 0777) != 0) e(3);	/* make bar */

  /* Check if no access on part of path generates the correct error. */
  System("chmod 577 bar");	/* r-xrwxrwx */
  if (!superuser) {
	/* Normal users can't creat without write permision. */
	if (creat("bar/nono", 0666) != -1) e(4);
	if (errno != EACCES) e(5);
	if (creat("bar/../nono", 0666) != -1) e(6);
	if (errno != EACCES) e(7);
  }
  if (superuser) {
	/* Super user can still creat stuff. */
	if ((fd = creat("bar/nono", 0666)) != 3) e(8);
	if (close(fd) != 0) e(9);
	if (unlink("bar/nono") != 0) e(10);
  }

  /* Clean up bar. */
  System("rm -rf bar");

  /* Test ToLongName and ToLongPath */
  does_truncate = does_fs_truncate();
  fd = creat(ToLongName, 0777);
  if (does_truncate) {
	if (fd == -1) e(11);
	if (close(fd) != 0) e(12);
  } else {
	if (fd != -1) e(13);
	if (errno != ENAMETOOLONG) e(14);
	(void) close(fd);			/* Just in case. */
  }

  ToLongPath[PATH_MAX - 2] = '/';
  ToLongPath[PATH_MAX - 1] = 'a';
  if ((fd = creat(ToLongPath, 0777)) != -1) e(15);
  if (errno != ENAMETOOLONG) e(16);
  if (close(fd) != -1) e(17);
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
