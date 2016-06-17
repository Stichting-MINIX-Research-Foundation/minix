/* test33: access()		Author: Jan-Mark Wams (jms@cs.vu.nl) */

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

int max_error = 1;
#include "common.h"

#define ITERATIONS     2

#define System(cmd)	if (system_p(cmd) != 0) printf("``%s'' failed\n", cmd)
#define Chdir(dir)	if (chdir(dir) != 0) printf("Can't goto %s\n", dir)
#define Stat(a,b)	if (stat(a,b) != 0) printf("Can't stat %s\n", a)
#define Chmod(a,b)	if (chmod(a,b) != 0) printf("Can't chmod %s\n", a)
#define Mkfifo(f)	if (mkfifo(f,0777)!=0) printf("Can't make fifo %s\n", f)

int superuser;			/* nonzero if uid == euid (euid == 0 always) */
char *MaxName;			/* Name of maximum length */
char MaxPath[PATH_MAX];		/* Same for path */
char *ToLongName;		/* Name of maximum +1 length */
char ToLongPath[PATH_MAX + 1];	/* Same for path, both too long */

void test33a(void);
void test33b(void);
void test33c(void);
void test33d(void);
void test_access(void);
void makelongnames(void);

int main(int argc, char *argv[])
{
  int i, m = 0xFFFF;

  sync();
  if (argc == 2) m = atoi(argv[1]);
  umask(0000);
  start(33);

  if (geteuid() != 0) {
	printf("must be setuid root; test aborted\n");
	cleanup();
	exit(1);
  }
  if (getuid() == 0) {
       printf("must be setuid root logged in as someone else; test aborted\n");
       cleanup();
       exit(1);
  }

  makelongnames();
  superuser = (getuid() == 0);

  for (i = 0; i < ITERATIONS; i++) {
	if (m & 0001) test33a();
	if (m & 0002) test33b();
	if (m & 0004) test33c();
	if (m & 0010) test33d();
  }
  quit();
  return 1;
}

void test33a()
{				/* Test normal operation. */
  int stat_loc;			/* For the wait(&stat_loc) call. */

  subtest = 1;
  System("rm -rf ../DIR_33/*");

  /* To test normal access first make some files for real uid. */
  switch (fork()) {
      case -1:	printf("Can't fork\n");	break;
      case 0:
	alarm(20);
	setuid(getuid());	/* (Re)set the effective ids to the
				 * real ids. */
	setgid(getgid());
	System("> rwx; chmod 700 rwx");
	System("> rw_; chmod 600 rw_");
	System("> r_x; chmod 500 r_x");
	System("> r__; chmod 400 r__");
	System("> _wx; chmod 300 _wx");
	System("> _w_; chmod 200 _w_");
	System("> __x; chmod 100 __x");
	System("> ___; chmod 000 ___");
	exit(0);

      default:
	wait(&stat_loc);
	if (stat_loc != 0) e(1);/* Alarm? */
  }
  test_access();

  /* Let's test access() on directorys. */
  switch (fork()) {
      case -1:	printf("Can't fork\n");	break;
      case 0:
	alarm(20);
	setuid(getuid());	/* (Re)set the effective ids to the
				 * real ids. */
	setgid(getgid());
	System("rm -rf [_r][_w][_x]");
	System("mkdir rwx; chmod 700 rwx");
	System("mkdir rw_; chmod 600 rw_");
	System("mkdir r_x; chmod 500 r_x");
	System("mkdir r__; chmod 400 r__");
	System("mkdir _wx; chmod 300 _wx");
	System("mkdir _w_; chmod 200 _w_");
	System("mkdir __x; chmod 100 __x");
	System("mkdir ___; chmod 000 ___");
	exit(0);

      default:
	wait(&stat_loc);
	if (stat_loc != 0) e(2);/* Alarm? */
  }
  test_access();

  switch (fork()) {
      case -1:	printf("Can't fork\n");	break;
      case 0:
	alarm(20);
	setuid(getuid());	/* (Re)set the effective ids to the
				 * real ids. */
	setgid(getgid());
	System("rmdir [_r][_w][_x]");
	Mkfifo("rwx");
	System("chmod 700 rwx");
	Mkfifo("rw_");
	System("chmod 600 rw_");
	Mkfifo("r_x");
	System("chmod 500 r_x");
	Mkfifo("r__");
	System("chmod 400 r__");
	Mkfifo("_wx");
	System("chmod 300 _wx");
	Mkfifo("_w_");
	System("chmod 200 _w_");
	Mkfifo("__x");
	System("chmod 100 __x");
	Mkfifo("___");
	System("chmod 000 ___");
	exit(0);

      default:
	wait(&stat_loc);
	if (stat_loc != 0) e(3);/* Alarm? */
  }
  test_access();

  /* Remove all the fifos. */
  switch (fork()) {
      case -1:	printf("Can't fork\n");	break;
      case 0:
	alarm(20);
	setuid(getuid());
	setgid(getgid());
	System("rm -rf [_r][_w][_x]");
	exit(0);

      default:
	wait(&stat_loc);
	if (stat_loc != 0) e(4);/* Alarm? */
  }
}

