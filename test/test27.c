/* test27: stat() fstat()	Author: Jan-Mark Wams (jms@cs.vu.nl) */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include <stdio.h>

#define MODE_MASK	(S_IRWXU | S_IRWXG | S_IRWXO | S_ISUID | S_ISGID)
#define MAX_ERROR	4
#define ITERATIONS      2

#define System(cmd)	if (system(cmd) != 0) printf("``%s'' failed\n", cmd)
#define Chdir(dir)	if (chdir(dir) != 0) printf("Can't goto %s\n", dir)

#include "common.c"

int superuser;
char *MaxName;			/* Name of maximum length */
char MaxPath[PATH_MAX];
char *ToLongName;		/* Name of maximum +1 length */
char ToLongPath[PATH_MAX + 1];

void test27a(void);
void test27b(void);
void test27c(void);
void makelongnames(void);

int main(int argc, char *argv[])
{
  int i, m = 0xFFFF;

  start(27);
  if (argc == 2) m = atoi(argv[1]);
  superuser = (getuid() == 0);
  makelongnames();

  for (i = 0; i < ITERATIONS; i++) {
	if (m & 0001) test27a();
	if (m & 0002) test27b();
	if (m & 0004) test27c();
  }
  quit();

  return(-1);	/* Unreachable */
}

