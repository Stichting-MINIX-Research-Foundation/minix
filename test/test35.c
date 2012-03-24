/* test35: utime()		Author: Jan-Mark Wams (jms@cs.vu.nl) */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <limits.h>
#include <utime.h>
#include <errno.h>
#include <time.h>
#include <ctype.h>
#include <stdio.h>

#define MAX_ERROR	1
#define ITERATIONS     10
#define N 100

#include "common.c"

#define System(cmd)   if (system(cmd) != 0) printf("``%s'' failed\n", cmd)
#define Chdir(dir)    if (chdir(dir) != 0) printf("Can't goto %s\n", dir)
#define Stat(a,b)     if (stat(a,b) != 0) printf("Can't stat %s\n", a)
#define Mkfifo(f)     if (mkfifo(f,0777)!=0) printf("Can't make fifo %s\n", f)
#define Mkdir(f)      if (mkdir(f,0777)!=0) printf("Can't make dir %s\n", f)
#define Creat(f)      if (close(creat(f,0777))!=0) printf("Can't creat %s\n",f)
#define Time(t)	      if (time(t) == (time_t)-1) printf("Time error\n")
#define Chown(f,u,g)  if (chown(f,u,g) != 0) printf("Can't chown %s\n", f)
#define Chmod(f,m)    if (chmod(f,m) != 0) printf("Can't chmod %s\n", f)

#define PASSWD_FILE 	"/etc/passwd"

int I_can_chown;
int superuser;
char *MaxName;			/* Name of maximum length */
char MaxPath[PATH_MAX];		/* Same for path */
char *NameTooLong;		/* Name of maximum +1 length */
char PathTooLong[PATH_MAX + 1];	/* Same for path, both too long */

void test35a(void);
void test35b(void);
void test35c(void);
void makelongnames(void);
void getids(uid_t * uid, gid_t * gid);

int main(int argc, char *argv[])
{
  int i, m = 0xFFFF;

  sync();
  if (argc == 2) m = atoi(argv[1]);
  start(35);
  makelongnames();
  superuser = (geteuid() == 0);

#ifdef _POSIX_CHOWN_RESTRICTED
# if _POSIX_CHOWN_RESTRICTED - 0 != -1
  I_can_chown = superuser;
# else
  I_can_chown = 1;
# endif
#else
# include "error, this case requires dynamic checks and is not handled"
#endif

  for (i = 0; i < ITERATIONS; i++) {
	if (m & 0001) test35a();
	if (m & 0002) test35b();
	if (m & 0004) test35c();
  }
  quit();
  return 1;
}

void test35a()
{				/* Test normal operation. */
  struct stat st;
  struct utimbuf ub;
  time_t time1, time2;
  int cnt;

  subtest = 1;

  /* Creat scratch file. */
  Creat("foo");

  /* Set file times back two seconds. */
  Stat("foo", &st);
  ub.actime = st.st_atime - 2;
  ub.modtime = st.st_mtime - 2;
  Time(&time1);
  utime("foo", &ub);
  Time(&time2);
  Stat("foo", &st);
  if (ub.actime != st.st_atime) e(1);
  if (ub.modtime != st.st_mtime) e(2);

  /* The status changed time sould be changed. */
#ifndef V1_FILESYSTEM
  if (st.st_ctime < time1) e(3);
#endif
  if (st.st_ctime > time2) e(4);

  /* Add twenty seconds. */
  Stat("foo", &st);
  ub.actime = st.st_atime + 20;
  ub.modtime = st.st_mtime + 20;
  Time(&time1);
  utime("foo", &ub);
  Time(&time2);
  Stat("foo", &st);
  if (ub.actime != st.st_atime) e(5);
  if (ub.modtime != st.st_mtime) e(6);
  if (st.st_ctime < time1) e(7);
#ifndef V1_FILESYSTEM
  if (st.st_ctime > time2) e(8);
#endif

  /* Try 100 times to do utime in less than one second. */
  cnt = 0;
  do {
	Time(&time1);
	utime("foo", (struct utimbuf *) NULL);
	Time(&time2);
  } while (time1 != time2 && cnt++ < 100);
  if (time1 == time2) {
	Stat("foo", &st);
	Time(&time2);
	if (st.st_atime != time1) e(9);
	if (st.st_mtime != time1) e(10);
  } else {
	Stat("foo", &st);
	if (st.st_atime > time2) e(11);
	if (st.st_mtime > time2) e(12);
	Time(&time2);
	if (st.st_atime < time1) e(13);
	if (st.st_mtime < time1) e(14);
  }
  if (st.st_ctime < time1) e(15);
  if (st.st_ctime > time2) e(16);

  System("rm -rf ../DIR_35/*");
}

