/* POSIX test program (39).			Author: Andy Tanenbaum */

/* The following POSIX calls are tested:
 *
 *	opendir()
 *	readdir()
 *	rewinddir()
 *	closedir()
 *	chdir()
 *	getcwd()
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <utime.h>
#include <stdio.h>

#define DIR_NULL (DIR*) NULL
#define ITERATIONS         3	/* LINK_MAX is high, so time consuming */
#define MAX_FD           100	/* must be large enough to cause error */
#define BUF_SIZE PATH_MAX+20
#define ERR_CODE          -1	/* error return */
#define RD_BUF           200
#define MAX_ERROR          4

char str[] = {"The time has come the walrus said to talk of many things.\n"};
char str2[] = {"Of ships and shoes and sealing wax, of cabbages and kings.\n"};
char str3[] = {"Of why the sea is boiling hot and whether pigs have wings\n"};

int subtest, errct;

_PROTOTYPE(int main, (int argc, char *argv []));
_PROTOTYPE(void test39a, (void));
_PROTOTYPE(void checkdir, (DIR *dirp, int t));
_PROTOTYPE(void test39b, (void));
_PROTOTYPE(void test39c, (void));
_PROTOTYPE(void test39d, (void));
_PROTOTYPE(void test39e, (void));
_PROTOTYPE(void test39f, (void));
_PROTOTYPE(void test39g, (void));
_PROTOTYPE(void test39h, (void));
_PROTOTYPE(void test39i, (void));
_PROTOTYPE(void test39j, (void));
_PROTOTYPE(void e, (int n));
_PROTOTYPE(void quit, (void));

int main(argc, argv)
int argc;
char *argv[];
{

  int i, m = 0xFFFF;

  sync();
  if (geteuid() == 0 || getuid() == 0) {
	printf("Test 39 cannot run as root; test aborted\n");
	exit(1);
  }

  if (argc == 2) m = atoi(argv[1]);
  printf("Test 39 ");
  fflush(stdout);

  system("rm -rf DIR_39; mkdir DIR_39");
  chdir("DIR_39");

  for (i = 0; i < ITERATIONS; i++) {
	if (m & 00001) test39a();	/* test for correct operation */
	if (m & 00002) test39b();	/* test general error handling */
	if (m & 00004) test39c();	/* test for EMFILE error */
	if (m & 00010) test39d();	/* test chdir() and getcwd() */
	if (m & 00020) test39e();	/* test open() */
	if (m & 00040) test39f();	/* test umask(), stat(), fstat() */
	if (m & 00100) test39g();	/* test link() and unlink() */
	if (m & 00200) test39h();	/* test access() */
	if (m & 00400) test39i();	/* test chmod() and chown() */
	if (m & 01000) test39j();	/* test utime() */
  }
  quit();
  return(-1);			/* impossible */
}

void test39a()
{
/* Subtest 1. Correct operation */

  int f1, f2, f3, f4, f5;
  DIR *dirp;

  /* Remove any residue of previous tests. */
  subtest = 1;

  system("rm -rf foo");

  /* Create a directory foo with 5 files in it. */
  mkdir("foo", 0777);
  if ((f1 = creat("foo/f1", 0666)) < 0) e(1);
  if ((f2 = creat("foo/f2", 0666)) < 0) e(2);
  if ((f3 = creat("foo/f3", 0666)) < 0) e(3);
  if ((f4 = creat("foo/f4", 0666)) < 0) e(4);
  if ((f5 = creat("foo/f5", 0666)) < 0) e(5);

  /* Now remove 2 files to create holes in the directory. */
  if (unlink("foo/f2") < 0) e(6);
  if (unlink("foo/f4") < 0) e(7);

  /* Close the files. */
  close(f1);
  close(f2);
  close(f3);
  close(f4);
  close(f5);

  /* Open the directory. */
  dirp = opendir("./foo");
  if (dirp == DIR_NULL) e(6);

  /* Read the 5 files from it. */
  checkdir(dirp, 2); 

  /* Rewind dir and test again. */
  rewinddir(dirp);
  checkdir(dirp, 3);

  /* We're done.  Close the directory stream. */
  if (closedir(dirp) < 0) e(7);

  /* Remove dir for next time. */
  system("rm -rf foo");
}

