/* POSIX test program (21).			Author: Andy Tanenbaum */

/* The following POSIX calls are tested:
 *
 *	rename(),  mkdir(),  rmdir()
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

#define ITERATIONS        1
#define MAX_ERROR 4

int subtest, errct;

_PROTOTYPE(int main, (int argc, char *argv []));
_PROTOTYPE(void test21a, (void));
_PROTOTYPE(void test21b, (void));
_PROTOTYPE(void test21c, (void));
_PROTOTYPE(void test21d, (void));
_PROTOTYPE(void test21e, (void));
_PROTOTYPE(void test21f, (void));
_PROTOTYPE(void test21g, (void));
_PROTOTYPE(void test21h, (void));
_PROTOTYPE(void test21i, (void));
_PROTOTYPE(void test21k, (void));
_PROTOTYPE(void test21l, (void));
_PROTOTYPE(void test21m, (void));
_PROTOTYPE(void test21n, (void));
_PROTOTYPE(void test21o, (void));
_PROTOTYPE(int get_link, (char *name));
_PROTOTYPE(void e, (int n));
_PROTOTYPE(void quit, (void));

int main(argc, argv)
int argc;
char *argv[];
{

  int i, m = 0xFFFF;

  sync();
  if (geteuid() == 0 || getuid() == 0) {
	printf("Test 21 cannot run as root; test aborted\n");
	exit(1);
  }

  if (argc == 2) m = atoi(argv[1]);
  printf("Test 21 ");
  fflush(stdout);

  system("rm -rf DIR_21; mkdir DIR_21");
  chdir("DIR_21");

  for (i = 0; i < ITERATIONS; i++) {
	if (m & 00001) test21a();
	if (m & 00002) test21b();
	if (m & 00004) test21c();
	if (m & 00010) test21d();
	if (m & 00020) test21e();
	if (m & 00040) test21f();
	if (m & 01000) test21g();
	if (m & 00200) test21h();
	if (m & 00400) test21i();
	if (m & 01000) test21k();
	if (m & 02000) test21l();
	if (m & 04000) test21m();
	if (m & 010000) test21n();
	if (m & 020000) test21o();
  }
  quit();
  return(-1);			/* impossible */
}