void test35b()
{
  subtest = 2;

  /* MaxPath and MaxName checkup. */
  Creat(MaxName);
  MaxPath[strlen(MaxPath) - 2] = '/';
  MaxPath[strlen(MaxPath) - 1] = 'a';	/* make ././.../a */
  Creat(MaxPath);
  if (utime(MaxName, NULL) != 0) e(1);
  if (utime(MaxPath, NULL) != 0) e(2);

  /* The owner doesn't need write permisson to set  times. */
  Creat("foo");
  if (chmod("foo", 0) != 0) e(3);
  if (utime("foo", NULL) != 0) e(4);
  if (chmod("foo", 0777) != 0) e(5);
  if (utime("foo", NULL) != 0) e(6);

  System("rm -rf ../DIR_35/*");
}

void test35c()
{
  gid_t gid, gid2;
  uid_t uid, uid2;
  struct utimbuf ub;
  int fd, does_truncate, stat_loc;

  subtest = 3;

  /* Access problems. */
  Mkdir("bar");
  Creat("bar/tryme");
  if (superuser) {
	Chmod("bar", 0000);	/* No search permisson at all. */
	if (utime("bar/tryme", NULL) != 0) e(1);
  }
  if (!superuser) {
	Chmod("bar", 0677);	/* No search permisson. */
	if (utime("bar/tryme", NULL) != -1) e(2);
	if (errno != EACCES) e(3);
  }
  Chmod("bar", 0777);

  if (I_can_chown) {
	switch (fork()) {
	    case -1:	printf("Can't fork\n");	break;
	    case 0:
		alarm(20);

		/* Get two differend non root uids. */
		if (superuser) {
			getids(&uid, &gid);
			if (uid == 0) getids(&uid, &gid);
			if (uid == 0) e(4);
		}
		if (!superuser) {
			uid = geteuid();
			gid = getegid();
		}
		getids(&uid2, &gid);
		if (uid == uid2) getids(&uid2, &gid2);
		if (uid == uid2) e(5);

		/* Creat a number of files for root, user and user2. */
		Creat("rootfile");	/* Owned by root. */
		Chmod("rootfile", 0600);
		Chown("rootfile", 0, 0);
		Creat("user2file");	/* Owned by user 2, writeable. */
		Chmod("user2file", 0020);
		Chown("user2file", uid2, gid);
		Creat("user2private");	/* Owned by user 2, privately. */
		Chmod("user2private", 0600);
		Chown("user2private", uid2, gid);

		if (superuser) {
			setgid(gid);
			setuid(uid);
		}

		/* We now are user ``uid'' from group ``gid''. */
		ub.actime = (time_t) 12345L;
		ub.modtime = (time_t) 12345L;

		if (utime("rootfile", NULL) != -1) e(6);
		if (errno != EACCES) e(7);
		if (utime("rootfile", &ub) != -1) e(8);
		if (errno != EPERM) e(9);

		if (utime("user2file", NULL) != 0) e(10);
		if (utime("user2file", &ub) != -1) e(11);
		if (errno != EPERM) e(12);

		if (utime("user2private", NULL) != -1) e(13);
		if (errno != EACCES) e(14);
		if (utime("user2private", &ub) != -1) e(15);
		if (errno != EPERM) e(16);

		exit(errct ? 1 : 0);
	    default:
		wait(&stat_loc);
		if (stat_loc != 0) e(17);	/* Alarm? */
	}
  }

  /* Test names that are too long. */
  does_truncate = does_fs_truncate();
  fd = creat(NameTooLong, 0777);
  if (does_truncate) {
	if (utime(NameTooLong, NULL) != 0) e(18);
  } else {
	if (utime(NameTooLong, NULL) != -1) e(19);
	if (errno != ENAMETOOLONG) e(20);
  }
  (void) close(fd);

  /* Make PathTooLong contain ././.../a */
  PathTooLong[strlen(PathTooLong) - 2] = '/';
  PathTooLong[strlen(PathTooLong) - 1] = 'a';
  Creat("a");
  if (utime(PathTooLong, NULL) != -1) e(21);
  if (errno != ENAMETOOLONG) e(22);

  /* Non existing file name. */
  if (utime("nonexist", NULL) != -1) e(23);
  if (errno != ENOENT) e(24);

  /* Empty file name. */
  if (utime("", NULL) != -1) e(25);
  if (errno != ENOENT) e(26);

  System("rm -rf ../DIR_35/*");
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
** It assumes the following format for a passwd file line:
** <user_name>:<passwd>:<uid>:<gid>:<other_stuff>
** If no uids and gids can be found, it will only return 0 ids.
*/
void getids(r_uid, r_gid)
uid_t *r_uid;
gid_t *r_gid;
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