void checkdir(dirp, t)
DIR *dirp;			/* poinrter to directory stream */
int t;				/* subtest number to use */
{

  int i, f1, f2, f3, f4, f5, dot, dotdot, subt;
  struct dirent *d;
  char *s;

  /* Save subtest number */
  subt = subtest;
  subtest = t;

  /* Clear the counters. */
  f1 = 0;
  f2 = 0;
  f3 = 0;
  f4 = 0;
  f5 = 0;
  dot = 0;
  dotdot = 0;

  /* Read the directory.  It should contain 5 entries, ".", ".." and 3
   * files. */
  for (i = 0; i < 5; i++) {
	d = readdir(dirp);
	if (d == (struct dirent *) NULL) {
		e(1);
		subtest = subt;	/* restore subtest number */
		return;
	}
	s = d->d_name;
	if (strcmp(s, ".") == 0) dot++;
	if (strcmp(s, "..") == 0) dotdot++;
	if (strcmp(s, "f1") == 0) f1++;
	if (strcmp(s, "f2") == 0) f2++;
	if (strcmp(s, "f3") == 0) f3++;
	if (strcmp(s, "f4") == 0) f4++;
	if (strcmp(s, "f5") == 0) f5++;
  }

  /* Check results. */
  d = readdir(dirp);
  if (d != (struct dirent *) NULL) e(2);
  if (f1 != 1 || f3 != 1 || f5 != 1) e(3);
  if (f2 != 0 || f4 != 0) e(4);
  if (dot != 1 || dotdot != 1) e(5);
  subtest = subt;
  return;
}

void test39b()
{
/* Subtest 4.  Test error handling. */

  int fd;
  DIR *dirp;

  subtest = 4;

  if (opendir("foo/xyz/---") != DIR_NULL) e(1);
  if (errno != ENOENT) e(2);
  if (mkdir("foo", 0777) < 0) e(3);
  if (chmod("foo", 0) < 0) e(4);
  if (opendir("foo/xyz/--") != DIR_NULL) e(5);
  if (errno != EACCES) e(6);
  if (chmod("foo", 0777) != 0) e(7);
  if (rmdir("foo") != 0) e(8);
  if ((fd = creat("abc", 0666)) < 0) e(9);
  if (close(fd) < 0) e(10);
  if (opendir("abc/xyz") != DIR_NULL) e(11);
  if (errno != ENOTDIR) e(12);
  if ((dirp = opendir(".")) == DIR_NULL) e(13);
  if (closedir(dirp) != 0) e(14);
  if (unlink("abc") != 0) e(15);

}

void test39c()
{
/* Subtest 5.  See what happens if we open too many directory streams. */

  int i, j;
  DIR *dirp[MAX_FD];

  subtest = 5;

  for (i = 0; i < MAX_FD; i++) {
	dirp[i] = opendir(".");
	if (dirp[i] == (DIR *) NULL) {
		/* We have hit the limit. */
		if (errno != EMFILE && errno != ENOMEM) e(1);
		for (j = 0; j < i; j++) {
			if (closedir(dirp[j]) != 0) e(2);	/* close */
		}
		return;
	}
  }

  /* Control should never come here.  This is an error. */
  e(3);
  for (i = 0; i < MAX_FD; i++) closedir(dirp[i]);	/* don't check */
}

