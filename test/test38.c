/* Many of the tests require 1.6.n, n > 16, so we may as well assume that
 * POSIX signals are implemented.
 */
#define SIGACTION

/* test38: read(), write()	Author: Jan-Mark Wams (jms@cs.vu.nl) */

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
#include <signal.h>
#include <stdio.h>

int max_error = 	4;
#include "common.h"

#define ITERATIONS      3
#define BUF_SIZE 1024

#define System(cmd)	if (system(cmd) != 0) printf("``%s'' failed\n", cmd)
#define Chdir(dir)	if (chdir(dir) != 0) printf("Can't goto %s\n", dir)
#define Stat(a,b)	if (stat(a,b) != 0) printf("Can't stat %s\n", a)


int superuser;
int signumber = 0;

void test38a(void);
void test38b(void);
void test38c(void);
void setsignumber(int _signumber);

int main(int argc, char *argv[])
{
  int i, m = 0xFFFF;

  sync();
  start(38);

  if (argc == 2) m = atoi(argv[1]);
  superuser = (geteuid() == 0);
  umask(0000);

  for (i = 0; i < ITERATIONS; i++) {
	if (m & 0001) test38a();
	if (m & 0002) test38b();
	if (m & 0004) test38c();
  }
  quit();

  return(-1);	/* Unreachable */
}