void test33b()
{
  int stat_loc;			/* For the wait(&stat_loc) call. */

  subtest = 2;
  System("rm -rf ../DIR_33/*");

  switch (fork()) {
      case -1:	printf("Can't fork\n");	break;
      case 0:
	alarm(20);

	/* (Re)set the effective ids to the real ids. */
	setuid(getuid());
	setgid(getgid());
	System("> ______rwx; chmod 007 ______rwx");
	System("> ________x; chmod 001 ________x");
	System("> _________; chmod 000 _________");
	exit(0);

      default:
	wait(&stat_loc);
	if (stat_loc != 0) e(1);/* Alarm? */
  }

  /* If we are superuser, we have access to all. */
  /* Well, almost, execution access might need at least one X bit. */
  if (superuser) {
	if (access("_________", R_OK) != 0) e(2);
	if (access("_________", W_OK) != 0) e(3);
	if (access("________x", R_OK) != 0) e(4);
	if (access("________x", W_OK) != 0) e(5);
	if (access("________x", X_OK) != 0) e(6);
	if (access("______rwx", R_OK) != 0) e(7);
	if (access("______rwx", W_OK) != 0) e(8);
	if (access("______rwx", X_OK) != 0) e(9);
  }
  if (!superuser) {
	if (access("_________", R_OK) != -1) e(10);
	if (errno != EACCES) e(11);
	if (access("_________", W_OK) != -1) e(12);
	if (errno != EACCES) e(13);
	if (access("_________", X_OK) != -1) e(14);
	if (errno != EACCES) e(15);
	if (access("________x", R_OK) != -1) e(16);
	if (errno != EACCES) e(17);
	if (access("________x", W_OK) != -1) e(18);
	if (errno != EACCES) e(19);
	if (access("________x", X_OK) != -1) e(20);
	if (errno != EACCES) e(21);
	if (access("______rwx", R_OK) != -1) e(22);
	if (errno != EACCES) e(23);
	if (access("______rwx", W_OK) != -1) e(24);
	if (errno != EACCES) e(25);
	if (access("______rwx", X_OK) != -1) e(26);
	if (errno != EACCES) e(27);
  }

  /* If the real uid != effective uid. */
  if (!superuser) {
	System("rm -rf [_r][_w][_x]");
	System("> rwx");
	Chmod("rwx", 0700);
	System("> rw_");
	Chmod("rw_", 0600);
	System("> r_x");
	Chmod("r_x", 0500);
	System("> r__");
	Chmod("r__", 0400);
	System("> _wx");
	Chmod("_wx", 0300);
	System("> _w_");
	Chmod("_w_", 0200);
	System("> __x");
	Chmod("__x", 0100);
	System("> ___");
	Chmod("___", 0000);

	if (access("rwx", F_OK) != 0) e(28);
	if (access("rwx", R_OK) != -1) e(29);
	if (errno != EACCES) e(30);
	if (access("rwx", W_OK) != -1) e(31);
	if (errno != EACCES) e(32);
	if (access("rwx", X_OK) != -1) e(33);
	if (errno != EACCES) e(34);

	if (access("rw_", F_OK) != 0) e(35);
	if (access("rw_", R_OK) != -1) e(36);
	if (errno != EACCES) e(37);
	if (access("rw_", W_OK) != -1) e(38);
	if (errno != EACCES) e(39);
	if (access("rw_", X_OK) != -1) e(40);
	if (errno != EACCES) e(41);

	if (access("r_x", F_OK) != 0) e(42);
	if (access("r_x", R_OK) != -1) e(43);
	if (errno != EACCES) e(44);
	if (access("r_x", W_OK) != -1) e(45);
	if (errno != EACCES) e(46);
	if (access("r_x", X_OK) != -1) e(47);
	if (errno != EACCES) e(48);

	if (access("r__", F_OK) != 0) e(49);
	if (access("r__", R_OK) != -1) e(50);
	if (errno != EACCES) e(51);
	if (access("r__", W_OK) != -1) e(52);
	if (errno != EACCES) e(53);
	if (access("r__", X_OK) != -1) e(54);
	if (errno != EACCES) e(55);

	if (access("_wx", F_OK) != 0) e(56);
	if (access("_wx", R_OK) != -1) e(57);
	if (errno != EACCES) e(58);
	if (access("_wx", W_OK) != -1) e(59);
	if (errno != EACCES) e(60);
	if (access("_wx", X_OK) != -1) e(61);
	if (errno != EACCES) e(62);

	if (access("_w_", F_OK) != 0) e(63);
	if (access("_w_", R_OK) != -1) e(64);
	if (errno != EACCES) e(65);
	if (access("_w_", W_OK) != -1) e(66);
	if (errno != EACCES) e(67);
	if (access("_w_", X_OK) != -1) e(68);
	if (errno != EACCES) e(69);

	if (access("__x", F_OK) != 0) e(70);
	if (access("__x", R_OK) != -1) e(71);
	if (errno != EACCES) e(72);
	if (access("__x", W_OK) != -1) e(73);
	if (errno != EACCES) e(74);
	if (access("__x", X_OK) != -1) e(75);
	if (errno != EACCES) e(76);

	if (access("___", F_OK) != 0) e(77);
	if (access("___", R_OK) != -1) e(78);
	if (errno != EACCES) e(79);
	if (access("___", W_OK) != -1) e(80);
	if (errno != EACCES) e(81);
	if (access("___", X_OK) != -1) e(82);
	if (errno != EACCES) e(83);

	System("rm -rf [_r][_w][_x]");
  }
}