void test39d()
{
/* Test chdir and getcwd(). */

  int fd;
  char *s;
  char base[BUF_SIZE], buf2[BUF_SIZE], tmp[BUF_SIZE];

  subtest = 6;

  if (getcwd(base, BUF_SIZE) == (char *) NULL) e(1); /* get test dir's path */
  if (system("rm -rf Dir") != 0) e(2);	/* remove residue of previous test */
  if (mkdir("Dir", 0777) < 0) e(3); 	/* create directory called "Dir" */

  /* Change to Dir and verify that it worked. */
  if (chdir("Dir") < 0) e(4);	/* go to Dir */
  s = getcwd(buf2, BUF_SIZE);	/* get full path of Dir */
  if (s == (char *) NULL) e(5);	/* check for error return */
  if (s != buf2) e(6);		/* if successful, first arg is returned */
  strcpy(tmp, base);		/* concatenate base name and "/Dir" */
  strcat(tmp, "/");
  strcat(tmp, "Dir");
  if (strcmp(tmp, s) != 0) e(7);

  /* Change to ".." and verify that it worked. */
  if (chdir("..") < 0) e(8);
  if (getcwd(buf2, BUF_SIZE) != buf2) e(9);
  if (strcmp(buf2, base) != 0) e(10);

  /* Now make calls that do nothing, but do it in a strange way. */
  if (chdir("Dir/..") < 0) e(11);
  if (getcwd(buf2, BUF_SIZE) != buf2) e(12);
  if (strcmp(buf2, base) != 0) e(13);

  if (chdir("Dir/../Dir/..") < 0) e(14);
  if (getcwd(buf2, BUF_SIZE) != buf2) e(15);
  if (strcmp(buf2, base) != 0) e(16);

  if (chdir("Dir/../Dir/../Dir/../Dir/../Dir/../Dir/../Dir/..") < 0) e(17);
  if (getcwd(buf2, BUF_SIZE) != buf2) e(18);
  if (strcmp(buf2, base) != 0) e(19);

  /* Make Dir unreadable and unsearchable.  Check error message. */
  if (chmod("Dir", 0) < 0) e(20);
  if (chdir("Dir") >= 0) e(21);
  if (errno != EACCES) e(22);

  /* Check error message for bad path. */
  if (chmod("Dir", 0777) < 0) e(23);
  if (chdir("Dir/x/y") != ERR_CODE) e(24);
  if (errno != ENOENT) e(25);

  if ( (fd=creat("Dir/x", 0777)) < 0) e(26);
  if (close(fd) != 0) e(27);
  if (chdir("Dir/x/y") != ERR_CODE) e(28);
  if (errno != ENOTDIR) e(29);  

  /* Check empty string. */
  if (chdir("") != ERR_CODE) e(30);
  if (errno != ENOENT) e(31);

  /* Remove the directory. */
  if (unlink("Dir/x") != 0) e(32);
  if (system("rmdir Dir") != 0) e(33);
}

void test39e()
{
/* Test open. */

  int fd, bytes, bytes2;
  char buf[RD_BUF];

  subtest = 7;

  unlink("T39");		/* get rid of it in case it exists */

  /* Create a test file. */
  bytes = strlen(str);
  bytes2 = strlen(str2);
  if ((fd = creat("T39", 0777)) < 0) e(1);
  if (write(fd, str, bytes) != bytes) e(2);	/* T39 now has 'bytes' bytes */
  if (close(fd) != 0) e(3);

  /* Test opening a file with O_RDONLY. */
  if ((fd = open("T39", O_RDONLY)) < 0) e(4);
  buf[0] = '\0';
  if (read(fd, buf, RD_BUF) != bytes) e(5);
  if (strncmp(buf, str, bytes) != 0) e(6);
  if (close(fd) < 0) e(7);

  /* Test the same thing, only with O_RDWR now. */
  if ((fd = open("T39", O_RDWR)) < 0) e(8);
  buf[0] = '\0';
  if (read(fd, buf, RD_BUF) != bytes) e(9);
  if (strncmp(buf, str, bytes) != 0) e(10);
  if (close(fd) < 0) e(11);

  /* Try opening and reading with O_WRONLY.  It should fail. */
  if ((fd = open("T39", O_WRONLY)) < 0) e(12);
  buf[0] = '\0';
  if (read(fd, buf, RD_BUF) >= 0) e(13);
  if (close(fd) != 0) e(14);

  /* Test O_APPEND. */
  if ((fd = open("T39", O_RDWR | O_APPEND)) < 0) e(15);
  if (lseek(fd, 0L, SEEK_SET) < 0) e(16);	/* go to start of file */
  if ( write(fd, str2, bytes2) != bytes2) e(17); /* write at start of file */
  if (lseek(fd, 0L, SEEK_SET) < 0) e(18); 	/* go back to start again */
  if (read(fd, buf, RD_BUF) != bytes + bytes2) e(19); /* read whole file */
  if (strncmp(buf, str, bytes) != 0) e(20);
  if (close(fd) != 0) e(21);

  /* Get rid of the file. */
  if (unlink("T39") < 0) e(22);
}