void test27a()
{				/* Test Normal operation. */
  struct stat st1, st2;
  time_t time1, time2;
  int fd, pfd[2];

  subtest = 1;

  time(&time1);			/* get time before */
  while (time1 >= time((time_t *)0))
	;			/* Wait for time to change. */
  System("echo 7bytes > foo; chmod 4750 foo");
  if (stat("foo", &st1) != 0) e(1);	/* get foo's info */
  time(&time2);
  while (time2 >= time((time_t *)0))
	;			/* Wait for next second. */
  time(&time2);			/* get time after */
  if ((st1.st_mode & MODE_MASK) != 04750) e(2);
  if (st1.st_nlink != 1) e(3);	/* check stat */
  if (st1.st_uid != geteuid()) e(4);
#if defined(NGROUPS_MAX) && NGROUPS_MAX == 0
  if (st1.st_gid != getegid()) e(5);
#endif /* defined(NGROUPS_MAX) && NGROUPS_MAX == 0 */
  if (st1.st_size != (size_t) 7) e(6);
  if (st1.st_atime <= time1) e(7);
  if (st1.st_atime >= time2) e(8);
  if (st1.st_ctime <= time1) e(9);
  if (st1.st_ctime >= time2) e(10);
  if (st1.st_mtime <= time1) e(11);
  if (st1.st_mtime >= time2) e(12);

  /* Compair stat and fstat. */
  System("echo 7bytes > bar");
  fd = open("bar", O_RDWR | O_APPEND);	/* the bar is open! */
  if (fd != 3) e(13);		/* should be stderr + 1 */
  if (stat("bar", &st1) != 0) e(14);	/* get bar's info */
  if (fstat(fd, &st2) != 0) e(15);	/* get bar's info */

  /* St1 en st2 should be the same. */
  if (st1.st_dev != st2.st_dev) e(16);
  if (st1.st_ino != st2.st_ino) e(17);
  if (st1.st_mode != st2.st_mode) e(18);
  if (st1.st_nlink != st2.st_nlink) e(19);
  if (st1.st_uid != st2.st_uid) e(20);
  if (st1.st_gid != st2.st_gid) e(21);
  if (st1.st_size != st2.st_size) e(22);
  if (st1.st_atime != st2.st_atime) e(23);
  if (st1.st_ctime != st2.st_ctime) e(24);
  if (st1.st_mtime != st2.st_mtime) e(25);
  time(&time1);			/* wait a sec. */
  while (time1 >= time((time_t *)0))
	;
  System("chmod 755 bar");	/* chainge mode */
  System("rm -f foobar; ln bar foobar");	/* chainge # links */
  if (write(fd, "foo", 4) != 4) e(26);	/* write a bit (or two) */
  if (stat("bar", &st2) != 0) e(27);	/* get new info */
  if (st2.st_dev != st1.st_dev) e(28);
  if (st2.st_ino != st1.st_ino) e(29);	/* compair the fealds */
  if ((st2.st_mode & MODE_MASK) != 0755) e(30);
  if (!S_ISREG(st2.st_mode)) e(31);
  if (st2.st_nlink != st1.st_nlink + 1) e(32);
  if (st2.st_uid != st1.st_uid) e(33);
  if (st2.st_gid != st1.st_gid) e(34);
  if (st2.st_size != (size_t) 11) e(35);
  if (st2.st_atime != st1.st_atime) e(36);
  if (st2.st_ctime <= st1.st_ctime) e(37);
  if (st2.st_mtime <= st1.st_mtime) e(38);
  if (close(fd) != 0) e(39);	/* sorry the bar is closed */

  /* Check special file. */
  if (stat("/dev/tty", &st1) != 0) e(40);
  if (!S_ISCHR(st1.st_mode)) e(41);
#ifdef _MINIX
  if (stat("/dev/ram", &st1) != 0) e(42);
  if (!S_ISBLK(st1.st_mode)) e(43);
#endif

  /* Check fifos. */
  time(&time1);
  while (time1 >= time((time_t *)0))
	;
  if (mkfifo("fifo", 0640) != 0) e(44);
  if (stat("fifo", &st1) != 0) e(45);	/* get fifo's info */
  time(&time2);
  while (time2 >= time((time_t *)0))
	;
  time(&time2);
  if (!S_ISFIFO(st1.st_mode)) e(46);
  if (st1.st_nlink != 1) e(47);	/* check the stat info */
  if (st1.st_uid != geteuid()) e(48);
#if defined(NGROUPS_MAX) && NGROUPS_MAX == 0
  if (st1.st_gid != getegid()) e(49);
#endif /* defined(NGROUPS_MAX) && NGROUPS_MAX == 0 */
  if (st1.st_size != (size_t) 0) e(50);
  if (st1.st_atime <= time1) e(51);
  if (st1.st_atime >= time2) e(52);
  if (st1.st_ctime <= time1) e(53);
  if (st1.st_ctime >= time2) e(54);
  if (st1.st_mtime <= time1) e(55);
  if (st1.st_mtime >= time2) e(56);

  /* Note: the st_mode of a fstat on a pipe should contain a isfifo bit. */
  /* Check pipes. */
  time(&time1);
  while (time1 >= time((time_t *)0))
	;
  if (pipe(pfd) != 0) e(57);
  if (fstat(pfd[0], &st1) != 0) e(58);	/* get pipe input info */
  time(&time2);
  while (time2 >= time((time_t *)0))
	;
  time(&time2);
  if (!(S_ISFIFO(st1.st_mode))) e(59);	/* check stat struct */
  if (st1.st_uid != geteuid()) e(60);
  if (st1.st_gid != getegid()) e(61);
  if (st1.st_size != (size_t) 0) e(62);
  if (st1.st_atime <= time1) e(63);
  if (st1.st_atime >= time2) e(64);
  if (st1.st_ctime <= time1) e(65);
  if (st1.st_ctime >= time2) e(66);
  if (st1.st_mtime <= time1) e(67);
  if (st1.st_mtime >= time2) e(68);
  if (fstat(pfd[1], &st1) != 0) e(69);	/* get pipe output info */
  if (!(S_ISFIFO(st1.st_mode))) e(70);
  if (st1.st_uid != geteuid()) e(71);
  if (st1.st_gid != getegid()) e(72);
  if (st1.st_size != (size_t) 0) e(73);
  if (st1.st_atime < time1) e(74);
  if (st1.st_atime > time2) e(75);
  if (st1.st_ctime < time1) e(76);
  if (st1.st_ctime > time2) e(77);
  if (st1.st_mtime < time1) e(78);
  if (st1.st_mtime > time2) e(79);
  if (close(pfd[0]) != 0) e(80);
  if (close(pfd[1]) != 0) e(81);/* close pipe */

  /* Check dirs. */
  time(&time1);
  while (time1 >= time((time_t *)0))
	;
  System("mkdir dir");
  if (stat("dir", &st1) != 0) e(82);	/* get dir info */
  time(&time2);
  while (time2 >= time((time_t *)0))
	;
  time(&time2);
  if (!(S_ISDIR(st1.st_mode))) e(83);	/* check stat struct */
  if (st1.st_uid != geteuid()) e(84);
#if defined(NGROUPS_MAX) && NGROUPS_MAX == 0
  if (st1.st_gid != getegid()) e(85);
#endif /* defined(NGROUPS_MAX) && NGROUPS_MAX == 0 */
  if (st1.st_atime < time1) e(86);
  if (st1.st_atime > time2) e(87);
  if (st1.st_ctime < time1) e(88);
  if (st1.st_ctime > time2) e(89);
  if (st1.st_mtime < time1) e(90);
  if (st1.st_mtime > time2) e(91);
  System("rm -rf ../DIR_27/*");
}

