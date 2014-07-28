/* test34: chmod() chown() 	Author: Jan-Mark Wams (jms@cs.vu.nl) */

/* There is a problem getting valid uids and gids, so we use the passwd
** file (ie. /etc/passwd). I don't like this, but I see no other way.
** The read-only-device-error (EROFS) is not checked!
** Supplementary group IDs are ignored.
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
#include <ctype.h>
#include <time.h>
#include <stdio.h>

int max_error = 	4;
#include "common.h"

#define ITERATIONS      4
#define N 100


#define ALL_RWXB	(S_IRWXU | S_IRWXG | S_IRWXO)
#define ALL_SETB	(S_ISUID | S_ISGID)
#define ALL_BITS	(ALL_RWXB | ALL_SETB)

#define System(cmd)   if (system(cmd) != 0) printf("``%s'' failed\n", cmd)
#define Chdir(dir)    if (chdir(dir) != 0) printf("Can't goto %s\n", dir)
#define Stat(a,b)     if (stat(a,b) != 0) printf("Can't stat %s\n", a)
#define Mkfifo(f)     if (mkfifo(f,0777)!=0) printf("Can't make fifo %s\n", f)
#define Mkdir(f)      if (mkdir(f,0777)!=0) printf("Can't make dir %s\n", f)
#define Creat(f)      if (close(creat(f,0777))!=0) printf("Can't creat %s\n",f)

/* This program uses /etc/passwd and assumes things about it's contents. */
#define PASSWD_FILE	"/etc/passwd"

int superuser;
int I_can_chown;
char *MaxName;			/* Name of maximum length */
char MaxPath[PATH_MAX];		/* Same for path */
char *NameTooLong;		/* Name of maximum +1 length */
char PathTooLong[PATH_MAX + 1];	/* Same for path, both too long */

void test34a(void);
void test34b(void);
void test34c(void);
mode_t mode(char *file_name);
void makelongnames(void);
void getids(uid_t * uid, gid_t * gid);

int main(int argc, char *argv[])
{
  int i, m = 0xFFFF;

  sync();
  if (argc == 2) m = atoi(argv[1]);
  umask(0000);
  start(34);
  makelongnames();
  superuser = (geteuid() == (uid_t) 0);

#ifdef _POSIX_CHOWN_RESTRICTED
  I_can_chown = superuser;
#else
  I_can_chown = 1;
#endif


  for (i = 1; i < ITERATIONS; i++) {
	if (m & 0001) test34a();
	if (m & 0002) test34b();
	if (m & 0004) test34c();
  }
  quit();

  return(-1);	/* Unreachable */
}