void test39f()
{
/* Test stat, fstat, umask. */
  int i, fd;
  mode_t m1;
  struct stat stbuf1, stbuf2;
  time_t t, t1;

  subtest = 8;

  m1 = umask(~0777);
  if (system("rm -rf foo xxx") != 0) e(1);
  if ((fd = creat("foo", 0777)) < 0) e(2);
  if (stat("foo", &stbuf1) < 0) e(3);
  if (fstat(fd, &stbuf2) < 0) e(4);
  if (stbuf1.st_mode != stbuf2.st_mode) e(5);
  if (stbuf1.st_ino != stbuf2.st_ino) e(6);
  if (stbuf1.st_dev != stbuf2.st_dev) e(7);
  if (stbuf1.st_nlink != stbuf2.st_nlink) e(8);
  if (stbuf1.st_uid != stbuf2.st_uid) e(9);
  if (stbuf1.st_gid != stbuf2.st_gid) e(10);
  if (stbuf1.st_size != stbuf2.st_size) e(11);
  if (stbuf1.st_atime != stbuf2.st_atime) e(12);
  if (stbuf1.st_mtime != stbuf2.st_mtime) e(13);
  if (stbuf1.st_ctime != stbuf2.st_ctime) e(14);

  if (!S_ISREG(stbuf1.st_mode)) e(15);
  if (S_ISDIR(stbuf1.st_mode)) e(16);
  if (S_ISCHR(stbuf1.st_mode)) e(17);
  if (S_ISBLK(stbuf1.st_mode)) e(18);
  if (S_ISFIFO(stbuf1.st_mode)) e(19);

  if ((stbuf1.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO)) != 0777) e(20);
  if (stbuf1.st_nlink != 1) e(21);
  if (stbuf1.st_uid != getuid()) e(22);
  if (stbuf1.st_gid != getgid()) e(23);
  if (stbuf1.st_size != 0L) e(24);

  /* First unlink, then close -- harder test */
  if (unlink("foo") < 0) e(25);
  if (close(fd) < 0) e(26);

  /* Now try umask a bit more. */
  fd = 0;
  if ((i = umask(~0704)) != 0) e(27);
  if ((fd = creat("foo", 0777)) < 0) e(28);
  if (stat("foo", &stbuf1) < 0) e(29);
  if (fstat(fd, &stbuf2) < 0) e(30);
  if (stbuf1.st_mode != stbuf2.st_mode) e(31);
  if ((stbuf1.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO)) != 0704) e(32);

  /* First unlink, then close -- harder test */
  if (unlink("foo") < 0) e(33);
  if (close(fd) < 0) e(34);
  if (umask(m1) != 073) e(35);

  /* Test some errors. */
  if (system("mkdir Dir; date >Dir/x; chmod 666 Dir") != 0) e(36);
  if (stat("Dir/x", &stbuf1) >= 0) e(37);
  if (errno != EACCES) e(38);
  if (stat("......", &stbuf1) >= 0) e(39);
  if (errno != ENOENT) e(40);
  if (stat("", &stbuf1) >= 0) e(41);
  if (errno != ENOENT) e(42);
  if (stat("xxx/yyy/zzz", &stbuf1) >= 0) e(43);
  if (errno != ENOENT) e(44);
  if (fstat(10000, &stbuf1) >= 0) e(45);
  if (errno != EBADF) e(46);
  if (chmod("Dir", 0777) != 0) e(47);
  if (system("rm -rf foo Dir") != 0) e(48);

  /* See if time looks reasonable. */
  errno = 0;
  t = time(&t1);		/* current time */
  if (t < 650000000L) e(49);	/* 650000000 is Sept. 1990 */
  unlink("T39f");
  fd = creat("T39f", 0777);
  if (fd < 0) e(50);
  if (close(fd) < 0) e(51);
  if (stat("T39f", &stbuf1) < 0) e(52);
  if (stbuf1.st_mtime < t) e(53);
  if (unlink("T39f") < 0) e(54);
}