void test27b()
{				/* Test maxima. */
  struct stat st;
  int fd;

  subtest = 2;

  /* Check stats on maximum length files names. */
  if (mkdir(MaxName, 0777) != 0) e(1);
  if (stat(MaxName, &st) != 0) e(2);
  if ((fd = open(MaxName, O_RDONLY)) != 3) e(3);
  if (fstat(fd, &st) != 0) e(4);
  if (close(fd) != 0) e(5);
  if (rmdir(MaxName) != 0) e(6);
  if (stat(MaxPath, &st) != 0) e(7);
  if ((fd = open(MaxPath, O_RDONLY)) != 3) e(8);
  if (fstat(fd, &st) != 0) e(9);
  if (close(fd) != 0) e(10);
  System("rm -rf ../DIR_27/*");
}

void test27c()
{				/* Test error response. */
  struct stat st;
  int fd, i;

  subtest = 3;

  System("echo Hi > foo");	/* Make a file called foo. */
  /* Check if a un searchable dir is handled ok. */
  Chdir("..");			/* cd .. */
  System("chmod 677 DIR_27");	/* no search permission */
  if (stat("DIR_27/nono", &st) != -1) e(1);
  if (superuser) {
	if (errno != ENOENT) e(2);	/* su has access */
  }
  if (!superuser) {
	if (errno != EACCES) e(3);	/* we don't ;-) */
  }
  System("chmod 777 DIR_27");
  Chdir("DIR_27");		/* back to test dir */

  /* Check on ToLongName etc. */
  if (stat(ToLongPath, &st) != -1) e(6);	/* path is too long */
  if (errno != ENAMETOOLONG) e(7);

  /* Test some common errors. */
  if (stat("nono", &st) != -1) e(8);	/* nono nonexistent */
  if (errno != ENOENT) e(9);
  if (stat("", &st) != -1) e(10);	/* try empty */
  if (errno != ENOENT) e(11);
  if (stat("foo/bar", &st) != -1) e(12);	/* foo is a file */
  if (errno != ENOTDIR) e(13);

  /* Test fstat on file descriptors that are not open. */
  for (i = 3; i < 6; i++) {
	if (fstat(i, &st) != -1) e(14);
	if (errno != EBADF) e(15);
  }

  /* Test if a just closed file is `fstat()'-able. */
  if ((fd = open("foo", O_RDONLY)) != 3) e(16);	/* open foo */
  if (fstat(fd, &st) != 0) e(17);	/* get stat */
  if (close(fd) != 0) e(18);	/* close it */
  if (fstat(fd, &st) != -1) e(19);	/* get stat */
  if (errno != EBADF) e(20);
  System("rm -rf ../DIR_27/*");
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