void test21a()
{
/* Test rename(). */

  int fd, fd2;
  char buf[PATH_MAX+1], buf1[PATH_MAX+1], buf2[PATH_MAX+1];
  struct stat stat1, stat2;

  subtest = 1;

  unlink("A1");			/* get rid of it if it exists */
  unlink("A2");			/* get rid of it if it exists */
  unlink("A3");			/* get rid of it if it exists */
  unlink("A4");			/* get rid of it if it exists */
  unlink("A5");			/* get rid of it if it exists */
  unlink("A6");			/* get rid of it if it exists */
  unlink("A7");			/* get rid of it if it exists */

  /* Basic test.  Rename A1 to A2 and then A2 to A3. */
  if ( (fd=creat("A1", 0666)) < 0) e(1);
  if (write(fd, buf, 20) != 20) e(2);
  if (close(fd) < 0) e(3);
  if (rename("A1", "A2") < 0) e(4);
  if ( (fd=open("A2", O_RDONLY)) < 0) e(5);
  if (rename("A2", "A3") < 0) e(6);
  if ( (fd2=open("A3", O_RDONLY)) < 0) e(7);
  if (close(fd) != 0) e(8);
  if (close(fd2) != 0) e(9);
  if (unlink("A3") != 0) e(10);

  /* Now get the absolute path name of the current directory using getcwd()
   * and use it to test RENAME using different combinations of relative and
   * absolute path names.
   */
  if (getcwd(buf, PATH_MAX) == (char *) NULL) e(11);
  if ( (fd = creat("A4", 0666)) < 0) e(12);
  if (write(fd, buf, 30) != 30) e(13);
  if (close(fd) != 0) e(14);
  strcpy(buf1, buf);
  strcat(buf1, "/A4");
  if (rename(buf1, "A5") != 0) e(15);	/* rename(absolute, relative) */
  if (access("A5", 6) != 0) e(16);	/* use access to see if file exists */
  strcpy(buf2, buf);
  strcat(buf2, "/A6");
  if (rename("A5", buf2) != 0) e(17);	/* rename(relative, absolute) */
  if (access("A6", 6) != 0) e(18);
  if (access(buf2, 6) != 0) e(19);
  strcpy(buf1, buf);
  strcat(buf1, "/A6");
  strcpy(buf2, buf);
  strcat(buf2, "/A7");
  if (rename(buf1, buf2) != 0) e(20);	/* rename(absolute, absolute) */
  if (access("A7", 6) != 0) e(21);
  if (access(buf2, 6) != 0) e(22);

  /* Try renaming using names like "./A8" */
  if (rename("A7", "./A8") != 0) e(23);
  if (access("A8", 6) != 0) e(24);
  if (rename("./A8", "A9") != 0) e(25);
  if (access("A9", 6) != 0) e(26);
  if (rename("./A9", "./A10") != 0) e(27);
  if (access("A10", 6) != 0) e(28);
  if (access("./A10", 6) != 0) e(29);
  if (unlink("A10") != 0) e(30);

  /* Now see if directories can be renamed. */
  if (system("rm -rf ?uzzy scsi") != 0) e(31);
  if (system("mkdir fuzzy") != 0) e(32);
  if (rename("fuzzy", "wuzzy") != 0) e(33);
  if ( (fd=creat("wuzzy/was_a_bear", 0666)) < 0) e(34);
  if (access("wuzzy/was_a_bear", 6) != 0) e(35);
  if (unlink("wuzzy/was_a_bear") != 0) e(36);
  if (close(fd) != 0) e(37);
  if (rename("wuzzy", "buzzy") != 0) e(38);
  if (system("rmdir buzzy") != 0) e(39);

  /* Now start testing the case that 'new' exists. */
  if ( (fd = creat("horse", 0666)) < 0) e(40);
  if ( (fd2 = creat("sheep", 0666)) < 0) e(41);
  if (write(fd, buf, PATH_MAX) != PATH_MAX) e(42);
  if (write(fd2, buf, 23) != 23) e(43);
  if (stat("horse", &stat1) != 0) e(44);
  if (rename("horse", "sheep") != 0) e(45);
  if (stat("sheep", &stat2) != 0) e(46);
  if (stat1.st_dev != stat2.st_dev) e(47);
  if (stat1.st_ino != stat2.st_ino) e(48);
  if (stat2.st_size != PATH_MAX) e(49);
  if (access("horse", 6) == 0) e(50);
  if (close(fd) != 0) e(51);
  if (close(fd2) != 0) e(52);
  if (rename("sheep", "sheep") != 0) e(53);
  if (unlink("sheep") != 0) e(54);

  /* Now try renaming something to a directory that already exists. */
  if (system("mkdir fuzzy wuzzy") != 0) e(55);
  if ( (fd = creat("fuzzy/was_a_bear", 0666)) < 0) e(56);
  if (close(fd) != 0) e(57);
  if (rename("fuzzy", "wuzzy") != 0) e(58);	/* 'new' is empty dir */
  if (system("mkdir scsi") != 0) e(59);
  if (rename("scsi", "wuzzy") == 0) e(60);	/* 'new' is full dir */
  if (errno != EEXIST && errno != ENOTEMPTY) e(61);

  /* Test 14 character names--always tricky. */
  if (rename("wuzzy/was_a_bear", "wuzzy/was_not_a_bear") != 0) e(62);
  if (access("wuzzy/was_not_a_bear", 6) != 0) e(63);
  if (rename("wuzzy/was_not_a_bear", "wuzzy/was_not_a_duck") != 0) e(64);
  if (access("wuzzy/was_not_a_duck", 6) != 0) e(65);
  if (rename("wuzzy/was_not_a_duck", "wuzzy/was_a_bird") != 0) e(65);
  if (access("wuzzy/was_a_bird", 6) != 0) e(66);

  /* Test moves between directories. */
  if (rename("wuzzy/was_a_bird", "beast") != 0) e(67);
  if (access("beast", 6) != 0) e(68);
  if (rename("beast", "wuzzy/was_a_cat") != 0) e(69);
  if (access("wuzzy/was_a_cat", 6) != 0) e(70);

  /* Test error conditions. 'scsi' and 'wuzzy/was_a_cat' exist now. */
  if (rename("wuzzy/was_a_cat", "wuzzy/was_a_dog") != 0) e(71);
  if (access("wuzzy/was_a_dog", 6) != 0) e(72);
  if (chmod("wuzzy", 0) != 0) e(73);

  errno = 0;
  if (rename("wuzzy/was_a_dog", "wuzzy/was_a_pig") != -1) e(74);
  if (errno != EACCES) e(75);

  errno = 0;
  if (rename("wuzzy/was_a_dog", "doggie") != -1) e(76);
  if (errno != EACCES) e(77);

  errno = 0;
  if ( (fd = creat("beast", 0666)) < 0) e(78);
  if (close(fd) != 0) e(79);
  if (rename("beast", "wuzzy/was_a_twit") != -1) e(80);
  if (errno != EACCES) e(81);

  errno = 0;
  if (rename("beast", "wuzzy") != -1) e(82);
  if (errno != EISDIR) e(83);

  errno = 0;
  if (rename("beest", "baste") != -1) e(84);
  if (errno != ENOENT) e(85);

  errno = 0;
  if (rename("wuzzy", "beast") != -1) e(86);
  if (errno != ENOTDIR) e(87);

  /* Test prefix rule. */
  errno = 0;
  if (chmod("wuzzy", 0777) != 0) e(88);
  if (unlink("wuzzy/was_a_dog") != 0) e(89);
  strcpy(buf1, buf);
  strcat(buf1, "/wuzzy");
  if (rename(buf, buf1) != -1) e(90);
  if (errno != EINVAL) e(91);

  if (system("rm -rf wuzzy beast scsi") != 0) e(92);
}

  