void test34a()
{				/* Test normal operation. */
  time_t time1, time2;
  mode_t mod;
  struct stat st1, st2;
  int cnt;
  uid_t uid, uid2;
  gid_t gid, gid2;
  int stat_loc;

  subtest = 1;

  /* Make scratch file. */
  Creat("foo");

  for (mod = 0; mod <= ALL_BITS; mod++) {
	if ((mod & ALL_BITS) != mod)	/* If not a valid mod next. */
		continue;
	Stat("foo", &st1);
	if (time(&time1) == (time_t) - 1) e(1);
	if (chmod("foo", mod) != 0) e(2);
	Stat("foo", &st2);
	if (time(&time2) == (time_t) - 1) e(3);
	if (superuser)
		if ((st2.st_mode & ALL_BITS) != mod) e(4);
	if (!superuser)
		if ((st2.st_mode & ALL_RWXB) != (mod & ALL_RWXB)) e(5);

	/* Test the C time feald. */
	if (st1.st_ctime > st2.st_ctime) e(6);
	if (st1.st_ctime > time1) e(7);
	if (st1.st_ctime > time2) e(8);
#ifndef V1_FILESYSTEM
	if (st2.st_ctime < time1) e(9);
#endif
	if (st2.st_ctime > time2) e(10);
	if (st1.st_atime != st2.st_atime) e(11);
	if (st1.st_mtime != st2.st_mtime) e(12);
  }				/* End for loop. */

  /* Check if chown(file, geteuid(), getegid()) works. */
  for (cnt = 0; cnt < 20; cnt++) {
	/* Set all rights on foo, including the set .id bits. */
	if (chmod("foo", ALL_BITS) != 0) e(13);
	Stat("foo", &st1);
	if (time(&time1) == (time_t) -1) e(14);

	if (chown("foo", geteuid(), getegid()) != 0) e(15);
	Stat("foo", &st2);
	if (time(&time2) == (time_t) -1) e(16);

	/* Check ``chown()'' killed the set .id bits. */
	if (!superuser) {
		if ((st1.st_mode & ALL_RWXB) != ALL_RWXB) e(17);
		if ((st2.st_mode & ALL_BITS) != ALL_RWXB) e(18);
	}
	if (superuser) {
		if ((st1.st_mode & ALL_BITS) != ALL_BITS) e(19);
		if ((st1.st_mode & ALL_RWXB) != ALL_RWXB) e(20);
	}

	/* Check the timing. */
	if (st1.st_ctime > st2.st_ctime) e(21);
	if (st1.st_ctime > time1) e(22);
	if (st1.st_ctime > time2) e(23);
#ifndef V1_FILESYSTEM
	if (st2.st_ctime < time1) e(24);
#endif
	if (st2.st_ctime > time2) e(25);
	if (st1.st_atime != st2.st_atime) e(26);
	if (st1.st_mtime != st2.st_mtime) e(27);
  }				/* End for loop. */

  /* Make scratch file. */
  if (chmod("foo", ALL_RWXB) != 0) e(28);

  if (I_can_chown) {
	/* Do a 20 tests on a gid and uid. */
	for (cnt = 0; cnt < 20; cnt++) {
		/* Get a uid and a gid, test chown. */
		getids(&uid, &gid);
		Stat("foo", &st1);
		if (time(&time1) == (time_t) -1) e(29);
		if (chown("foo", (uid_t) 0, (gid_t) 0) != 0) e(30);
		Stat("foo", &st2);
		if (time(&time2) == (time_t) -1) e(31);

		/* Test the C time field. */
		if (st1.st_ctime > st2.st_ctime) e(32);
		if (st1.st_ctime > time1) e(33);
		if (st1.st_ctime > time2) e(34);
		if (st2.st_ctime < time1) e(35);
		if (st2.st_ctime > time2) e(36);
		if (st1.st_atime != st2.st_atime) e(37);
		if (st1.st_mtime != st2.st_mtime) e(38);

		/* Do aditional tests. */
		if (chown("foo", (uid_t) 0, gid) != 0) e(39);
		if (chown("foo", uid, (gid_t) 0) != 0) e(40);
		if (chown("foo", uid, gid) != 0) e(41);
	}
  }
  if (superuser) {
	/* Check if a non-superuser can change a files gid to gid2 *
	 * if gid2 is the current process gid. */
	for (cnt = 0; cnt < 5; cnt++) {
		switch (fork()) {
		    case -1:
			printf("Can't fork\n");
			break;
		    case 0:
			alarm(20);

			getids(&uid, &gid);
			if (uid == 0) {
				getids(&uid, &gid);
				if (uid == 0) e(42);
			}
			getids(&uid2, &gid2);
			if (gid == gid2) e(43);

			/* Creat boo and bar for user uid of group gid. */
			Creat("boo");
			if (chown("boo", uid, gid) != 0) e(44);
			if (chmod("boo", ALL_BITS) != 0) e(45);
			Creat("bar");
			if (chown("bar", uid, gid) != 0) e(46);
			if (chmod("bar", ALL_BITS) != 0) e(47);

			/* We now become user uid of group gid2. */
			setgid(gid2);
			setuid(uid);

			Stat("bar", &st1);
			if (time(&time1) == (time_t) -1) e(48);
			if (chown("bar", uid, gid2) != 0) e(49);
			Stat("bar", &st2);
			if (time(&time2) == (time_t) -1) e(50);

			/* Check if the SET_BITS are cleared. */
			if ((st1.st_mode & ALL_BITS) != ALL_BITS) e(51);
			if ((st2.st_mode & ALL_BITS) != ALL_RWXB) e(52);

			/* Check the st_times. */
			if (st1.st_ctime > st2.st_ctime) e(53);
			if (st1.st_ctime > time1) e(54);
			if (st1.st_ctime > time2) e(55);
			if (st2.st_ctime < time1) e(56);
			if (st2.st_ctime > time2) e(57);
			if (st1.st_atime != st2.st_atime) e(58);
			if (st1.st_mtime != st2.st_mtime) e(59);

			Stat("boo", &st1);
			if (chmod("boo", ALL_BITS) != 0) e(60);
			Stat("boo", &st2);

			/* Check if the set gid bit is cleared. */
			if ((st1.st_mode & ALL_RWXB) != ALL_RWXB) e(61);
			if ((st2.st_mode & S_ISGID) != 0) e(62);

			if (chown("boo", uid, gid2) != 0) e(63);
			Stat("boo", &st1);

			/* Check if the set uid bit is cleared. */
			if ((st1.st_mode & S_ISUID) != 0) e(64);

			exit(0);
		    default:
			wait(&stat_loc);
			if (stat_loc != 0) e(65);	/* Alarm? */
		}
	}			/* end for loop. */
  }				/* end if (superuser). */
  if (chmod("foo", ALL_BITS) != 0) e(66);
  Stat("foo", &st1);
  if (chown("foo", geteuid(), getegid()) != 0) e(67);
  Stat("foo", &st2);
  if ((st1.st_mode & ALL_BITS) != ALL_BITS) e(68);	/* See intro! */
  if (superuser)
	if ((st2.st_mode & ALL_RWXB) != ALL_RWXB) e(69);
  if (!superuser)
	if ((st2.st_mode & ALL_BITS) != ALL_RWXB) e(70);

  (void) system("chmod 777 ../DIR_34/* > /dev/null 2> /dev/null");
  System("rm -rf ../DIR_34/*");
}

