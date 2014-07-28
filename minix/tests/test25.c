/* test25: open (), close ()	(p) Jan-Mark Wams. email: jms@cs.vu.nl */

/* Not tested: O_NONBLOCK on special files, supporting it.
** On a read-only file system, some error reports are to be expected.
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

int max_error = 	4;
#include "common.h"

#define ITERATIONS	2


#define System(cmd)	if (system(cmd) != 0) printf("``%s'' failed\n", cmd)
#define Chdir(dir)	if (chdir(dir) != 0) printf("Can't goto %s\n", dir)
#define Stat(a,b)	if (stat(a,b) != 0) printf("Can't stat %s\n", a)
#define Creat(f)	if (close(creat(f,0777))!=0) printf("Can't creat %s\n",f)
#define Report(s,n)	printf("Subtest %d" s,subtest,(n))

int subtest = 1;
int superuser;
char *MaxName;			/* Name of maximum length */
char MaxPath[PATH_MAX];		/* Same for path */
char *ToLongName;		/* Name of maximum +1 length */
char ToLongPath[PATH_MAX + 1];	/* Same for path, both too long */

void test25a(void);
void test25b(void);
void test25c(void);
void test25d(void);
void test25e(void);
void makelongnames(void);

int main(int argc, char *argv[])
{
  int i, m = 0xFFFF;

  sync();

  start(25);
  if (argc == 2) m = atoi(argv[1]);
  makelongnames();
  superuser = (geteuid() == 0);

  /* Close all files, the parent might have opened. */
  for (i = 3; i < 100; i++) close(i);

  for (i = 0; i < ITERATIONS; i++) {
	if (m & 001) test25a();
	if (m & 002) test25b();
	if (m & 004) test25c();
	if (m & 010) test25d();
	if (m & 020) test25e();
  }
  quit();

  return(-1);	/* Unreachable */
}

void test25a()
{				/* Test fcntl flags. */
  subtest = 1;

#define EXCLUDE(a,b)	(((a)^(b)) == ((a)|(b)))
#define ADDIT		(O_APPEND | O_CREAT | O_EXCL | O_NONBLOCK | O_TRUNC)

  /* If this compiles all flags are defined but they have to be or-able. */
  if (!(EXCLUDE(O_NONBLOCK, O_TRUNC))) e(1);
  if (!(EXCLUDE(O_EXCL, O_NONBLOCK | O_TRUNC))) e(2);
  if (!(EXCLUDE(O_CREAT, O_EXCL | O_NONBLOCK | O_TRUNC))) e(3);
  if (!(EXCLUDE(O_APPEND, O_CREAT | O_EXCL | O_NONBLOCK | O_TRUNC))) e(4);
  if (!(EXCLUDE(O_RDONLY, ADDIT))) e(5);
  if (!(EXCLUDE(O_WRONLY, ADDIT))) e(6);
  if (!(EXCLUDE(O_RDWR, ADDIT))) e(7);
}