void test21b()
{
/* Test mkdir() and rmdir(). */

  int i;
  char name[3];
  struct stat statbuf;

  subtest = 2;

  /* Simple stuff. */
  if (mkdir("D1", 0700) != 0) e(1);
  if (stat("D1", &statbuf) != 0) e(2);
  if (!S_ISDIR(statbuf.st_mode)) e(3);
  if ( (statbuf.st_mode & 0777) != 0700) e(4);
  if (rmdir("D1") != 0) e(5);

  /* Make and remove 40 directories.  By doing so, the directory has to
   * grow to 2 blocks.  That presents plenty of opportunity for bugs.
   */
  name[0] = 'D';
  name[2] = '\0';
  for (i = 0; i < 40; i++) {
	name[1] = 'A' + i;
	if (mkdir(name, 0700 + i%7) != 0) e(10+i);	/* for simplicity */
  }
  for (i = 0; i < 40; i++) {
	name[1] = 'A' + i;
	if (rmdir(name) != 0) e(50+i);
  }
}

void test21c()
{
/* Test mkdir() and rmdir(). */

  subtest = 3;

  if (mkdir("D1", 0777) != 0) e(1);
  if (mkdir("D1/D2", 0777) != 0) e(2);
  if (mkdir("D1/D2/D3", 0777) != 0) e(3);
  if (mkdir("D1/D2/D3/D4", 0777) != 0) e(4);
  if (mkdir("D1/D2/D3/D4/D5", 0777) != 0) e(5);
  if (mkdir("D1/D2/D3/D4/D5/D6", 0777) != 0) e(6);
  if (mkdir("D1/D2/D3/D4/D5/D6/D7", 0777) != 0) e(7);
  if (mkdir("D1/D2/D3/D4/D5/D6/D7/D8", 0777) != 0) e(8);
  if (mkdir("D1/D2/D3/D4/D5/D6/D7/D8/D9", 0777) != 0) e(9);
  if (access("D1/D2/D3/D4/D5/D6/D7/D8/D9", 7) != 0) e(10);
  if (rmdir("D1/D2/D3/D4/D5/D6/D7/D8/D9") != 0) e(11);
  if (rmdir("D1/D2/D3/D4/D5/D6/D7/D8") != 0) e(12);
  if (rmdir("D1/D2/D3/D4/D5/D6/D7") != 0) e(13);
  if (rmdir("D1/D2/D3/D4/D5/D6") != 0) e(11);
  if (rmdir("D1/D2/D3/D4/D5") != 0) e(13);
  if (rmdir("D1/D2/D3/D4") != 0) e(14);
  if (rmdir("D1/D2/D3") != 0) e(15);
  if (rmdir("D1/D2") != 0) e(16);
  if (rmdir("D1") != 0) e(17);
}
  