void test34b()
{
  time_t time1, time2;
  mode_t mod;
  struct stat st1, st2;

  subtest = 2;

  /* Test chmod() and chown() on non regular files and on MaxName and
   * MaxPath. * Funny, but dirs should also have S_IS.ID bits.
   */
  Mkfifo("fifo");
  Mkdir("dir");
  Creat(MaxName);
  MaxPath[strlen(MaxPath) - 2] = '/';
  MaxPath[strlen(MaxPath) - 1] = 'a';	/* make ././.../a */
  Creat(MaxPath);

  for (mod = 1; mod <= ALL_BITS; mod <<= 1) {
	if ((mod & ALL_BITS) != mod) continue;	/* bad mod */
	Stat("dir", &st1);
	if (time(&time1) == (time_t) -1) e(1);
	if (chmod("dir", mod) != 0) e(2);
	Stat("dir", &st2);
	if (time(&time2) == (time_t) -1) e(3);
	if (superuser)
		if ((st2.st_mode & ALL_BITS) != mod) e(4);
	if (!superuser)
		if ((st2.st_mode & ALL_RWXB) != (mod & ALL_RWXB)) e(5);

	/* Test the C time field. */
	if (st1.st_ctime > st2.st_ctime) e(6);
	if (st1.st_ctime > time1) e(7);
	if (st1.st_ctime > time2) e(8);
#ifndef V1_FILESYSTEM
	if (st2.st_ctime < time1) e(9);
#endif
	if (st2.st_ctime > time2) e(10);
	if (st1.st_atime != st2.st_atime) e(11);
	if (st1.st_mtime != st2.st_mtime) e(12);

	Stat("fifo", &st1);
	if (time(&time1) == (time_t) -1) e(13);
	if (chmod("fifo", mod) != 0) e(14);
	Stat("fifo", &st2);
	if (time(&time2) == (time_t) -1) e(15);
	if (superuser)
		if ((st2.st_mode & ALL_BITS) != mod) e(16);
	if (!superuser)
		if ((st2.st_mode & ALL_RWXB) != (mod & ALL_RWXB)) e(17);

	/* Test the C time field. */
	if (st1.st_ctime > st2.st_ctime) e(18);
	if (st1.st_ctime > time1) e(19);
	if (st1.st_ctime > time2) e(20);
#ifndef V1_FILESYSTEM
	if (st2.st_ctime < time1) e(21);
#endif
	if (st2.st_ctime > time2) e(22);
	if (st1.st_atime != st2.st_atime) e(23);
	if (st1.st_mtime != st2.st_mtime) e(24);

	Stat(MaxName, &st1);
	if (time(&time1) == (time_t) -1) e(25);
	if (chmod(MaxName, mod) != 0) e(26);
	Stat(MaxName, &st2);
	if (time(&time2) == (time_t) -1) e(27);
	if (superuser)
		if ((st2.st_mode & ALL_BITS) != mod) e(28);
	if (!superuser)
		if ((st2.st_mode & ALL_RWXB) != (mod & ALL_RWXB)) e(29);

	/* Test the C time field. */
	if (st1.st_ctime > st2.st_ctime) e(30);
	if (st1.st_ctime > time1) e(31);
	if (st1.st_ctime > time2) e(32);
#ifndef V1_FILESYSTEM
	if (st2.st_ctime < time1) e(33);
#endif
	if (st2.st_ctime > time2) e(34);
	if (st1.st_atime != st2.st_atime) e(35);
	if (st1.st_mtime != st2.st_mtime) e(36);

	Stat(MaxPath, &st1);
	if (time(&time1) == (time_t) -1) e(37);
	if (chmod(MaxPath, mod) != 0) e(38);
	Stat(MaxPath, &st2);
	if (time(&time2) == (time_t) -1) e(39);
	if (superuser)
		if ((st2.st_mode & ALL_BITS) != mod) e(40);
	if (!superuser)
		if ((st2.st_mode & ALL_RWXB) != (mod & ALL_RWXB)) e(41);

	/* Test the C time field. */
	if (st1.st_ctime > st2.st_ctime) e(42);
	if (st1.st_ctime > time1) e(43);
	if (st1.st_ctime > time2) e(44);
#ifndef V1_FILESYSTEM
	if (st2.st_ctime < time1) e(45);
#endif
	if (st2.st_ctime > time2) e(46);
	if (st1.st_atime != st2.st_atime) e(47);
	if (st1.st_mtime != st2.st_mtime) e(48);
  }

  if (chmod("dir", 0777) != 0) e(49);
  if (chmod("fifo", 0777) != 0) e(50);
  if (chmod(MaxName, 0777) != 0) e(51);
  if (chmod(MaxPath, 0777) != 0) e(52);

  (void) system("chmod 777 ../DIR_34/* > /dev/null 2> /dev/null");
  System("rm -rf ../DIR_34/*");
}