void test39g()
{
/* Test link and unlink. */
  int i, fd;
  struct stat stbuf;
  char name[20];

  subtest = 9;

  if (system("rm -rf L? L?? Dir; mkdir Dir") != 0) e(1);
  if ( (fd = creat("L1", 0666)) < 0) e(2);
  if (fstat(fd, &stbuf) != 0) e(3);
  if (stbuf.st_nlink != 1) e(4);
  if (link("L1", "L2") != 0) e(5);
  if (fstat(fd, &stbuf) != 0) e(6);
  if (stbuf.st_nlink != 2) e(7);
  if (unlink("L2") != 0) e(8);
  if (link("L1", "L2") != 0) e(9);
  if (unlink("L1") != 0) e(10);
  if (close(fd) != 0) e(11);

  /* L2 exists at this point. */
  if ( (fd = creat("L1", 0666)) < 0) e(12);
  if (stat("L1", &stbuf) != 0) e(13);
  if (stbuf.st_nlink != 1) e(14);
  if (link("L1", "Dir/L2") != 0) e(15);
  if (stat("L1", &stbuf) != 0) e(16);
  if (stbuf.st_nlink != 2) e(17);
  if (stat("Dir/L2", &stbuf) != 0) e(18);
  if (stbuf.st_nlink != 2) e(19);

  /* L1, L2, and Dir/L2 exist at this point. */
  if (unlink("Dir/L2") != 0) e(20);
  if (link("L1", "Dir/L2") != 0) e(21);
  if (unlink("L1") != 0) e(22);
  if (close(fd) != 0) e(23);
  if (chdir("Dir") != 0) e(24);
  if (unlink("L2") != 0) e(25);
  if (chdir("..") != 0) e(26);
  
  /* L2 exists at this point. Test linking to unsearchable dir. */
  if (link("L2", "Dir/L2") != 0) e(27);
  if (chmod("Dir", 0666) != 0) e(27);
  if (link("L2", "Dir/L2") != -1) e(28);
  if (errno != EACCES) e(29);
  errno = 0;
  if (link("Dir/L2", "L3") != -1) e(30);
  if (errno != EACCES) e(31);
  if (chmod("Dir", 0777) != 0) e(32);
  if (unlink("Dir/L2") != 0) e(33);
  if (unlink("L3") == 0) e(34);

  /* L2 exists at this point. Test linking to unwriteable dir. */
  if (chmod("Dir", 0555) != 0) e(35);
  if (link("L2", "Dir/L2") != -1) e(36);
  if (errno != EACCES) e(37);
  if (chmod("Dir", 0777) != 0) e(38);

  /* L2 exists at this point.  Test linking mode 0 file. */
  if (chmod("L2", 0) != 0) e(39);
  if (link("L2", "L3") != 0) e(40);
  if (stat("L3", &stbuf) != 0) e(41);
  if (stbuf.st_nlink != 2) e(42);
  if (unlink("L2") != 0) e(43);

  /* L3 exists at this point.  Test linking to an existing file. */
  if ( (fd = creat("L1", 0666)) < 0) e(44);
  if (link("L1", "L3") != -1) e(45);
  if (errno != EEXIST) e(46);
  errno = 0;
  if (link("L1", "L1") != -1) e(47);
  if (errno != EEXIST) e(48);
  if (unlink("L3") != 0) e(49);

  /* L1 exists at this point. Test creating too many links. */
  for (i = 2; i <= LINK_MAX; i++) {
	sprintf(name, "Lx%d", i);
	if (link("L1", name) != 0) e(50);
  }
  if (stat("L1", &stbuf) != 0) e(51);
  if (stbuf.st_nlink != LINK_MAX) e(52);
  if (link("L1", "L2") != -1) e(53);
  if (errno != EMLINK) e(54);
  for (i = 2; i <= LINK_MAX; i++) {
	sprintf(name, "Lx%d", i);
	if (unlink(name) != 0) e(55);
  }

  if (stat("L1", &stbuf) != 0) e(56);
  if (stbuf.st_nlink != 1) e(57);

  /* L1 exists.  Test ENOENT. */
  errno = 0;
  if (link("xx/L1", "L2") != -1) e(58);
  if (errno != ENOENT) e(59);
  errno = 0;
  if (link("L1", "xx/L2") != -1) e(60);
  if (errno != ENOENT) e(61);
  errno = 0;
  if (link("L4", "L5") != -1) e(62);
  if (errno != ENOENT) e(63);
  errno = 0;
  if (link("", "L5") != -1) e(64);
  if (errno != ENOENT) e(65);
  errno = 0;
  if (link("L1", "") != -1) e(66);
  if (errno != ENOENT) e(67);

  /* L1 exists.  Test ENOTDIR. */
  errno = 0;
  if (link("/dev/tty/x", "L2") != -1) e(68);
  if (errno != ENOTDIR) e(69);

  /* L1 exists.  Test EPERM. */
  if (link(".", "L2") != -1) e(70);
  if (errno != EPERM) e(71);

  /* L1 exists. Test unlink. */
  if (link("L1", "Dir/L1") != 0) e(72);
  if (chmod("Dir", 0666) != 0) e(73);
  if (unlink("Dir/L1") != -1) e(74);
  if (errno != EACCES) e(75);
  errno = 0;
  if (chmod("Dir", 0555) != 0) e(76);
  if (unlink("Dir/L1") != -1) e(77);
  if (errno != EACCES) e(78);

  if (unlink("L7") != -1) e(79);
  if (errno != ENOENT) e(80);
  errno = 0;
  if (unlink("") != -1) e(81);
  if (errno != ENOENT) e(82);

  if (unlink("Dir/L1/L2") != -1) e(83);
  if (errno != ENOTDIR) e(84);
 
  if (chmod("Dir", 0777) != 0) e(85);
  if (unlink("Dir/L1") != 0) e(86);
  if (unlink("Dir") != -1) e(87);
  if (errno != EPERM) e(88);
  if (unlink("L1") != 0) e(89);
  if (system("rm -rf Dir") != 0) e(90);
  if (close(fd) != 0) e(91);  
}