void test21d()
{
/* Test making directories with files and directories in them. */

  int fd1, fd2, fd3, fd4, fd5, fd6, fd7, fd8, fd9;

  subtest = 4;
  
  if (mkdir("D1", 0777) != 0) e(1);
  if (mkdir("D1/D2", 0777) != 0) e(2);
  if (mkdir("./D1/D3", 0777) != 0) e(3);
  if (mkdir("././D1/D4", 0777) != 0) e(4);
  if ( (fd1 = creat("D1/D2/x", 0700)) < 0) e(5);
  if ( (fd2 = creat("D1/D2/y", 0700)) < 0) e(6);
  if ( (fd3 = creat("D1/D2/z", 0700)) < 0) e(7);
  if ( (fd4 = creat("D1/D3/x", 0700)) < 0) e(8);
  if ( (fd5 = creat("D1/D3/y", 0700)) < 0) e(9);
  if ( (fd6 = creat("D1/D3/z", 0700)) < 0) e(10);
  if ( (fd7 = creat("D1/D4/x", 0700)) < 0) e(11);
  if ( (fd8 = creat("D1/D4/y", 0700)) < 0) e(12);
  if ( (fd9 = creat("D1/D4/z", 0700)) < 0) e(13);
  if (unlink("D1/D2/z") != 0) e(14);
  if (unlink("D1/D2/y") != 0) e(15);
  if (unlink("D1/D2/x") != 0) e(16);
  if (unlink("D1/D3/x") != 0) e(17);
  if (unlink("D1/D3/z") != 0) e(18);
  if (unlink("D1/D3/y") != 0) e(19);
  if (unlink("D1/D4/y") != 0) e(20);
  if (unlink("D1/D4/z") != 0) e(21);
  if (unlink("D1/D4/x") != 0) e(22);
  if (rmdir("D1/D2") != 0) e(23);
  if (rmdir("D1/D3") != 0) e(24);
  if (rmdir("D1/D4") != 0) e(25);
  if (rmdir("D1") != 0) e(26);
  if (close(fd1) != 0) e(27);
  if (close(fd2) != 0) e(28);
  if (close(fd3) != 0) e(29);
  if (close(fd4) != 0) e(30);
  if (close(fd5) != 0) e(31);
  if (close(fd6) != 0) e(32);
  if (close(fd7) != 0) e(33);
  if (close(fd8) != 0) e(34);
  if (close(fd9) != 0) e(35);

}

void test21e()
{
/* Test error conditions. */
  
  subtest = 5;

  if (mkdir("D1", 0677) != 0) e(1);
  errno = 0;
  if (mkdir("D1/D2", 0777) != -1) e(2);
  if (errno != EACCES) e(3);
  if (chmod ("D1", 0577) != 0) e(4);
  errno = 0;
  if (mkdir("D1/D2", 0777) != -1) e(5);
  if (errno != EACCES) e(6);
  if (chmod ("D1", 0777) != 0) e(7);
  errno = 0;
  if (mkdir("D1", 0777) != -1) e(8);
  if (errno != EEXIST) e(9);
#if NAME_MAX == 14
  if (mkdir("D1/ABCDEFGHIJKLMNOPQRSTUVWXYZ", 0777) != 0) e(10);
  if (access("D1/ABCDEFGHIJKLMN", 7 ) != 0) e(11);
  if (rmdir("D1/ABCDEFGHIJKLMNOPQ") != 0) e(12);
  if (access("D1/ABCDEFGHIJKLMN", 7 ) != -1) e(13);
#endif
  errno = 0;
  if (mkdir("D1/D2/x", 0777) != -1) e(14);
  if (errno != ENOENT) e(15);

  /* A particularly nasty test is when the parent has mode 0.  Although
   * this is unlikely to work, it had better not muck up the file system
   */
  if (mkdir("D1/D2", 0777) != 0) e(16);
  if (chmod("D1", 0) != 0) e(17);
  errno = 0;
  if (rmdir("D1/D2") != -1) e(18);
  if (errno != EACCES) e(19);
  if (chmod("D1", 0777) != 0) e(20);
  if (rmdir("D1/D2") != 0) e(21);
  if (rmdir("D1") != 0) e(22);
}