void test33c()
{				/* Test errors returned. */
  int i, fd, does_truncate;

  subtest = 3;
  System("rm -rf ../DIR_33/*");

  /* Test what access() does with non existing files. */
  System("rm -rf nonexist");
  if (access("noexist", F_OK) != -1) e(1);
  if (errno != ENOENT) e(2);
  if (access("noexist", R_OK) != -1) e(3);
  if (errno != ENOENT) e(4);
  if (access("noexist", W_OK) != -1) e(5);
  if (errno != ENOENT) e(6);
  if (access("noexist", X_OK) != -1) e(7);
  if (errno != ENOENT) e(8);
  if (access("noexist", R_OK | W_OK) != -1) e(9);
  if (errno != ENOENT) e(10);
  if (access("noexist", R_OK | X_OK) != -1) e(11);
  if (errno != ENOENT) e(12);
  if (access("noexist", W_OK | X_OK) != -1) e(13);
  if (errno != ENOENT) e(14);
  if (access("noexist", R_OK | W_OK | X_OK) != -1) e(15);
  if (errno != ENOENT) e(16);

  /* Test access on a nonsearchable path. */
  if (mkdir("nosearch", 0777) != 0) e(1000);
  if ( (i = creat("nosearch/file", 0666)) < 0) e(1001);
  if (close(i) < 0) e(1002);
  if ( (i = creat("file", 0666)) < 0) e(1003);
  if (close(i) < 0) e(1004);
  if (chmod("nosearch/file", 05777) < 0) e(1005);
  if (chmod("file", 05777) < 0) e(1006);
  if (chmod("nosearch", 0677) != 0) e(1007);
  if (access("nosearch/file", F_OK) != 0) e(17);

  /* Test ToLongName and ToLongPath */
  does_truncate = does_fs_truncate();
  if (does_truncate) {
	if ((fd = creat(ToLongName, 0777)) == -1) e(18);
  	if (close(fd) != 0) e(19);
	if (access(ToLongName, F_OK) != 0) e(20);
  } else {
  	if ((fd = creat(ToLongName, 0777)) != -1) e(21);
	if (errno != ENAMETOOLONG) e(22);
  	(void) close(fd);	/* Just in case */
	if (access(ToLongName, F_OK) != -1) e(23);
	if (errno != ENAMETOOLONG) e(24);
  }

  ToLongPath[PATH_MAX - 2] = '/';
  ToLongPath[PATH_MAX - 1] = 'a';
  if (access(ToLongPath, F_OK) != -1) e(27);
  if (errno != ENAMETOOLONG) e(28);
  ToLongPath[PATH_MAX - 1] = '/';

  /* Test empty strings. */
  if (access("", F_OK) != -1) e(29);
  if (errno != ENOENT) e(30);
  System("rm -rf idonotexist");
  if (access("idonotexist", F_OK) != -1) e(31);
  if (errno != ENOENT) e(32);

  /* Test non directorys in prefix of path. */
  if (access("/etc/passwd/dir/foo", F_OK) != -1) e(33);
  if (errno != ENOTDIR) e(34);
  System("rm -rf nodir; > nodir");
  if (access("nodir/foo", F_OK) != -1) e(35);
  if (errno != ENOTDIR) e(36);

  /* Test if invalid amode arguments are signaled. */
  System("> allmod");
  Chmod("allmod", 05777);
  for (i = -1025; i < 1025; i++) {
	if ((mode_t) i != F_OK && ((mode_t) i & ~(R_OK | W_OK | X_OK)) != 0) {
		if (access("allmod", (mode_t) i) != -1) e(37);
		if (errno != EINVAL) e(38);
	} else 
		if (access("allmod", (mode_t) i) != 0) e(39);
  }
}