void test38a()
{				/* Try normal operation. */
  int fd1;
  struct stat st1, st2;
  time_t time1;
  char buf[BUF_SIZE];
  int stat_loc;
  int i, j;
  int tube[2];

  subtest = 1;
  System("rm -rf ../DIR_38/*");

  /* Let's open bar. */
  if ((fd1 = open("bar", O_RDWR | O_CREAT, 0777)) != 3) e(1);
  Stat("bar", &st1);

  /* Writing nothing should not affect the file at all. */
  if (write(fd1, "", 0) != 0) e(2);
  Stat("bar", &st2);
  if (st1.st_uid != st2.st_uid) e(3);
  if (st1.st_gid != st2.st_gid) e(4);	/* should be same */
  if (st1.st_mode != st2.st_mode) e(5);
  if (st1.st_size != st2.st_size) e(6);
  if (st1.st_nlink != st2.st_nlink) e(7);
  if (st1.st_mtime != st2.st_mtime) e(8);
  if (st1.st_ctime != st2.st_ctime) e(9);
  if (st1.st_atime != st2.st_atime) e(10);

  /* A write should update some status fields. */
  time(&time1);
  while (time1 >= time((time_t *)0))
	;
  if (write(fd1, "foo", 4) != 4) e(11);
  Stat("bar", &st2);
  if (st1.st_mode != st2.st_mode) e(12);
  if (st1.st_size >= st2.st_size) e(13);
  if ((off_t) 4 != st2.st_size) e(14);
  if (st1.st_nlink != st2.st_nlink) e(15);
  if (st1.st_mtime >= st2.st_mtime) e(16);
  if (st1.st_ctime >= st2.st_ctime) e(17);
  if (st1.st_atime != st2.st_atime) e(18);

  /* Lseeks should not change the file status. */
  if (lseek(fd1, (off_t) - 2, SEEK_END) != 2) e(19);
  Stat("bar", &st1);
  if (st1.st_mode != st2.st_mode) e(20);
  if (st1.st_size != st2.st_size) e(21);
  if (st1.st_nlink != st2.st_nlink) e(22);
  if (st1.st_mtime != st2.st_mtime) e(23);
  if (st1.st_ctime != st2.st_ctime) e(24);
  if (st1.st_atime != st2.st_atime) e(25);

  /* Writing should start at the current (2) position. */
  if (write(fd1, "foo", 4) != 4) e(26);
  Stat("bar", &st2);
  if (st1.st_mode != st2.st_mode) e(27);
  if (st1.st_size >= st2.st_size) e(28);
  if ((off_t) 6 != st2.st_size) e(29);
  if (st1.st_nlink != st2.st_nlink) e(30);
  if (st1.st_mtime > st2.st_mtime) e(31);
  if (st1.st_ctime > st2.st_ctime) e(32);
  if (st1.st_atime != st2.st_atime) e(33);

  /* A read of zero bytes should not affect anything. */
  if (read(fd1, buf, 0) != 0) e(34);
  Stat("bar", &st1);
  if (st1.st_uid != st2.st_uid) e(35);
  if (st1.st_gid != st2.st_gid) e(36);	/* should be same */
  if (st1.st_mode != st2.st_mode) e(37);
  if (st1.st_size != st2.st_size) e(38);
  if (st1.st_nlink != st2.st_nlink) e(39);
  if (st1.st_mtime != st2.st_mtime) e(40);
  if (st1.st_ctime != st2.st_ctime) e(41);
  if (st1.st_atime != st2.st_atime) e(42);

  /* The file now should contain ``fofoo\0'' Let's check that. */
  if (lseek(fd1, (off_t) 0, SEEK_SET) != 0) e(43);
  if (read(fd1, buf, BUF_SIZE) != 6) e(44);
  if (strcmp(buf, "fofoo") != 0) e(45);

  /* Only the Access Time should be updated. */
  Stat("bar", &st2);
  if (st1.st_mtime != st2.st_mtime) e(46);
  if (st1.st_ctime != st2.st_ctime) e(47);
  if (st1.st_atime >= st2.st_atime) e(48);

  /* A read of zero bytes should do nothing even at the end of the file. */
  time(&time1);
  while (time1 >= time((time_t *)0))
	;
  if (read(fd1, buf, 0) != 0) e(49);
  Stat("bar", &st1);
  if (st1.st_size != st2.st_size) e(50);
  if (st1.st_mtime != st2.st_mtime) e(51);
  if (st1.st_ctime != st2.st_ctime) e(52);
  if (st1.st_atime != st2.st_atime) e(53);

  /* Reading should be done from the current offset. */
  if (read(fd1, buf, BUF_SIZE) != 0) e(54);
  if (lseek(fd1, (off_t) 2, SEEK_SET) != 2) e(55);
  if (read(fd1, buf, BUF_SIZE) != 4) e(56);
  if (strcmp(buf, "foo") != 0) e(57);

  /* Reading should effect the current file position. */
  if (lseek(fd1, (off_t) 2, SEEK_SET) != 2) e(58);
  if (read(fd1, buf, 1) != 1) e(59);
  if (*buf != 'f') e(60);
  if (lseek(fd1, (off_t) 0, SEEK_CUR) != 3) e(61);
  if (read(fd1, buf, 1) != 1) e(62);
  if (*buf != 'o') e(63);
  if (lseek(fd1, (off_t) 0, SEEK_CUR) != 4) e(64);
  if (read(fd1, buf, 1) != 1) e(65);
  if (*buf != 'o') e(66);
  if (lseek(fd1, (off_t) 0, SEEK_CUR) != 5) e(67);
  if (read(fd1, buf, 1) != 1) e(68);
  if (*buf != '\0') e(69);
  if (lseek(fd1, (off_t) 0, SEEK_CUR) != 6) e(70);

  /* Read's at EOF should return 0. */
  if (read(fd1, buf, BUF_SIZE) != 0) e(71);
  if (lseek(fd1, (off_t) 0, SEEK_CUR) != 6) e(72);
  if (read(fd1, buf, BUF_SIZE) != 0) e(73);
  if (lseek(fd1, (off_t) 0, SEEK_CUR) != 6) e(74);
  if (read(fd1, buf, BUF_SIZE) != 0) e(75);
  if (lseek(fd1, (off_t) 0, SEEK_CUR) != 6) e(76);
  if (read(fd1, buf, BUF_SIZE) != 0) e(77);
  if (lseek(fd1, (off_t) 0, SEEK_CUR) != 6) e(78);
  if (read(fd1, buf, BUF_SIZE) != 0) e(79);
  if (lseek(fd1, (off_t) 0, SEEK_CUR) != 6) e(80);

  /* Writing should not always change the file size. */
  if (lseek(fd1, (off_t) 2, SEEK_SET) != 2) e(81);
  if (write(fd1, "ba", 2) != 2) e(82);
  if (lseek(fd1, (off_t) 0, SEEK_CUR) != 4) e(83);
  Stat("bar", &st1);
  if (st1.st_size != 6) e(84);

  /* Kill the \0 at the end. */
  if (lseek(fd1, (off_t) 5, SEEK_SET) != 5) e(85);
  if (write(fd1, "x", 1) != 1) e(86);

  /* And close the bar. */
  if (close(fd1) != 0) e(87);

  /* Try some stuff with O_APPEND. Bar contains ``fobaox'' */
  if ((fd1 = open("bar", O_RDWR | O_APPEND)) != 3) e(88);

  /* No matter what the file position is. Writes should append. */
  if (lseek(fd1, (off_t) 2, SEEK_SET) != 2) e(89);
  if (write(fd1, "y", 1) != 1) e(90);
  Stat("bar", &st1);
  if (st1.st_size != (off_t) 7) e(91);
  if (lseek(fd1, (off_t) 0, SEEK_CUR) != 7) e(92);
  if (lseek(fd1, (off_t) 2, SEEK_SET) != 2) e(93);
  if (write(fd1, "z", 2) != 2) e(94);

  /* The file should contain ``fobaoxyz\0'' == 9 chars long. */
  Stat("bar", &st1);
  if (st1.st_size != (off_t) 9) e(95);
  if (lseek(fd1, (off_t) 0, SEEK_CUR) != 9) e(96);

  /* Reading on a O_APPEND flag should be from the current offset. */
  if (lseek(fd1, (off_t) 0, SEEK_SET) != 0) e(97);
  if (read(fd1, buf, BUF_SIZE) != 9) e(98);
  if (strcmp(buf, "fobaoxyz") != 0) e(99);
  if (lseek(fd1, (off_t) 0, SEEK_CUR) != 9) e(100);

  if (close(fd1) != 0) e(101);

  /* Let's test fifo writes. First blocking. */
  if (mkfifo("fifo", 0777) != 0) e(102);

  /* Read from fifo but no writer. */
  System("rm -rf /tmp/sema.38a");
  switch (fork()) {
      case -1:	printf("Can't fork\n");	break;

      case 0:
	alarm(20);
	if ((fd1 = open("fifo", O_RDONLY)) != 3) e(103);
	system("> /tmp/sema.38a");
	system("while test -f /tmp/sema.38a; do sleep 1; done");
errno =0;
	if (read(fd1, buf, BUF_SIZE) != 0) e(104);
	if (read(fd1, buf, BUF_SIZE) != 0) e(105);
	if (read(fd1, buf, BUF_SIZE) != 0) e(106);
	if (close(fd1) != 0) e(107);
	exit(0);

      default:
	if ((fd1 = open("fifo", O_WRONLY)) != 3) e(108);
	while (stat("/tmp/sema.38a", &st1) != 0) sleep(1);
	if (close(fd1) != 0) e(109);
	unlink("/tmp/sema.38a");
	if (wait(&stat_loc) == -1) e(110);
	if (stat_loc != 0) e(111);	/* Alarm? */
  }

  /* Read from fifo should wait for writer. */
  switch (fork()) {
      case -1:	printf("Can't fork\n");	break;

      case 0:
	alarm(20);
	if ((fd1 = open("fifo", O_RDONLY)) != 3) e(112);
	if (read(fd1, buf, BUF_SIZE) != 10) e(113);
	if (strcmp(buf, "Hi reader") != 0) e(114);
	if (close(fd1) != 0) e(115);
	exit(0);

      default:
	if ((fd1 = open("fifo", O_WRONLY)) != 3) e(116);
	sleep(1);
	if (write(fd1, "Hi reader", 10) != 10) e(117);
	if (close(fd1) != 0) e(118);
	if (wait(&stat_loc) == -1) e(119);
	if (stat_loc != 0) e(120);	/* Alarm? */
  }

#if DEAD_CODE
  /* Does this test test what it is supposed to test??? */

  /* Read from fifo should wait for all writers to close. */
  switch (fork()) {
      case -1:	printf("Can't fork\n");	break;

      case 0:
	alarm(60);
	switch (fork()) {
	    case -1:	printf("Can't fork\n");	break;
	    case 0:
		alarm(20);
		if ((fd1 = open("fifo", O_WRONLY)) != 3) e(121);
		printf("C2 did open\n");
		if (close(fd1) != 0) e(122);
		printf("C2 did close\n");
		exit(0);
	    default:
		printf("C1 scheduled\n");
		if ((fd1 = open("fifo", O_WRONLY)) != 3) e(123);
		printf("C1 did open\n");
		sleep(2);
		if (close(fd1) != 0) e(124);
		printf("C1 did close\n");
		sleep(1);
		if (wait(&stat_loc) == -1) e(125);
		if (stat_loc != 0) e(126);	/* Alarm? */
	}
	exit(stat_loc);

      default: {
	int wait_status;
	printf("Parent running\n");
	sleep(1);				/* open in childs first */
	if ((fd1 = open("fifo", O_RDONLY)) != 3) e(127);
	if (read(fd1, buf, BUF_SIZE) != 0) e(128);
	if (close(fd1) != 0) e(129);
	printf("Parent closed\n");
	if ((wait_status=wait(&stat_loc)) == -1) e(130);

      printf("wait_status %d, stat_loc %d:", wait_status, stat_loc);
      if (WIFSIGNALED(stat_loc)) {
          printf(" killed, signal number %d\n", WTERMSIG(stat_loc));
      } 
      else if (WIFEXITED(stat_loc)) {
          printf(" normal exit, status %d\n", WEXITSTATUS(stat_loc));
      }

	if (stat_loc != 0) e(131);	/* Alarm? */
      }
  }
#endif

  /* PIPE_BUF has to have a nice value. */
  if (PIPE_BUF < 5) e(132);
  if (BUF_SIZE < 1000) e(133);

  /* Writes of blocks smaller than PIPE_BUF should be atomic. */
  System("rm -rf /tmp/sema.38b;> /tmp/sema.38b");
  switch (fork()) {
      case -1:	printf("Can't fork\n");	break;

      case 0:
	alarm(20);
	switch (fork()) {
	    case -1:	printf("Can't fork\n");	break;

	    case 0:
		alarm(20);
		if ((fd1 = open("fifo", O_WRONLY)) != 3) e(134);
		for (i = 0; i < 100; i++) write(fd1, "1234 ", 5);
		system("while test -f /tmp/sema.38b; do sleep 1; done");
		if (close(fd1) != 0) e(135);
		exit(0);

	    default:
		if ((fd1 = open("fifo", O_WRONLY)) != 3) e(136);
		for (i = 0; i < 100; i++) write(fd1, "1234 ", 5);
		while (stat("/tmp/sema.38b", &st1) == 0) sleep(1);
		if (close(fd1) != 0) e(137);
		if (wait(&stat_loc) == -1) e(138);
		if (stat_loc != 0) e(139);	/* Alarm? */
	}
	exit(stat_loc);

      default:
	if ((fd1 = open("fifo", O_RDONLY)) != 3) e(140);
	i = 0;
	memset(buf, '\0', BUF_SIZE);

	/* Read buffer full or till EOF. */
	do {
		j = read(fd1, buf + i, BUF_SIZE - i);
		if (j > 0) {
			if (j % 5 != 0) e(141);
			i += j;
		}
	} while (j > 0 && i < 1000);

	/* Signal the children to close write ends. This should not be */
	/* Necessary. But due to a bug in 1.16.6 this is necessary. */
	unlink("/tmp/sema.38b");
	if (j < 0) e(142);
	if (i != 1000) e(143);
	if (wait(&stat_loc) == -1) e(144);
	if (stat_loc != 0) e(145);	/* Alarm? */

	/* Check 200 times 1234. */
	for (i = 0; i < 200; i++)
		if (strncmp(buf + (i * 5), "1234 ", 5) != 0) break;
	if (i != 200) e(146);
	if (buf[1000] != '\0') e(147);
	if (buf[1005] != '\0') e(148);
	if (buf[1010] != '\0') e(149);
	if (read(fd1, buf, BUF_SIZE) != 0) e(150);
	if (close(fd1) != 0) e(151);
  }

  /* Read from pipe should wait for writer. */
  if (pipe(tube) != 0) e(152);
  switch (fork()) {
      case -1:	printf("Can't fork\n");	break;
      case 0:
	alarm(20);
	if (close(tube[1]) != 0) e(153);
	if (read(tube[0], buf, BUF_SIZE) != 10) e(154);
	if (strcmp(buf, "Hi reader") != 0) e(155);
	if (close(tube[0]) != 0) e(156);
	exit(0);
      default:
	if (close(tube[0]) != 0) e(157);
	sleep(1);
	if (write(tube[1], "Hi reader", 10) != 10) e(158);
	if (close(tube[1]) != 0) e(159);
	if (wait(&stat_loc) == -1) e(160);
	if (stat_loc != 0) e(161);	/* Alarm? */
  }

  /* Read from pipe should wait for all writers to close. */
  if (pipe(tube) != 0) e(162);
  switch (fork()) {
      case -1:	printf("Can't fork\n");	break;
      case 0:
	alarm(20);
	if (close(tube[0]) != 0) e(163);
	switch (fork()) {
	    case -1:	printf("Can't fork\n");	break;
	    case 0:
		alarm(20);
		if (close(tube[1]) != 0) e(164);
		exit(0);
	    default:
		sleep(1);
		if (close(tube[1]) != 0) e(165);
		if (wait(&stat_loc) == -1) e(166);
		if (stat_loc != 0) e(167);	/* Alarm? */
	}
	exit(stat_loc);
      default:
	if (close(tube[1]) != 0) e(168);
	if (read(tube[0], buf, BUF_SIZE) != 0) e(169);
	if (close(tube[0]) != 0) e(170);
	if (wait(&stat_loc) == -1) e(171);
	if (stat_loc != 0) e(172);	/* Alarm? */
  }

  /* Writes of blocks smaller than PIPE_BUF should be atomic. */
  System("rm -rf /tmp/sema.38c;> /tmp/sema.38c");
  if (pipe(tube) != 0) e(173);
  switch (fork()) {
      case -1:	printf("Can't fork\n");	break;
      case 0:
	alarm(20);
	if (close(tube[0]) != 0) e(174);
	switch (fork()) {
	    case -1:	printf("Can't fork\n");	break;
	    case 0:
		alarm(20);
		for (i = 0; i < 100; i++) write(tube[1], "1234 ", 5);
		system("while test -f /tmp/sema.38c; do sleep 1; done");
		if (close(tube[1]) != 0) e(175);
		exit(0);
	    default:
		for (i = 0; i < 100; i++) write(tube[1], "1234 ", 5);
		while (stat("/tmp/sema.38c", &st1) == 0) sleep(1);
		if (close(tube[1]) != 0) e(176);
		if (wait(&stat_loc) == -1) e(177);
		if (stat_loc != 0) e(178);	/* Alarm? */
	}
	exit(stat_loc);
      default:
	i = 0;
	if (close(tube[1]) != 0) e(179);
	memset(buf, '\0', BUF_SIZE);
	do {
		j = read(tube[0], buf + i, BUF_SIZE - i);
		if (j > 0) {
			if (j % 5 != 0) e(180);
			i += j;
		} else
			break;	/* EOF seen. */
	} while (i < 1000);
	unlink("/tmp/sema.38c");
	if (j < 0) e(181);
	if (i != 1000) e(182);
	if (close(tube[0]) != 0) e(183);
	if (wait(&stat_loc) == -1) e(184);
	if (stat_loc != 0) e(185);	/* Alarm? */

	/* Check 200 times 1234. */
	for (i = 0; i < 200; i++)
		if (strncmp(buf + (i * 5), "1234 ", 5) != 0) break;
	if (i != 200) e(186);
  }
}