void test21f()
{
/* The rename() function affects the link count of all the files and
 * directories it goes near.  Test to make sure it gets everything ok.
 * There are four cases:
 *
 *   1. rename("d1/file1", "d1/file2");	- rename file without moving it
 *   2. rename("d1/file1", "d2/file2");	- move a file to another dir
 *   3. rename("d1/dir1", "d2/dir2");	- rename a dir without moving it
 *   4. rename("d1/dir1", "d2/dir2");	- move a dir to another dir
 *
 * Furthermore, a distinction has to be made when the target file exists
 * and when it does not exist, giving 8 cases in all.
 */

  int fd, D1_before, D1_after, x_link, y_link;

  /* Test case 1: renaming a file within the same directory. */
  subtest = 6;
  if (mkdir("D1", 0777) != 0) e(1);
  if ( (fd = creat("D1/x", 0777)) < 0) e(2);
  if (close(fd) != 0) e(3);
  D1_before = get_link("D1");
  x_link = get_link("D1/x");
  if (rename("D1/x", "D1/y") != 0) e(4);
  y_link = get_link("D1/y");
  D1_after = get_link("D1");
  if (D1_before != 2) e(5);
  if (D1_after != 2) e(6);
  if (x_link != 1) e(7);
  if (y_link != 1) e(8);
  if (access("D1/y", 7) != 0) e(9);
  if (unlink("D1/y") != 0) e(10);
  if (rmdir("D1") != 0) e(11);
}

void test21g()
{
  int fd, D1_before, D1_after, D2_before, D2_after, x_link, y_link;

  /* Test case 2: move a file to a new directory. */
  subtest = 7;
  if (mkdir("D1", 0777) != 0) e(1);
  if (mkdir("D2", 0777) != 0) e(2);
  if ( (fd = creat("D1/x", 0777)) < 0) e(3);
  if (close(fd) != 0) e(4);
  D1_before = get_link("D1");
  D2_before = get_link("D2");
  x_link = get_link("D1/x");
  if (rename("D1/x", "D2/y") != 0) e(5);
  y_link = get_link("D2/y");
  D1_after = get_link("D1");
  D2_after = get_link("D2");
  if (D1_before != 2) e(6);
  if (D2_before != 2) e(7);
  if (D1_after != 2) e(8);
  if (D2_after != 2) e(9);
  if (x_link != 1) e(10);
  if (y_link != 1) e(11);
  if (access("D2/y", 7) != 0) e(12);
  if (unlink("D2/y") != 0) e(13);
  if (rmdir("D1") != 0) e(14);
  if (rmdir("D2") != 0) e(15);
}

void test21h()
{
  int D1_before, D1_after, x_link, y_link;

  /* Test case 3: renaming a directory within the same directory. */
  subtest = 8;
  if (mkdir("D1", 0777) != 0) e(1);
  if (mkdir("D1/X", 0777) != 0) e(2);
  D1_before = get_link("D1");
  x_link = get_link("D1/X");
  if (rename("D1/X", "D1/Y") != 0) e(3);
  y_link = get_link("D1/Y");
  D1_after = get_link("D1");
  if (D1_before != 3) e(4);
  if (D1_after != 3) e(5);
  if (x_link != 2) e(6);
  if (y_link != 2) e(7);
  if (access("D1/Y", 7) != 0) e(8);
  if (rmdir("D1/Y") != 0) e(9);
  if (get_link("D1") != 2) e(10);
  if (rmdir("D1") != 0) e(11);
}