void test25b()
{				/* Test normal operation. */

#define BUF_SIZE 1024

  int fd1, fd2, fd3, fd4, fd5;
  char buf[BUF_SIZE];
  struct stat st1, st2, st3;
  time_t time1, time2;
  int stat_loc;

  subtest = 2;

  System("rm -rf ../DIR_25/*");

  System("echo Hello > he");	/* make test files */
  System("echo Hello > ha");	/* size 6 bytes */
  System("echo Hello > hi");
  System("echo Hello > ho");

  /* Check path resolution. Check if lowest fds are returned */
  if ((fd1 = open("he", O_RDONLY)) != 3) e(1);
  if (read(fd1, buf, BUF_SIZE) != 6) e(2);
  if ((fd2 = open("./ha", O_RDONLY)) != 4) e(3);
  if ((fd3 = open("../DIR_25/he", O_RDWR)) != 5) e(4);
  if ((fd4 = open("ho", O_WRONLY)) != 6) e(5);
  if (close(fd4) != 0) e(6);
  if (close(fd1) != 0) e(7);
  if ((fd1 = open("./././ho", O_RDWR)) != 3) e(8);
  if ((fd4 = open("../DIR_25/he", O_RDONLY)) != 6) e(9);
  if (close(fd2) != 0) e(10);
  if (close(fd3) != 0) e(11);
  if ((fd2 = open("ha", O_RDONLY)) != 4) e(12);
  if ((fd3 = open("/etc/passwd", O_RDONLY)) != 5) e(13);
  if (close(fd4) != 0) e(14);	/* close all */
  if (close(fd1) != 0) e(15);
  if (close(fd3) != 0) e(16);

  /* Check if processes share fd2, and if they have independent new fds */
  System("rm -rf /tmp/sema.25");
  switch (fork()) {
      case -1:	printf("Can't fork\n");	break;

      case 0:
	if ((fd1 = open("he", O_WRONLY)) != 3) e(17);
	if ((fd3 = open("../././DIR_25/ha", O_WRONLY)) != 5) e(18);
	if ((fd4 = open("../DIR_25/hi", O_WRONLY)) != 6) e(19);
	if ((fd5 = open("ho", O_WRONLY)) != 7) e(20);
	system("while test ! -f /tmp/sema.25; do sleep 1; done");  /* parent */
	if (read(fd2, buf, BUF_SIZE) != 3) e(21);	/* gets Hel */
	if (strncmp(buf, "lo\n", 3) != 0) e(22);	/* we get lo */
	if (close(fd1) != 0) e(23);
	if (close(fd2) != 0) e(24);
	if (close(fd3) != 0) e(25);
	if (close(fd4) != 0) e(26);
	if (close(fd5) != 0) e(27);
	exit(0);

      default:
	if ((fd1 = open("ha", O_RDONLY)) != 3) e(28);
	if ((fd3 = open("./he", O_RDONLY)) != 5) e(29);
	if ((fd4 = open("../DIR_25/hi", O_RDWR)) != 6) e(30);
	if ((fd5 = open("ho", O_WRONLY)) != 7) e(31);
	if (close(fd1) != 0) e(32);
	if (read(fd2, buf, 3) != 3) e(33);	/* get Hel */
	Creat("/tmp/sema.25");
	if (strncmp(buf, "Hel", 3) != 0) e(34);
	if (close(fd2) != 0) e(35);
	if (close(fd3) != 0) e(36);
	if (close(fd4) != 0) e(37);
	if (close(fd5) != 0) e(38);
	if (wait(&stat_loc) == -1) e(39);
	if (stat_loc != 0) e(40);
  }
  System("rm -f /tmp/sema.25");

  /* Check if the file status information is updated correctly */
  Stat("hi", &st1);		/* get info */
  Stat("ha", &st2);		/* of files */
  time(&time1);
  while (time1 >= time((time_t *)0))
	;			/* wait a sec */
  if ((fd1 = open("hi", O_RDONLY)) != 3) e(41);	/* open files */
  if ((fd2 = open("ha", O_WRONLY)) != 4) e(42);
  if (read(fd1, buf, 1) != 1) e(43);	/* read one */
  if (close(fd1) != 0) e(44);	/* close one */
  Stat("hi", &st3);		/* get info */
  if (st1.st_uid != st3.st_uid) e(45);
  if (st1.st_gid != st3.st_gid) e(46);	/* should be same */
  if (st1.st_mode != st3.st_mode) e(47);
  if (st1.st_size != st3.st_size) e(48);
  if (st1.st_nlink != st3.st_nlink) e(49);
  if (st1.st_mtime != st3.st_mtime) e(50);
  if (st1.st_ctime != st3.st_ctime) e(51);
#ifndef V1_FILESYSTEM
  if (st1.st_atime >= st3.st_atime) e(52);	/* except for atime. */
#endif
  if (write(fd2, "Howdy\n", 6) != 6) e(53);	/* Update c & mtime. */
  if ((fd1 = open("ha", O_RDWR)) != 3) e(54);
  if (read(fd1, buf, 6) != 6) e(55);	/* Update atime. */
  if (strncmp(buf, "Howdy\n", 6) != 0) e(56);
  if (close(fd1) != 0) e(57);
  Stat("ha", &st3);
  if (st2.st_uid != st3.st_uid) e(58);
  if (st2.st_gid != st3.st_gid) e(59);	/* should be same */
  if (st2.st_mode != st3.st_mode) e(60);
  if (st2.st_nlink != st3.st_nlink) e(61);
  if (st2.st_ctime >= st3.st_ctime) e(62);
#ifndef V1_FILESYSTEM
  if (st2.st_atime >= st3.st_atime) e(63);
#endif
  if (st2.st_mtime >= st3.st_mtime) e(64);
  if (st2.st_size != st3.st_size) e(65);
  if (close(fd2) != 0) e(66);

  /* Let's see if RDONLY files are read only. */
  if ((fd1 = open("hi", O_RDONLY)) != 3) e(67);
  if (write(fd1, " again", 7) != -1) e(68);	/* we can't write */
  if (errno != EBADF) e(69);	/* a read only fd */
  if (read(fd1, buf, 7) != 6) e(70);	/* but we can read */
  if (close(fd1) != 0) e(71);

  /* Let's see if WRONLY files are write only. */
  if ((fd1 = open("hi", O_WRONLY)) != 3) e(72);
  if (read(fd1, buf, 7) != -1) e(73);	/* we can't read */
  if (errno != EBADF) e(74);	/* a write only fd */
  if (write(fd1, "hELLO", 6) != 6) e(75);	/* but we can write */
  if (close(fd1) != 0) e(76);

  /* Let's see if files are closable only once. */
  if (close(fd1) != -1) e(77);
  if (errno != EBADF) e(78);

  /* Let's see how calling close() with bad fds is handled. */
  if (close(10) != -1) e(79);
  if (errno != EBADF) e(80);
  if (close(111) != -1) e(81);
  if (errno != EBADF) e(82);
  if (close(-432) != -1) e(83);
  if (errno != EBADF) e(84);

  /* Let's see if RDWR files are read & write able. */
  if ((fd1 = open("hi", O_RDWR)) != 3) e(85);
  if (read(fd1, buf, 6) != 6) e(86);	/* we can read */
  if (strncmp(buf, "hELLO", 6) != 0) e(87);	/* and we can write */
  if (write(fd1, "Hello", 6) != 6) e(88);	/* a read write fd */
  if (close(fd1) != 0) e(89);

  /* Check if APPENDed files are realy appended */
  if ((fd1 = open("hi", O_RDWR | O_APPEND)) != 3) e(90);	/* open hi */

  /* An open should set the file offset to 0. */
  if (lseek(fd1, (off_t) 0, SEEK_CUR) != 0) e(91);

  /* Writing 0 bytes should not have an effect. */
  if (write(fd1, "", 0) != 0) e(92);
  if (lseek(fd1, (off_t) 0, SEEK_CUR) != 0) e(93);	/* the end? */

  /* A seek befor a wirte should not matter with O_APPEND. */
  Stat("hi", &st1);
  if (lseek(fd1, (off_t) - 3, SEEK_END) != st1.st_size - 3) e(94);

  /* By writing 1 byte, we force the offset to the end of the file */
  if (write(fd1, "1", 1) != 1) e(95);
  Stat("hi", &st1);
  if (lseek(fd1, (off_t) 0, SEEK_CUR) != st1.st_size) e(96);
  if (write(fd1, "2", 1) != 1) e(97);
  Stat("hi", &st1);
  if (lseek(fd1, (off_t) 0, SEEK_CUR) != st1.st_size) e(98);
  if (write(fd1, "3", 1) != 1) e(99);
  Stat("hi", &st1);
  if (lseek(fd1, (off_t) 0, SEEK_CUR) != st1.st_size) e(100);
  if (lseek(fd1, (off_t) - 2, SEEK_CUR) <= 0) e(101);
  if (write(fd1, "4", 1) != 1) e(102);

  /* Since the mode was O_APPEND, the offset should be reset to EOF */
  Stat("hi", &st1);
  if (lseek(fd1, (off_t) 0, SEEK_CUR) != st1.st_size) e(103);
  if (lseek(fd1, (off_t) - 4, SEEK_CUR) != st1.st_size - 4) e(104);
  if (read(fd1, buf, BUF_SIZE) != 4) e(105);
  if (strncmp(buf, "1234", 4) != 0) e(106);
  if (close(fd1) != 0) e(107);

  /* Check the effect of O_CREAT */
  Stat("ho", &st1);
  fd1 = open("ho", O_RDWR | O_CREAT, 0000);
  if (fd1 != 3) e(108);
  Stat("ho", &st2);
  if (memcmp(&st1, &st2, sizeof(struct stat)) != 0) e(109);
  if (read(fd1, buf, 6) != 6) e(110);
  if (strncmp(buf, "Hello\n", 6) != 0) e(111);
  if (write(fd1, "@", 1) != 1) e(112);
  if (close(fd1) != 0) e(113);
  (void) umask(0000);
  fd1 = open("ho", O_RDWR | O_CREAT | O_EXCL, 0777);
  if (fd1 != -1) e(114);	/* ho exists */
  System("rm -rf new");
  time(&time1);
  while (time1 >= time((time_t *)0))	
	;
  fd1 = open("new", O_RDWR | O_CREAT, 0716);
  if (fd1 != 3) e(115);		/* new file */
  Stat("new", &st1);
  time(&time2);
  while (time2 >= time((time_t *)0))
	;
  time(&time2);
  if (st1.st_uid != geteuid()) e(116);	/* try this as superuser. */
  if (st1.st_gid != getegid()) e(117);
  if ((st1.st_mode & 0777) != 0716) e(118);
  if (st1.st_nlink != 1) e(119);
  if (st1.st_mtime <= time1) e(120);
  if (st1.st_mtime >= time2) e(121);
#ifndef V1_FILESYSTEM
  if (st1.st_atime != st1.st_mtime) e(122);
#endif
  if (st1.st_ctime != st1.st_mtime) e(123);
  if (st1.st_size != 0) e(124);
  if (write(fd1, "I'm new in town", 16) != 16) e(125);
  if (lseek(fd1, (off_t) - 5, SEEK_CUR) != 11) e(126);
  if (read(fd1, buf, 5) != 5) e(127);
  if (strncmp(buf, "town", 5) != 0) e(128);
  if (close(fd1) != 0) e(129);

  /* Let's test the O_TRUNC flag on this new file. */
  time(&time1);
  while (time1 >= time((time_t *)0));
  if ((fd1 = open("new", O_RDWR | O_TRUNC)) != 3) e(130);
  Stat("new", &st1);
  time(&time2);
  while (time2 >= time((time_t *)0));
  time(&time2);
  if ((st1.st_mode & 0777) != 0716) e(131);
  if (st1.st_size != (size_t) 0) e(132);	/* TRUNCed ? */
  if (st1.st_mtime <= time1) e(133);
  if (st1.st_mtime >= time2) e(134);
  if (st1.st_ctime != st1.st_mtime) e(135);
  if (close(fd1) != 0) e(136);

  /* Test if file permission bits and the file ownership are unchanged. */
  /* So we will see if `O_CREAT' has no effect if the file exists. */
  if (superuser) {
	System("echo > bar; chmod 077 bar");	/* Make bar 077 */
	System("chown daemon bar");
	System("chgrp daemon bar");	/* Daemon's bar */
	fd1 = open("bar", O_RDWR | O_CREAT | O_TRUNC, 0777);	/* knock knock */
	if (fd1 == -1) e(137);
	if (write(fd1, "foo", 3) != 3) e(138);	/* rewrite bar */
	if (close(fd1) != 0) e(139);
	Stat("bar", &st1);
	if (st1.st_uid != 1) e(140);	/* bar is still */
	if (st1.st_gid != 1) e(141);	/* owned by daemon */
	if ((st1.st_mode & 0777) != 077) e(142);	/* mode still is 077 */
	if (st1.st_size != (size_t) 3) e(143);	/* 3 bytes long */

	/* We do the whole thing again, but with O_WRONLY */
	fd1 = open("bar", O_WRONLY | O_CREAT | O_TRUNC, 0777);
	if (fd1 == -1) e(144);
	if (write(fd1, "foobar", 6) != 6) e(145);	/* rewrite bar */
	if (close(fd1) != 0) e(146);
	Stat("bar", &st1);
	if (st1.st_uid != 1) e(147);	/* bar is still */
	if (st1.st_gid != 1) e(148);	/* owned by daemon */
	if ((st1.st_mode & 0777) != 077) e(149);	/* mode still is 077 */
	if (st1.st_size != (size_t) 6) e(150);	/* 6 bytes long */
  }
}