void test38b()
{
  int i, fd, stat_loc;
  char buf[BUF_SIZE];
  char buf2[BUF_SIZE];
  struct stat st;

  subtest = 2;
  System("rm -rf ../DIR_38/*");

  /* Lets try sequential writes. */
  system("rm -rf /tmp/sema.38d");
  System("> testing");
  switch (fork()) {
      case -1:	printf("Can't fork\n");	break;
      case 0:
	alarm(20);
	if ((fd = open("testing", O_WRONLY | O_APPEND)) != 3) e(1);
	if (write(fd, "one ", 4) != 4) e(2);
	if (close(fd) != 0) e(3);
	system("> /tmp/sema.38d");
	system("while test -f /tmp/sema.38d; do sleep 1; done");
	if ((fd = open("testing", O_WRONLY | O_APPEND)) != 3) e(4);
	if (write(fd, "three ", 6) != 6) e(5);
	if (close(fd) != 0) e(6);
	system("> /tmp/sema.38d");
	exit(0);
      default:
	while (stat("/tmp/sema.38d", &st) != 0) sleep(1);
	if ((fd = open("testing", O_WRONLY | O_APPEND)) != 3) e(7);
	if (write(fd, "two ", 4) != 4) e(8);
	if (close(fd) != 0) e(9);
	unlink("/tmp/sema.38d");
	while (stat("/tmp/sema.38d", &st) != 0) sleep(1);
	if ((fd = open("testing", O_WRONLY | O_APPEND)) != 3) e(10);
	if (write(fd, "four", 5) != 5) e(11);
	if (close(fd) != 0) e(12);
	if (wait(&stat_loc) == -1) e(13);
	if (stat_loc != 0) e(14);	/* The alarm went off? */
	unlink("/tmp/sema.38d");
  }
  if ((fd = open("testing", O_RDONLY)) != 3) e(15);
  if (read(fd, buf, BUF_SIZE) != 19) e(16);
  if (strcmp(buf, "one two three four") != 0) e(17);
  if (close(fd) != 0) e(18);

  /* Non written bytes in regular files should be zero. */
  memset(buf2, '\0', BUF_SIZE);
  if ((fd = open("bigfile", O_RDWR | O_CREAT, 0644)) != 3) e(19);
  if (lseek(fd, (off_t) 102400, SEEK_SET) != (off_t) 102400L) e(20);
  if (read(fd, buf, BUF_SIZE) != 0) e(21);
  if (write(fd, ".", 1) != 1) e(22);
  Stat("bigfile", &st);
  if (st.st_size != (off_t) 102401) e(23);
  if (lseek(fd, (off_t) 0, SEEK_SET) != 0) e(24);
  for (i = 0; i < 102400 / BUF_SIZE; i++) {
	if (read(fd, buf, BUF_SIZE) != BUF_SIZE) e(25);
	if (memcmp(buf, buf2, BUF_SIZE) != 0) e(26);
  }
  if (close(fd) != 0) e(27);
}