void test34c()
{
  struct stat st;
  uid_t uid, uid2;
  gid_t gid, gid2;
  int fd, does_truncate, stat_loc;

  subtest = 3;

  Mkdir("dir");
  Creat("dir/try_me");

  /* Disalow search permission and see if chmod() and chown() return
   * EACCES. 
   */
  if (chmod("dir", ALL_BITS & ~S_IXUSR) != 0) e(1);
  if (!superuser) {
	if (chmod("dir/try_me", 0) != -1) e(2);
	if (errno != EACCES) e(3);
	if (I_can_chown) {
		if (chown("dir/try_me", geteuid(), getegid()) != -1) e(4);
		if (errno != EACCES) e(5);
	}
  }

  /* Check ENOTDIR. */
  Mkfifo("fifo");
  if (chmod("fifo/try_me", 0) != -1) e(6);
  if (errno != ENOTDIR) e(7);
  if (chown("fifo/try_me", geteuid(), getegid()) != -1) e(8);
  if (errno != ENOTDIR) e(9);

  Creat("file");
  if (chmod("file/try_me", 0) != -1) e(10);
  if (errno != ENOTDIR) e(11);
  if (chown("file/try_me", geteuid(), getegid()) != -1) e(12);
  if (errno != ENOTDIR) e(13);

  /* Check empty path. */
  if (chmod("", 0) != -1) e(14);
  if (errno != ENOENT) e(15);
  if (chown("", geteuid(), getegid()) != -1) e(16);
  if (errno != ENOENT) e(17);

  /* Check non existing file name. */
  if (chmod("non_exist", 0) != -1) e(18);
  if (errno != ENOENT) e(19);
  if (chown("non_exist", geteuid(), getegid()) != -1) e(20);
  if (errno != ENOENT) e(21);

  /* Check what we get if we do not have permisson. */
  if (!superuser) {
	Stat("/", &st);
	if (st.st_uid == geteuid()) e(22);

	/* First I had 0, I changed it to st.st_mode 8-). */
	if (chmod("/", st.st_mode) != -1) e(23);
	if (errno != EPERM) e(24);
  }
  if (!I_can_chown) {
	Stat("/", &st);
	if (st.st_uid == geteuid()) e(25);
	if (chown("/", geteuid(), getegid()) != -1) e(26);
	if (errno != EPERM) e(27);
  }

  /* If we are superuser, we can test all id combinations. */
  if (superuser) {
	switch (fork()) {
	    case -1:	printf("Can't fork\n");	break;
	    case 0:
		alarm(20);

		getids(&uid, &gid);
		if (uid == 0) {
			getids(&uid, &gid);
			if (uid == 0) e(28);
		}
		getids(&uid2, &gid2);
		if (gid == gid2) e(29);
		if (uid == uid2) e(30);

		/* Creat boo, owned by root. */
		Creat("boo");
		if (chmod("boo", ALL_BITS) != 0) e(31);

		/* Creat boo for user uid2 of group gid2. */
		Creat("bar");
		if (chown("bar", uid2, gid2) != 0) e(32);
		if (chmod("bar", ALL_BITS) != 0) e(33);

		/* Creat my_gid for user uid2 of group gid. */
		Creat("my_gid");
		if (chown("my_gid", uid2, gid) != 0) e(34);
		if (chmod("my_gid", ALL_BITS) != 0) e(35);

		/* Creat my_uid for user uid of uid gid. */
		Creat("my_uid");
		if (chown("my_uid", uid, gid) != 0) e(36);
		if (chmod("my_uid", ALL_BITS) != 0) e(37);

		/* We now become user uid of uid gid. */
		setgid(gid);
		setuid(uid);

		if (chown("boo", uid, gid) != -1) e(38);
		if (errno != EPERM) e(39);
		if (chown("bar", uid, gid) != -1) e(40);
		if (errno != EPERM) e(41);
		if (chown("my_gid", uid, gid) != -1) e(42);
		if (errno != EPERM) e(43);
		if (chown("my_uid", uid, gid2) != -1) e(44);

		/* The EPERM is not strict POSIX. */
		if (errno != EPERM) e(45);

		if (chmod("boo", 0) != -1) e(46);
		if (errno != EPERM) e(47);
		if (chmod("bar", 0) != -1) e(48);
		if (errno != EPERM) e(49);
		if (chmod("my_gid", 0) != -1) e(50);
		if (errno != EPERM) e(51);

		exit(0);
	    default:
		wait(&stat_loc);
		if (stat_loc != 0) e(52);	/* Alarm? */
	}
  }

  /* Check too long path ed. */
  does_truncate = does_fs_truncate();
  fd = creat(NameTooLong, 0777);
  if (does_truncate) {
  	if (fd == -1) e(53);
	if (close(fd) != 0) e(54);
	if (chmod(NameTooLong, 0777) != 0) e(55);
	if (chown(NameTooLong, geteuid(), getegid()) != 0) e(56);
  } else {
  	if (fd != -1) e(57);
  	if (errno != ENAMETOOLONG) e(58);
  	(void) close(fd);		/* Just in case */
  }

  /* Make PathTooLong contain ././.../a */
  PathTooLong[strlen(PathTooLong) - 2] = '/';
  PathTooLong[strlen(PathTooLong) - 1] = 'a';
  Creat("a");
  if (chmod(PathTooLong, 0777) != -1) e(59);
  if (errno != ENAMETOOLONG) e(60);
  if (chown(PathTooLong, geteuid(), getegid()) != -1) e(61);
  if (errno != ENAMETOOLONG) e(62);

  (void) system("chmod 777 ../DIR_34/* > /dev/null 2> /dev/null");
  System("rm -rf ../DIR_34/*");
}