void test25c()
{				/* Test normal operation Part two. */
  int fd1, fd2;
  char buf[BUF_SIZE];
  struct stat st;
  int stat_loc;
  static int iteration=0;

  subtest = 3;
  iteration++;

  System("rm -rf ../DIR_25/*");

  /* Fifo file test here. */
  if (mkfifo("fifo", 0777) != 0) e(1);
  switch (fork()) {
      case -1:	printf("Can't fork\n");	break;
      case 0:
	alarm(20);		/* Give child 20 seconds to live. */
	if ((fd1 = open("fifo", O_RDONLY)) != 3) e(2);
	if (read(fd1, buf, BUF_SIZE) != 23) e(3);
	if (strncmp(buf, "1 2 3 testing testing\n", 23) != 0) e(4);
	if (close(fd1) != 0) e(5);
	exit(0);
      default:
	if ((fd1 = open("fifo", O_WRONLY)) != 3) e(6);
	if (write(fd1, "1 2 3 testing testing\n", 23) != 23) e(7);
	if (close(fd1) != 0) e(8);
	if (wait(&stat_loc) == -1) e(9);
	if (stat_loc != 0) e(10);	/* The alarm went off? */
  }

  /* Try opening for writing with O_NONBLOCK. */
  fd1 = open("fifo", O_WRONLY | O_NONBLOCK);
  if (fd1 != -1) e(11);
  if (errno != ENXIO) e(12);
  close(fd1);

  /* Try opening for writing with O_NONBLOCK and O_CREAT. */
  fd1 = open("fifo", O_WRONLY | O_CREAT | O_NONBLOCK, 0777);
  if (fd1 != -1) e(13);
  if (errno != ENXIO) e(14);
  close(fd1);

  /* Both the NONBLOCK and the EXCLusive give raise to error. */
  fd1 = open("fifo", O_WRONLY | O_CREAT | O_EXCL | O_NONBLOCK, 0777);
  if (fd1 != -1) e(15);
  if (errno != EEXIST && errno != ENXIO) e(16);
  close(fd1);			/* Just in case. */

  /* Try opening for reading with O_NONBLOCK. */
  fd1 = open("fifo", O_RDONLY | O_NONBLOCK);
  if (fd1 != 3) e(17);
  if (close(fd1) != 0) e(18);

/* Nopt runs out of memory. ;-< We just cut out some valid code */
  /* FIFO's should always append. (They have no file position.) */
  switch (fork()) {
      case -1:	printf("Can't fork\n");	break;
      case 0:
	alarm(20);		/* Give child 20 seconds to live. */
	if ((fd1 = open("fifo", O_WRONLY)) != 3) e(19);
	if ((fd2 = open("fifo", O_WRONLY)) != 4) e(20);
	if (write(fd1, "I did see Elvis.\n", 18) != 18) e(21);
	if (write(fd2, "I DID.\n", 8) != 8) e(22);
	if (close(fd2) != 0) e(23);
	if (close(fd1) != 0) e(24);
	exit(0);
      default:
	if ((fd1 = open("fifo", O_RDONLY)) != 3) e(25);
	if (read(fd1, buf, 18) != 18) e(26);
	if (strncmp(buf, "I did see Elvis.\n", 18) != 0) e(27);
	if (read(fd1, buf, BUF_SIZE) != 8) e(28);
	if (strncmp(buf, "I DID.\n", 8) != 0) e(29);
	if (close(fd1) != 0) e(30);
	if (wait(&stat_loc) == -1) e(31);
	if (stat_loc != 0) e(32);	/* The alarm went off? */
  }

  /* O_TRUNC should have no effect on FIFO files. */
  switch (fork()) {
      case -1:	printf("Can't fork\n");	break;
      case 0:
	alarm(20);		/* Give child 20 seconds to live. */
	if ((fd1 = open("fifo", O_WRONLY)) != 3) e(33);
	if (write(fd1, "I did see Elvis.\n", 18) != 18) e(34);
	if ((fd2 = open("fifo", O_WRONLY | O_TRUNC)) != 4) e(35);
	if (write(fd2, "I DID.\n", 8) != 8) e(36);
	if (close(fd2) != 0) e(37);
	if (close(fd1) != 0) e(38);
	exit(0);
      default:
	if ((fd1 = open("fifo", O_RDONLY)) != 3) e(39);
	if (read(fd1, buf, 18) != 18) e(40);
	if (strncmp(buf, "I did see Elvis.\n", 18) != 0) e(41);
	if (read(fd1, buf, BUF_SIZE) != 8) e(42);
	if (strncmp(buf, "I DID.\n", 8) != 0) e(43);
	if (close(fd1) != 0) e(44);
	if (wait(&stat_loc) == -1) e(45);
	if (stat_loc != 0) e(46);	/* The alarm went off? */
  }

  /* Closing the last fd should flush all data to the bitbucket. */
  System("rm -rf /tmp/sema.25");
  switch (fork()) {
      case -1:	printf("Can't fork\n");	break;

      case 0:
	alarm(20);		/* Give child 20 seconds to live. */
	if ((fd1 = open("fifo", O_WRONLY)) != 3) e(47);
	if (write(fd1, "I did see Elvis.\n", 18) != 18) e(48);
	Creat("/tmp/sema.25");
	sleep(2);		/* give parent a chance to open */
	/* this was sleep(1), but that's too short: child also sleeps(1) */
	if (close(fd1) != 0) e(49);
	exit(0);

      default:
	if ((fd1 = open("fifo", O_RDONLY)) != 3) e(50);
	/* Make `sure' write has closed. */
	while (stat("/tmp/sema.25", &st) != 0) sleep(1);
	if (close(fd1) != 0) e(51);
	if ((fd1 = open("fifo", O_RDONLY | O_NONBLOCK)) != 3) e(52);
	if (read(fd1, buf, BUF_SIZE) != 18) e(53);
	if (close(fd1) != 0) e(54);
	if (wait(&stat_loc) == -1) e(55);
	if (stat_loc != 0) e(56);	/* The alarm went off? */
  }

  /* Let's try one too many. */
  System("rm -rf /tmp/sema.25");
  switch (fork()) {
      case -1:	printf("Can't fork\n");	break;
      case 0:
	alarm(20);		/* Give child 20 seconds to live. */
	if ((fd1 = open("fifo", O_WRONLY)) != 3) e(57);
	if (write(fd1, "I did see Elvis.\n", 18) != 18) e(58);

	/* Keep open till third reader is opened. */
	while (stat("/tmp/sema.25", &st) != 0) sleep(1);
	if (close(fd1) != 0) e(59);
	exit(0);
      default:
	if ((fd1 = open("fifo", O_RDONLY)) != 3) e(60);
	if (read(fd1, buf, 2) != 2) e(61);
	if (strncmp(buf, "I ", 2) != 0) e(62);
	if (close(fd1) != 0) e(63);
	if ((fd1 = open("fifo", O_RDONLY)) != 3) e(64);
	if (read(fd1, buf, 4) != 4) e(65);
	if (strncmp(buf, "did ", 4) != 0) e(66);
	if ((fd2 = open("fifo", O_RDONLY)) != 4) e(67);

	/* Signal third reader is open. */
	Creat("/tmp/sema.25");
	if (read(fd2, buf, BUF_SIZE) != 12) e(68);
	if (strncmp(buf, "see Elvis.\n", 12) != 0) e(69);
	if (close(fd2) != 0) e(70);
	if (close(fd1) != 0) e(71);
	if (wait(&stat_loc) == -1) e(72);
	if (stat_loc != 0) e(73);	/* The alarm went off? */
  }
  System("rm -rf fifo /tmp/sema.25");

  /* O_TRUNC should have no effect on directroys. */
  System("mkdir dir; touch dir/f1 dir/f2 dir/f3");
  if ((fd1 = open("dir", O_WRONLY | O_TRUNC)) != -1) e(74);
  if (errno != EISDIR) e(75);
  close(fd1);

  /* Opening a directory for reading should be possible. */
  if ((fd1 = open("dir", O_RDONLY)) != 3) e(76);
  if (close(fd1) != 0) e(77);
  if (unlink("dir/f1") != 0) e(78);	/* Should still be there. */
  if (unlink("dir/f2") != 0) e(79);
  if (unlink("dir/f3") != 0) e(80);
  if (rmdir("dir") != 0) e(81);

  if (!superuser) {
	/* Test if O_CREAT is not usable to open files with the wrong mode */
	(void) umask(0200);	/* nono has no */
	System("touch nono");	/* write bit */
	(void) umask(0000);
	fd1 = open("nono", O_RDWR | O_CREAT, 0777);	/* try to open */
	if (fd1 != -1) e(82);
	if (errno != EACCES) e(83);	/* but no access */
  }
}