void test21i()
{
  int D1_before, D1_after, D2_before, D2_after, x_link, y_link;

  /* Test case 4: move a directory to a new directory. */
  subtest = 9;
  if (mkdir("D1", 0777) != 0) e(1);
  if (mkdir("D2", 0777) != 0) e(2);
  if (mkdir("D1/X", 0777) != 0) e(3);
  D1_before = get_link("D1");
  D2_before = get_link("D2");
  x_link = get_link("D1/X");
  if (rename("D1/X", "D2/Y") != 0) e(4);
  y_link = get_link("D2/Y");
  D1_after = get_link("D1");
  D2_after = get_link("D2");
  if (D1_before != 3) e(5);
  if (D2_before != 2) e(6);
  if (D1_after != 2) e(7);
  if (D2_after != 3) e(8);
  if (x_link != 2) e(9);
  if (y_link != 2) e(10);
  if (access("D2/Y", 7) != 0) e(11);
  if (rename("D2/Y", "D1/Z") != 0) e(12);
  if (get_link("D1") != 3) e(13);
  if (get_link("D2") != 2) e(14);
  if (rmdir("D1/Z") != 0) e(15);
  if (get_link("D1") != 2) e(16);
  if (rmdir("D1") != 0) e(17);
  if (rmdir("D2") != 0) e(18);
}

void test21k()
{
/* Now test the same 4 cases, except when the target exists. */

  int fd, D1_before, D1_after, x_link, y_link;

  /* Test case 5: renaming a file within the same directory. */
  subtest = 10;
  if (mkdir("D1", 0777) != 0) e(1);
  if ( (fd = creat("D1/x", 0777)) < 0) e(2);
  if (close(fd) != 0) e(3);
  if ( (fd = creat("D1/y", 0777)) < 0) e(3);
  if (close(fd) != 0) e(4);
  D1_before = get_link("D1");
  x_link = get_link("D1/x");
  if (rename("D1/x", "D1/y") != 0) e(5);
  y_link = get_link("D1/y");
  D1_after = get_link("D1");
  if (D1_before != 2) e(6);
  if (D1_after != 2) e(7);
  if (x_link != 1) e(8);
  if (y_link != 1) e(9);
  if (access("D1/y", 7) != 0) e(10);
  if (unlink("D1/y") != 0) e(11);
  if (rmdir("D1") != 0) e(12);
}

void test21l()
{
  int fd, D1_before, D1_after, D2_before, D2_after, x_link, y_link;

  /* Test case 6: move a file to a new directory. */
  subtest = 11;
  if (mkdir("D1", 0777) != 0) e(1);
  if (mkdir("D2", 0777) != 0) e(2);
  if ( (fd = creat("D1/x", 0777)) < 0) e(3);
  if (close(fd) != 0) e(4);
  if ( (fd = creat("D2/y", 0777)) < 0) e(5);
  if (close(fd) != 0) e(6);
  D1_before = get_link("D1");
  D2_before = get_link("D2");
  x_link = get_link("D1/x");
  if (rename("D1/x", "D2/y") != 0) e(7);
  y_link = get_link("D2/y");
  D1_after = get_link("D1");
  D2_after = get_link("D2");
  if (D1_before != 2) e(8);
  if (D2_before != 2) e(9);
  if (D1_after != 2) e(10);
  if (D2_after != 2) e(11);
  if (x_link != 1) e(12);
  if (y_link != 1) e(13);
  if (access("D2/y", 7) != 0) e(14);
  if (unlink("D2/y") != 0) e(15);
  if (rmdir("D1") != 0) e(16);
  if (rmdir("D2") != 0) e(17);
}