void makelongnames()
{
  register int i;
  int max_name_length;

  max_name_length = name_max("."); /* Aka NAME_MAX, but not every FS supports
				    * the same length, hence runtime check */
  MaxName = malloc(max_name_length + 1);
  NameTooLong = malloc(max_name_length + 1 + 1); /* Name of maximum +1 length */
  memset(MaxName, 'a', max_name_length);
  MaxName[max_name_length] = '\0';

  for (i = 0; i < PATH_MAX - 1; i++) {	/* idem path */
	MaxPath[i++] = '.';
	MaxPath[i] = '/';
  }
  MaxPath[PATH_MAX - 1] = '\0';

  strcpy(NameTooLong, MaxName);	/* copy them Max to ToLong */
  strcpy(PathTooLong, MaxPath);

  NameTooLong[max_name_length] = 'a';
  NameTooLong[max_name_length+1] = '\0';/* extend ToLongName by one too many */
  PathTooLong[PATH_MAX - 1] = '/';
  PathTooLong[PATH_MAX] = '\0';	/* inc ToLongPath by one */
}

/* Getids returns a valid uid and gid. Is used PASSWD FILE.
 * It assumes the following format for a passwd file line:
 * <user_name>:<passwd>:<uid>:<gid>:<other_stuff>
 * If no uids and gids can be found, it will only return 0 ids.
 */