void test39h()
{
/* Test access. */

  int fd;

  subtest = 10;
  system("rm -rf A1");
  if ( (fd = creat("A1", 0777)) < 0) e(1);
  if (close(fd) != 0) e(2);
  if (access("A1", R_OK) != 0) e(3);
  if (access("A1", W_OK) != 0) e(4);
  if (access("A1", X_OK) != 0) e(5);
  if (access("A1", (R_OK|W_OK|X_OK)) != 0) e(6);
  
  if (chmod("A1", 0400) != 0) e(7);
  if (access("A1", R_OK) != 0) e(8);
  if (access("A1", W_OK) != -1) e(9);
  if (access("A1", X_OK) != -1) e(10);
  if (access("A1", (R_OK|W_OK|X_OK)) != -1) e(11);
  
  if (chmod("A1", 0077) != 0) e(12);
  if (access("A1", R_OK) != -1) e(13);
  if (access("A1", W_OK) != -1) e(14);
  if (access("A1", X_OK) != -1) e(15);
  if (access("A1", (R_OK|W_OK|X_OK)) != -1) e(16);
  if (errno != EACCES) e(17);

  if (access("", R_OK) != -1) e(18);
  if (errno != ENOENT) e(19);
  if (access("./A1/x", R_OK) != -1) e(20);
  if (errno != ENOTDIR) e(21);

  if (unlink("A1") != 0) e(22);
}

void test39i()
{
/* Test chmod. */

  int fd, i;
  struct stat stbuf;

  subtest = 11;
  system("rm -rf A1");
  if ( (fd = creat("A1", 0777)) < 0) e(1);

  for (i = 0; i < 511; i++) {
	if (chmod("A1", i) != 0) e(100+i);
	if (fstat(fd, &stbuf) != 0) e(200+i);
	if ( (stbuf.st_mode&(S_IRWXU|S_IRWXG|S_IRWXO)) != i) e(300+i);
  }
  if (close(fd) != 0) e(2);

  if (chmod("A1/x", 0777) != -1) e(3);
  if (errno != ENOTDIR) e(4);
  if (chmod("Axxx", 0777) != -1) e(5);
  if (errno != ENOENT) e(6);
  errno = 0;
  if (chmod ("", 0777) != -1) e(7);
  if (errno != ENOENT) e(8);

  /* Now perform limited chown tests.  These should work even as non su */
  i = getuid();
/* DEBUG -- Not yet implemented 
  if (chown("A1", i, 0) != 0) e(9);
  if (chown("A1", i, 1) != 0) e(10);
  if (chown("A1", i, 2) != 0) e(11);
  if (chown("A1", i, 3) != 0) e(12);
  if (chown("A1", i, 4) != 0) e(13);
  if (chown("A1", i, 0) != 0) e(14);
*/

  if (unlink("A1") != 0) e(9);
}

void test39j()
{
/* Test utime. */

  int fd;
  time_t tloc;
  struct utimbuf times;
  struct stat stbuf;

  subtest = 12;
  if (system("rm -rf A2") != 0) e(1);
  if ( (fd = creat("A2", 0666)) < 0) e(2);
  times.modtime = 100;
  if (utime("A2", &times) != 0) e(3);
  if (stat("A2", &stbuf) != 0) e(4);
  if (stbuf.st_mtime != 100) e(5);

  tloc = time((time_t *)NULL);		/* get current time */
  times.modtime = tloc;
  if (utime("A2", &times) != 0) e(6);
  if (stat("A2", &stbuf) != 0) e(7);
  if (stbuf.st_mtime != tloc) e(8);
  if (close(fd) != 0) e(9);
  if (unlink("A2") != 0) e(10);
}

void e(n)
int n;
{
  int err_num = errno;		/* save errno in case printf clobbers it */

  printf("Subtest %d,  error %d  errno=%d  ", subtest, n, errno);
  fflush(stdout);		/* stdout and stderr are mixed horribly */
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