void test21m()
{
  int D1_before, D1_after, x_link, y_link;

  /* Test case 7: renaming a directory within the same directory. */
  subtest = 12;
  if (mkdir("D1", 0777) != 0) e(1);
  if (mkdir("D1/X", 0777) != 0) e(2);
  if (mkdir("D1/Y", 0777) != 0) e(3);
  D1_before = get_link("D1");
  x_link = get_link("D1/X");
  if (rename("D1/X", "D1/Y") != 0) e(4);
  y_link = get_link("D1/Y");
  D1_after = get_link("D1");
  if (D1_before != 4) e(5);
  if (D1_after != 3) e(6);
  if (x_link != 2) e(7);
  if (y_link != 2) e(8);
  if (access("D1/Y", 7) != 0) e(9);
  if (rmdir("D1/Y") != 0) e(10);
  if (get_link("D1") != 2) e(11);
  if (rmdir("D1") != 0) e(12);
}

void test21n()
{
  int D1_before, D1_after, D2_before, D2_after, x_link, y_link;

  /* Test case 8: move a directory to a new directory. */
  subtest = 13;
  if (mkdir("D1", 0777) != 0) e(1);
  if (mkdir("D2", 0777) != 0) e(2);
  if (mkdir("D1/X", 0777) != 0) e(3);
  if (mkdir("D2/Y", 0777) != 0) e(4);
  D1_before = get_link("D1");
  D2_before = get_link("D2");
  x_link = get_link("D1/X");
  if (rename("D1/X", "D2/Y") != 0) e(5);
  y_link = get_link("D2/Y");
  D1_after = get_link("D1");
  D2_after = get_link("D2");
  if (D1_before != 3) e(6);
  if (D2_before != 3) e(7);
  if (D1_after != 2) e(8);
  if (D2_after != 3) e(9);
  if (x_link != 2) e(10);
  if (y_link != 2) e(11);
  if (access("D2/Y", 7) != 0) e(12);
  if (rename("D2/Y", "D1/Z") != 0) e(13);
  if (get_link("D1") != 3) e(14);
  if (get_link("D2") != 2) e(15);
  if (rmdir("D1/Z") != 0) e(16);
  if (get_link("D1") != 2) e(17);
  if (rmdir("D1") != 0) e(18);
  if (rmdir("D2") != 0) e(19);
}

void test21o()
{
  /* Test trying to remove . and .. */
  subtest = 14;
  if (mkdir("D1", 0777) != 0) e(1);
  if (chdir("D1") != 0) e(2);
  if (rmdir(".") == 0) e(3);
  if (rmdir("..") == 0) e(4);
  if (mkdir("D2", 0777) != 0) e(5);
  if (mkdir("D3", 0777) != 0) e(6);
  if (mkdir("D4", 0777) != 0) e(7);
  if (rmdir("D2/../D3/../D4") != 0) e(8);	/* legal way to remove D4 */
  if (rmdir("D2/../D3/../D2/..") == 0) e(9);	/* removing self is illegal */
  if (rmdir("D2/../D3/../D2/../..") == 0) e(10);/* removing parent is illegal*/
  if (rmdir("../D1/../D1/D3") != 0) e(11);	/* legal way to remove D3 */
  if (rmdir("./D2/../D2") != 0) e(12);		/* legal way to remove D2 */
  if (chdir("..") != 0) e(13);
  if (rmdir("D1") != 0) e(14);
}

int get_link(name)
char *name;
{
  struct stat statbuf;

  if (stat(name, &statbuf) != 0) {
	printf("Unable to stat %s\n", name);
	errct++;
	return(-1);
  }
  return(statbuf.st_nlink);
}

void e(n)
int n;
{
  int err_num = errno;		/* save errno in case printf clobbers it */

  printf("Subtest %d,  error %d  errno=%d  ", subtest, n, errno);
  errno = err_num;		/* restore errno, just in case */
  perror("");
  if (errct++ > MAX_ERROR) {
	printf("Too many errors; test aborted\n");
	chdir("..");
	system("rm -rf DIR*");
	exit(1);
  }
}

void quit()
{
  chdir("..");
  system("rm -rf DIR*");

  if (errct == 0) {
	printf("ok\n");
	exit(0);
  } else {
	printf("%d errors\n", errct);
	exit(1);
  }
}