void getids(r_uid, r_gid)
uid_t * r_uid;
gid_t * r_gid;
{
  char line[N];
  char *p;
  uid_t uid;
  gid_t gid;
  FILE *fp;
  int i;

  static uid_t a_uid[N];	/* Array for uids. */
  static gid_t a_gid[N];	/* Array for gids. */
  static int nuid = 0, ngid = 0;/* The number of user & group ids. */
  static int cuid = 0, cgid = 0;/* The current id index. */

  /* If we don't have any uids go read some from the passwd file. */
  if (nuid == 0) {
	a_uid[nuid++] = 0;	/* Root uid and gid. */
	a_gid[ngid++] = 0;
	if ((fp = fopen(PASSWD_FILE, "r")) == NULL) {
		printf("Can't open ");
		perror(PASSWD_FILE);
	}
	while (fp != NULL && fgets(line, sizeof(line), fp) != NULL) {
		p = strchr(line, ':');
		if (p != NULL) p = strchr(p + 1, ':');
		if (p != NULL) {
			p++;
			uid = 0;
			while (isdigit(*p)) {
				uid *= 10;
				uid += (uid_t) (*p - '0');
				p++;
			}
			if (*p != ':') continue;
			p++;
			gid = 0;
			while (isdigit(*p)) {
				gid *= 10;
				gid += (gid_t) (*p - '0');
				p++;
			}
			if (*p != ':') continue;
			if (nuid < N) {
				for (i = 0; i < nuid; i++)
					if (a_uid[i] == uid) break;
				if (i == nuid) a_uid[nuid++] = uid;
			}
			if (ngid < N) {
				for (i = 0; i < ngid; i++)
					if (a_gid[i] == gid) break;
				if (i == ngid) a_gid[ngid++] = gid;
			}
			if (nuid >= N && ngid >= N) break;
		}
	}
	if (fp != NULL) fclose(fp);
  }

  /* We now have uids and gids in a_uid and a_gid. */
  if (cuid >= nuid) cuid = 0;
  if (cgid >= ngid) cgid = 0;
  *r_uid = a_uid[cuid++];
  *r_gid = a_gid[cgid++];
}