void test33d()
{				/* Test access() flags. */
#define EXCLUDE(a,b)	(((a)^(b)) == ((a)|(b)))
  subtest = 4;
  System("rm -rf ../DIR_33/*");

  /* The test are rather strong, stronger that POSIX specifies. */
  /* The should be OR able, this test tests if all the 1 bits */
  /* Are in diferent places. This should be what one wants. */
  if (!EXCLUDE(R_OK, W_OK | X_OK)) e(1);
  if (!EXCLUDE(W_OK, R_OK | X_OK)) e(2);
  if (!EXCLUDE(X_OK, R_OK | W_OK)) e(3);
  if (F_OK == R_OK) e(4);
  if (F_OK == W_OK) e(5);
  if (F_OK == X_OK) e(6);
  if (F_OK == (R_OK | W_OK)) e(7);
  if (F_OK == (W_OK | X_OK)) e(8);
  if (F_OK == (R_OK | X_OK)) e(9);
  if (F_OK == (R_OK | W_OK | X_OK)) e(10);
}

void test_access()
{				/* Test all [_r][_w][_x] files. */
  if (!superuser) {
	/* Test normal access. */
	if (access("rwx", F_OK) != 0) e(11);
	if (access("rwx", R_OK) != 0) e(12);
	if (access("rwx", W_OK) != 0) e(13);
	if (access("rwx", X_OK) != 0) e(14);
	if (access("rwx", R_OK | W_OK) != 0) e(15);
	if (access("rwx", R_OK | X_OK) != 0) e(16);
	if (access("rwx", W_OK | X_OK) != 0) e(17);
	if (access("rwx", R_OK | W_OK | X_OK) != 0) e(18);

	if (access("rw_", F_OK) != 0) e(19);
	if (access("rw_", R_OK) != 0) e(20);
	if (access("rw_", W_OK) != 0) e(21);
	if (access("rw_", X_OK) != -1) e(22);
	if (errno != EACCES) e(23);
	if (access("rw_", R_OK | W_OK) != 0) e(24);
	if (access("rw_", R_OK | X_OK) != -1) e(25);
	if (errno != EACCES) e(26);
	if (access("rw_", W_OK | X_OK) != -1) e(27);
	if (errno != EACCES) e(28);
	if (access("rw_", R_OK | W_OK | X_OK) != -1) e(29);
	if (errno != EACCES) e(30);

	if (access("r_x", F_OK) != 0) e(31);
	if (access("r_x", R_OK) != 0) e(32);
	if (access("r_x", W_OK) != -1) e(33);
	if (errno != EACCES) e(34);
	if (access("r_x", X_OK) != 0) e(35);
	if (access("r_x", R_OK | W_OK) != -1) e(36);
	if (errno != EACCES) e(37);
	if (access("r_x", R_OK | X_OK) != 0) e(38);
	if (access("r_x", W_OK | X_OK) != -1) e(39);
	if (errno != EACCES) e(40);
	if (access("r_x", R_OK | W_OK | X_OK) != -1) e(41);
	if (errno != EACCES) e(42);

	if (access("r__", F_OK) != 0) e(43);
	if (access("r__", R_OK) != 0) e(44);
	if (access("r__", W_OK) != -1) e(45);
	if (errno != EACCES) e(46);
	if (access("r__", X_OK) != -1) e(47);
	if (errno != EACCES) e(48);
	if (access("r__", R_OK | W_OK) != -1) e(49);
	if (errno != EACCES) e(50);
	if (access("r__", R_OK | X_OK) != -1) e(51);
	if (errno != EACCES) e(52);
	if (access("r__", W_OK | X_OK) != -1) e(53);
	if (errno != EACCES) e(54);
	if (access("r__", R_OK | W_OK | X_OK) != -1) e(55);
	if (errno != EACCES) e(56);

	if (access("_wx", F_OK) != 0) e(57);
	if (access("_wx", R_OK) != -1) e(58);
	if (errno != EACCES) e(59);
	if (access("_wx", W_OK) != 0) e(60);
	if (access("_wx", X_OK) != 0) e(61);
	if (access("_wx", R_OK | W_OK) != -1) e(62);
	if (errno != EACCES) e(63);
	if (access("_wx", R_OK | X_OK) != -1) e(64);
	if (errno != EACCES) e(65);
	if (access("_wx", W_OK | X_OK) != 0) e(66);
	if (access("_wx", R_OK | W_OK | X_OK) != -1) e(67);
	if (errno != EACCES) e(68);

	if (access("_w_", F_OK) != 0) e(69);
	if (access("_w_", R_OK) != -1) e(70);
	if (errno != EACCES) e(71);
	if (access("_w_", W_OK) != 0) e(72);
	if (access("_w_", X_OK) != -1) e(73);
	if (errno != EACCES) e(74);
	if (access("_w_", R_OK | W_OK) != -1) e(75);
	if (errno != EACCES) e(76);
	if (access("_w_", R_OK | X_OK) != -1) e(77);
	if (errno != EACCES) e(78);
	if (access("_w_", W_OK | X_OK) != -1) e(79);
	if (errno != EACCES) e(80);
	if (access("_w_", R_OK | W_OK | X_OK) != -1) e(81);
	if (errno != EACCES) e(82);

	if (access("__x", F_OK) != 0) e(83);
	if (access("__x", R_OK) != -1) e(84);
	if (errno != EACCES) e(85);
	if (access("__x", W_OK) != -1) e(86);
	if (errno != EACCES) e(87);
	if (access("__x", X_OK) != 0) e(88);
	if (access("__x", R_OK | W_OK) != -1) e(89);
	if (errno != EACCES) e(90);
	if (access("__x", R_OK | X_OK) != -1) e(91);
	if (errno != EACCES) e(92);
	if (access("__x", W_OK | X_OK) != -1) e(93);
	if (errno != EACCES) e(94);
	if (access("__x", R_OK | W_OK | X_OK) != -1) e(95);
	if (errno != EACCES) e(96);

	if (access("___", F_OK) != 0) e(97);
	if (access("___", R_OK) != -1) e(98);
	if (errno != EACCES) e(99);
	if (access("___", W_OK) != -1) e(100);
	if (errno != EACCES) e(101);
	if (access("___", X_OK) != -1) e(102);
	if (errno != EACCES) e(103);
	if (access("___", R_OK | W_OK) != -1) e(104);
	if (errno != EACCES) e(105);
	if (access("___", R_OK | X_OK) != -1) e(106);
	if (errno != EACCES) e(107);
	if (access("___", W_OK | X_OK) != -1) e(108);
	if (errno != EACCES) e(109);
	if (access("___", R_OK | W_OK | X_OK) != -1) e(110);
	if (errno != EACCES) e(111);
  }
  if (superuser) {
	/* Test root access don't test X_OK on [_r][_w]_ files. */
	if (access("rwx", F_OK) != 0) e(112);
	if (access("rwx", R_OK) != 0) e(113);
	if (access("rwx", W_OK) != 0) e(114);
	if (access("rwx", X_OK) != 0) e(115);
	if (access("rwx", R_OK | W_OK) != 0) e(116);
	if (access("rwx", R_OK | X_OK) != 0) e(117);
	if (access("rwx", W_OK | X_OK) != 0) e(118);
	if (access("rwx", R_OK | W_OK | X_OK) != 0) e(119);

	if (access("rw_", F_OK) != 0) e(120);
	if (access("rw_", R_OK) != 0) e(121);
	if (access("rw_", W_OK) != 0) e(122);
	if (access("rw_", R_OK | W_OK) != 0) e(123);

	if (access("r_x", F_OK) != 0) e(124);
	if (access("r_x", R_OK) != 0) e(125);
	if (access("r_x", W_OK) != 0) e(126);
	if (access("r_x", X_OK) != 0) e(127);
	if (access("r_x", R_OK | W_OK) != 0) e(128);
	if (access("r_x", R_OK | X_OK) != 0) e(129);
	if (access("r_x", W_OK | X_OK) != 0) e(130);
	if (access("r_x", R_OK | W_OK | X_OK) != 0) e(131);

	if (access("r__", F_OK) != 0) e(132);
	if (access("r__", R_OK) != 0) e(133);
	if (access("r__", W_OK) != 0) e(134);
	if (access("r__", R_OK | W_OK) != 0) e(135);

	if (access("_wx", F_OK) != 0) e(136);
	if (access("_wx", R_OK) != 0) e(137);
	if (access("_wx", W_OK) != 0) e(138);
	if (access("_wx", X_OK) != 0) e(139);
	if (access("_wx", R_OK | W_OK) != 0) e(140);
	if (access("_wx", R_OK | X_OK) != 0) e(141);
	if (access("_wx", W_OK | X_OK) != 0) e(142);
	if (access("_wx", R_OK | W_OK | X_OK) != 0) e(143);

	if (access("_w_", F_OK) != 0) e(144);
	if (access("_w_", R_OK) != 0) e(145);
	if (access("_w_", W_OK) != 0) e(146);
	if (access("_w_", R_OK | W_OK) != 0) e(147);

	if (access("__x", F_OK) != 0) e(148);
	if (access("__x", R_OK) != 0) e(149);
	if (access("__x", W_OK) != 0) e(150);
	if (access("__x", X_OK) != 0) e(151);
	if (access("__x", R_OK | W_OK) != 0) e(152);
	if (access("__x", R_OK | X_OK) != 0) e(153);
	if (access("__x", W_OK | X_OK) != 0) e(154);
	if (access("__x", R_OK | W_OK | X_OK) != 0) e(155);

	if (access("___", F_OK) != 0) e(156);
	if (access("___", R_OK) != 0) e(157);
	if (access("___", W_OK) != 0) e(158);
	if (access("___", R_OK | W_OK) != 0) e(159);
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