void test25d()
{
  int fd;

  subtest = 4;

  System("rm -rf ../DIR_25/*");

  /* Test maximal file name length. */
  if ((fd = open(MaxName, O_RDWR | O_CREAT, 0777)) != 3) e(1);
  if (close(fd) != 0) e(2);
  MaxPath[strlen(MaxPath) - 2] = '/';
  MaxPath[strlen(MaxPath) - 1] = 'a';	/* make ././.../a */
  if ((fd = open(MaxPath, O_RDWR | O_CREAT, 0777)) != 3) e(3);
  if (close(fd) != 0) e(4);
  MaxPath[strlen(MaxPath) - 1] = '/';	/* make ././.../a */
}

void test25e()
{
  int fd, does_truncate;
  char *noread = "noread";	/* Name for unreadable file. */
  char *nowrite = "nowrite";	/* Same for unwritable. */
  int stat_loc;

  subtest = 5;

  System("rm -rf ../DIR_25/*");

  mkdir("bar", 0777);		/* make bar */

  /* Check if no access on part of path generates the correct error. */
  System("chmod 677 bar");	/* rw-rwxrwx */
  if (open("bar/nono", O_RDWR | O_CREAT, 0666) != -1) e(1);
  if (errno != EACCES) e(2);

  /* Ditto for no write permission. */
  System("chmod 577 bar");	/* r-xrwxrwx */
  if (open("bar/nono", O_RDWR | O_CREAT, 0666) != -1) e(3);
  if (errno != EACCES) e(4);

  /* Clean up bar. */
  System("rm -rf bar");

  /* Improper flags set on existing file. */
  System("touch noread; chmod 377 noread");	/* noread */
  if (open(noread, O_RDONLY) != -1) e(5);
  if (open(noread, O_RDWR) != -1) e(6);
  if (open(noread, O_RDWR | O_CREAT, 0777) != -1) e(7);
  if (open(noread, O_RDWR | O_CREAT | O_TRUNC, 0777) != -1) e(8);
  if ((fd = open(noread, O_WRONLY)) != 3) e(9);
  if (close(fd) != 0) e(10);
  System("touch nowrite; chmod 577 nowrite");	/* nowrite */
  if (open(nowrite, O_WRONLY) != -1) e(11);
  if (open(nowrite, O_RDWR) != -1) e(12);
  if (open(nowrite, O_RDWR | O_CREAT, 0777) != -1) e(13);
  if (open(nowrite, O_RDWR | O_CREAT | O_TRUNC, 0777) != -1) e(14);
  if ((fd = open(nowrite, O_RDONLY)) != 3) e(15);
  if (close(fd) != 0) e(16);
  if (superuser) {
	/* If we can make a file ownd by some one else, test access again. */
	System("chmod 733 noread");
	System("chown bin noread");
	System("chgrp system noread");
	System("chmod 755 nowrite");
	System("chown bin nowrite");
	System("chgrp system nowrite");
	switch (fork()) {
	    case -1:	printf("Can't fork\n");	break;
	    case 0:
		setuid(1);
		setgid(1);	/* become daemon */
		if (open(noread, O_RDONLY) != -1) e(17);
		if (open(noread, O_RDWR) != -1) e(18);
		if (open(noread, O_RDWR | O_CREAT, 0777) != -1) e(19);
		fd = open(noread, O_RDWR | O_CREAT | O_TRUNC, 0777);
		if (fd != -1) e(20);
		if ((fd = open(noread, O_WRONLY)) != 3) e(21);
		if (close(fd) != 0) e(22);
		if (open(nowrite, O_WRONLY) != -1) e(23);
		if (open(nowrite, O_RDWR) != -1) e(24);
		if (open(nowrite, O_RDWR | O_CREAT, 0777) != -1) e(25);
		fd = open(nowrite, O_RDWR | O_CREAT | O_TRUNC, 0777);
		if (fd != -1) e(26);
		if ((fd = open(nowrite, O_RDONLY)) != 3) e(27);
		if (close(fd) != 0) e(28);
		exit(0);
	    default:
		if (wait(&stat_loc) == -1) e(29);
	}
  }

  /* Clean up the noread and nowrite files. */
  System("rm -rf noread nowrite");

  /* Test the O_EXCL flag. */
  System("echo > exists");
  if (open("exists", O_RDWR | O_CREAT | O_EXCL, 0777) != -1) e(30);
  if (errno != EEXIST) e(31);
  if (open("exists", O_RDONLY | O_CREAT | O_EXCL, 0777) != -1) e(32);
  if (errno != EEXIST) e(33);
  if (open("exists", O_WRONLY | O_CREAT | O_EXCL, 0777) != -1) e(34);
  if (errno != EEXIST) e(35);
  fd = open("exists", O_RDWR | O_CREAT | O_EXCL | O_TRUNC, 0777);
  if (fd != -1) e(36);
  if (errno != EEXIST) e(37);
  fd = open("exists", O_RDONLY | O_CREAT | O_EXCL | O_TRUNC, 0777);
  if (fd != -1) e(38);
  if (errno != EEXIST) e(39);
  fd = open("exists", O_WRONLY | O_CREAT | O_EXCL | O_TRUNC, 0777);
  if (fd != -1) e(40);
  if (errno != EEXIST) e(41);

  /* open should fail when O_CREAT|O_EXCL are set and a symbolic link names
     a file with EEXIST (regardless of link actually works or not) */
  if (symlink("exists", "slinktoexists") == -1) e(42);
  if (open("slinktoexists", O_RDWR | O_CREAT | O_EXCL, 0777) != -1) e(43);
  if (unlink("exists") == -1) e(44);
  /* "slinktoexists has become a dangling symlink. open(2) should still fail
     with EEXIST */
  if (open("slinktoexists", O_RDWR | O_CREAT | O_EXCL, 0777) != -1) e(45);
  if (errno != EEXIST) e(46);
  

  /* Test ToLongName and ToLongPath */
  does_truncate = does_fs_truncate();
  fd = open(ToLongName, O_RDWR | O_CREAT, 0777);
  if (does_truncate) {
  	if (fd == -1) e(47);
  	if (close(fd) != 0) e(48);
  } else {
  	if (fd != -1) e(49);
  	(void) close(fd);		/* Just in case */
  }

  ToLongPath[PATH_MAX - 2] = '/';
  ToLongPath[PATH_MAX - 1] = 'a';
  if ((fd = open(ToLongPath, O_RDWR | O_CREAT, 0777)) != -1) e(50);
  if (errno != ENAMETOOLONG) e(51);
  if (close(fd) != -1) e(52);
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