void test38c()
{				/* Test correct error behavior. */
  char buf[BUF_SIZE];
  int fd, tube[2], stat_loc;
  struct stat st;
  pid_t pid;
#ifdef SIGACTION
  struct sigaction act, oact;
#else
  void (*oldfunc) (int);
#endif

  subtest = 3;
  System("rm -rf ../DIR_38/*");

  /* To test if writing processes on closed pipes are signumbered. */
#ifdef SIGACTION
  act.sa_handler = setsignumber;
  sigemptyset(&act.sa_mask);
  act.sa_flags = 0;
  if (sigaction(SIGPIPE, &act, &oact) != 0) e(1);
#else
  oldfunc = signal(SIGPIPE, setsignumber);
#endif

  /* Non valid file descriptors should be an error. */
  for (fd = -111; fd < 0; fd++) {
	errno = 0;
	if (read(fd, buf, BUF_SIZE) != -1) e(2);
	if (errno != EBADF) e(3);
  }
  for (fd = 3; fd < 111; fd++) {
	errno = 0;
	if (read(fd, buf, BUF_SIZE) != -1) e(4);
	if (errno != EBADF) e(5);
  }
  for (fd = -111; fd < 0; fd++) {
	errno = 0;
	if (write(fd, buf, BUF_SIZE) != -1) e(6);
	if (errno != EBADF) e(7);
  }
  for (fd = 3; fd < 111; fd++) {
	errno = 0;
	if (write(fd, buf, BUF_SIZE) != -1) e(8);
	if (errno != EBADF) e(9);
  }

  /* Writing a pipe with no readers should trigger SIGPIPE. */
  if (pipe(tube) != 0) e(10);
  close(tube[0]);
  switch (fork()) {
      case -1:	printf("Can't fork\n");	break;
      case 0:
	alarm(20);
	signumber = 0;
	if (write(tube[1], buf, BUF_SIZE) != -1) e(11);
	if (errno != EPIPE) e(12);
	if (signumber != SIGPIPE) e(13);
	if (close(tube[1]) != 0) e(14);
	exit(0);
      default:
	close(tube[1]);
	if (wait(&stat_loc) == -1) e(15);
	if (stat_loc != 0) e(16);	/* Alarm? */
  }

  /* Writing a fifo with no readers should trigger SIGPIPE. */
  System("> /tmp/sema.38e");
  if (mkfifo("fifo", 0666) != 0) e(17);
  switch (fork()) {
      case -1:	printf("Can't fork\n");	break;
      case 0:
	alarm(20);
	if ((fd = open("fifo", O_WRONLY)) != 3) e(18);
	system("while test -f /tmp/sema.38e; do sleep 1; done");
	signumber = 0;
	if (write(fd, buf, BUF_SIZE) != -1) e(19);
	if (errno != EPIPE) e(20);
	if (signumber != SIGPIPE) e(21);
	if (close(fd) != 0) e(22);
	exit(0);
      default:
	if ((fd = open("fifo", O_RDONLY)) != 3) e(23);
	if (close(fd) != 0) e(24);
	unlink("/tmp/sema.38e");
	if (wait(&stat_loc) == -1) e(25);
	if (stat_loc != 0) e(26);	/* Alarm? */
  }

#ifdef SIGACTION
  /* Restore normal (re)action to SIGPIPE. */
  if (sigaction(SIGPIPE, &oact, NULL) != 0) e(27);
#else
  signal(SIGPIPE, oldfunc);
#endif

  /* Read from fifo should return -1 and set errno to EAGAIN. */
  System("rm -rf /tmp/sema.38[fgh]");
  switch (fork()) {
      case -1:	printf("Can't fork\n");	break;
      case 0:
	alarm(20);
	system("while test ! -f /tmp/sema.38f; do sleep 1; done");
	System("rm -rf /tmp/sema.38f");
	if ((fd = open("fifo", O_WRONLY | O_NONBLOCK)) != 3) e(28);
	close(creat("/tmp/sema.38g", 0666));
	system("while test ! -f /tmp/sema.38h; do sleep 1; done");
	if (close(fd) != 0) e(38);
	System("rm -rf /tmp/sema.38h");
	exit(0);
      default:
	if ((fd = open("fifo", O_RDONLY | O_NONBLOCK)) != 3) e(30);
	close(creat("/tmp/sema.38f", 0666));
	system("while test ! -f /tmp/sema.38g; do sleep 1; done");
	System("rm -rf /tmp/sema.38g");
	if (read(fd, buf, BUF_SIZE) != -1) e(31);
	if (errno != EAGAIN) e(32);
	if (read(fd, buf, BUF_SIZE) != -1) e(33);
	if (errno != EAGAIN) e(34);
	if (read(fd, buf, BUF_SIZE) != -1) e(35);
	if (errno != EAGAIN) e(36);
	close(creat("/tmp/sema.38h", 0666));
	while (stat("/tmp/sema.38h", &st) == 0) sleep(1);
	if (read(fd, buf, BUF_SIZE) != 0) e(37);
	if (close(fd) != 0) e(38);
	if (wait(&stat_loc) == -1) e(39);
	if (stat_loc != 0) e(40);	/* Alarm? */
  }
  System("rm -rf fifo");

  /* If a read is interrupted by a SIGNAL. */
  if (pipe(tube) != 0) e(41);
  switch (pid = fork()) {
      case -1:	printf("Can't fork\n");	break;
      case 0:
	alarm(20);
#ifdef SIGACTION
	act.sa_handler = setsignumber;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	if (sigaction(SIGUSR1, &act, &oact) != 0) e(42);
#else
	oldfunc = signal(SIGUSR1, setsignumber);
#endif
	if (read(tube[0], buf, BUF_SIZE) != -1) e(43);
	if (errno != EINTR) e(44);
	if (signumber != SIGUSR1) e(45);
#ifdef SIGACTION
	/* Restore normal (re)action to SIGPIPE. */
	if (sigaction(SIGUSR1, &oact, NULL) != 0) e(46);
#else
	signal(SIGUSR1, oldfunc);
#endif
	close(tube[0]);
	close(tube[1]);
	exit(0);
      default:
	/* The sleep 1 should give the child time to start the read. */
	sleep(1);
	close(tube[0]);
	kill(pid, SIGUSR1);
	wait(&stat_loc);
	if (stat_loc != 0) e(47);	/* Alarm? */
	close(tube[1]);
  }
}

void setsignumber(signum)
int signum;
{
  signumber = signum;
}

