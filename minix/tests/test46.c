/* Test46.c
 *
 * Test getgroups(...) and setgroups system calls
 *
 * Please note that getgroups is POSIX defined, but setgroups is not. Errors
 * related to setgroups are thus not POSIX conformance issues.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>

void api_test(void);
void e(int error_no);
void group_test(void);
void limit_test(void);
void group_test_1(void);
void group_test_2(void);
void group_test_3(void);
void group_test_4(void);
void group_test_5(void);
int dotest(void (*testfunc)(void));

int max_error = 5;
#include "common.h"

#define IMAGINARY_GID 100
#define IMAGINARY_GID_STR "100"
#define IMAGINARY_UID 101
#define IMAGINARY_UID_STR "101"
#define SET_CREDENTIALS do { \
			  setgid((IMAGINARY_GID) + 1 ); \
			  setuid(IMAGINARY_UID); \
			} while(0)

int subtest = -1, errorct = 0;

int main(int argc, char *argv[])
{
  int superuser;
  start(46);

  superuser = (geteuid() == 0);

  if(!superuser) {
  	if(!(setuid(0) || seteuid(0))) {
		printf("Test 46 has to be run as root; test aborted\n");
		exit(1);
	}
  }

  limit_test();	/* Perform some tests on POSIX limits */
  api_test();	/* Perform some very basic API tests */
  group_test();	/* Perform some tests that mimic actual use */

  quit();

  return(-1);	/* Unreachable */
}

void limit_test() {
/* According to POSIX 2008 a process can have up to NGROUPS_MAX simultaneous
 * supplementary group IDs. The minimum acceptable value is _POSIX_NGROUPS_MAX.
 * In turn, _POSIX_NGROUPS_MAX is defined as 8. */

  subtest = 1;
  if (_POSIX_NGROUPS_MAX < 8) e(1);
  if (NGROUPS_MAX < _POSIX_NGROUPS_MAX) e(2);
}

void api_test() {
/* int getgroups( int gidsetsize, gid_t grouplist[]);
 * int setgroups( int size_t size, const gid_t grouplist[]);
 */
/* The getgroups() function shall fill in the array grouplist with the current
 * supplementary group IDs of the calling process. It is implementation-
 * defined whether getgroups() also returns the effective group ID in the
 * grouplist array.
 * The gidsetsize argument specifies the number of elements in the array
 * grouplist. The actual number of group IDs stored in the array shall be
 * returned. The values of array entries with indices greater than or equal to
 * the value returned are undefined.
 * If gidsetsize is 0, getgroups shall return the number of group IDs that it
 * would otherwise return without modifying the array pointed to by grouplist.
 *
 * setgroups() sets the supplementary group IDs for the calling process. The
 * size argument specifies the number of supplementary group IDs in the buffer
 * pointed to by grouplist. setgroups() is a privileged operation.
 */

 /* Minix does not return the effective group ID with the supplementary groups.
  * Use getegid() to get that value. In order to call setgroups, a process
  * must have super user privileges.
  */

  int i;
  gid_t *grouplist, *grouplist2;
  long ngroups_max;

  subtest = 2;

  /* Ask the system how many groups we're allowed to set */
  ngroups_max = sysconf(_SC_NGROUPS_MAX);
  grouplist = malloc(ngroups_max *sizeof(gid_t));
  grouplist2 = malloc(ngroups_max *sizeof(gid_t));

  /* Let's invent some imaginary groups */
#define START_GID 20001
  for (i = 0; i < ngroups_max; i++)
	grouplist[i] = i + START_GID;

  /* Normal usage */
  if (setgroups(ngroups_max, grouplist) != 0) e(1);

  /* Try one less than max supported groups */
  if (setgroups(ngroups_max - 1, grouplist) != 0) e(2);

  /* Try just one group */
  if (setgroups(1, grouplist) != 0) e(3);

  /* Unset all supplementary groups */
  if (setgroups(0, grouplist) != 0) e(4);

  /* Should not be allowed to use a negative set size */
  if (setgroups(-1, grouplist) == 0) e(5);
  else if(errno != EINVAL) e(6); /* error must be EINVAL */

  /* Should not be allowed to set more groups than supported by the system */
  if (setgroups(ngroups_max + 1, grouplist) == 0) e(7);
  else if(errno != EINVAL) e(8); /* error must be EINVAL */

  /* Should not be allowed to provide an invalid grouplist address */
  if (setgroups(ngroups_max, NULL) == 0) e(9);
  else if(errno != EFAULT) e(10); /* error must be EFAULT */

  /* The last time we called setgroups with proper parameters, we effectively
   * cleared the list. Verify that with getgroups(). */
  if (getgroups(ngroups_max, grouplist2) != 0) e(11);

  /* Repopulate grouplist with values and read them back */
  if (setgroups(ngroups_max, grouplist) != 0) e(12);
  if (getgroups(0, grouplist2) != ngroups_max) e(13);
  if (getgroups(ngroups_max, grouplist2) != ngroups_max) e(14);
  for (i = 0; i < ngroups_max; i++) {
  	if(grouplist[i] != grouplist2[i]) {
		e(15); 
		break; /* One error message should be enough here */ 
	}
  }

  /* Should not be able to read less groups than are actually stored. */
  if (getgroups(ngroups_max - 1, grouplist2) != -1) e(16);

  /* Repopulate grouplist with only half the groups and read them back */
  memset(grouplist2, 0, ngroups_max * sizeof(gid_t)); /* Clear array */
#define HALF_LIST_SIZE ngroups_max / 2
  if (setgroups(HALF_LIST_SIZE, grouplist) != 0) e(17);
  if (getgroups(0, grouplist2) != HALF_LIST_SIZE) e(18);
  if (getgroups(HALF_LIST_SIZE, grouplist2) != HALF_LIST_SIZE) e(19);
  for (i = 0; i < HALF_LIST_SIZE; i++) {
  	if(grouplist[i] != grouplist2[i]) {
  		e(20);
  		break; /* Also here one message ought to be enough */
  	}
  }

  /* Try to read more groups than we have set */
  memset(grouplist2, 0, ngroups_max * sizeof(gid_t)); /* Clear array */
  if (getgroups(ngroups_max, grouplist2) != HALF_LIST_SIZE) e(21);
  for (i = 0; i < HALF_LIST_SIZE; i++) {
	/* Anything above indices 'HALF_LIST_SIZE' is undefined */
	if(grouplist[i] != grouplist2[i]) {
		e(22);
		break;
	}
  }

  /* Try to set too high a group ID */
  grouplist2[0] = GID_MAX + 1;	/* Out of range */
  if (setgroups(1, grouplist2) == 0) e(23);
  if (errno != EINVAL) e(24);

  free(grouplist);
  free(grouplist2);
}

void group_test() {
/* To test supplemental group support we're going to create a temporary
 * directory that can only be accessed (x bit) by members of our imaginary
 * group, read from (r bit) and written to (w bit). 
 * Then we're going to create a file in that directory that's only readable and
 * writable by the owner, also readable, writable, and both (in that order) by
 * the imaginary group, and readable, writable, and both by everyone else (2). 
 */

  int i, round;
  gid_t *grouplist;
  long ngroups_max;
#define ROUNDS 8

  subtest = 3;

  ngroups_max = sysconf(_SC_NGROUPS_MAX);
  grouplist = malloc(ngroups_max *sizeof(gid_t));

  /* Let's invent imaginary groups and user id */
  grouplist = malloc(ngroups_max * sizeof(gid_t));

  /* Now loop a few tests while using different group set sizes */
  for(round = 0; round < ROUNDS; round++) {
	grouplist[round] = IMAGINARY_GID;
	for(i = 0; i < ngroups_max; i++) {
		if(i == round) continue;
		grouplist[i] = IMAGINARY_GID + i + ngroups_max;
  	}
	setgroups(round+1, grouplist);

	system("rm -rf DIR_046 > /dev/null 2>&1");
	system("mkdir DIR_046");
	system("chmod u=rwx,g=,o= DIR_046"); /* Only access for superuser */
	system("chgrp "IMAGINARY_GID_STR" DIR_046"); /* Make imaginary group
						      * owner */

	/* Test group access on directories */
	if(dotest(group_test_1) != 0) e(1);
	system("chmod g+r DIR_046"); /* Allow group read access */
	if(dotest(group_test_1) == 0) e(2);

	system("chmod g= DIR_046");
	if(dotest(group_test_2) != 0) e(3);
	system("chmod g+x DIR_046"); /* Allow 'search' (i.e., inode data)
				      * access */
	if(dotest(group_test_2) == 0) e(4);

	if(dotest(group_test_3) != 0) e(5);
	system("chmod g+w DIR_046"); /* Allow group write access */
	if(dotest(group_test_3) == 0) e(6);

	system("chmod g-wx DIR_046"); /* Remove write and 'search' permission */
	if(dotest(group_test_4) != 0) e(7);
	system("chmod g+w DIR_046"); /* Add write permission */
	if(dotest(group_test_4) != 0) e(8);
	system("chmod g+x DIR_046"); /* Add 'search' permission */
	if(dotest(group_test_4) == 0) e(9);

	/* Subdirectories */
	system("mkdir -p DIR_046/sub");
	system("chmod u=rwx,g=,o= DIR_046");
	system("chmod u=rwx,g=,o= DIR_046/sub");
	system("chgrp "IMAGINARY_GID_STR" DIR_046/sub"); 

	if(dotest(group_test_1) != 0) e(10);
	if(dotest(group_test_5) != 0) e(11);

	system("chmod g+r DIR_046");
	if(dotest(group_test_1) == 0) e(12);
	if(dotest(group_test_5) != 0) e(13);

	system("chmod g= DIR_046");
	if(dotest(group_test_5) != 0) e(14);
	system("chmod g+r DIR_046/sub");
	if(dotest(group_test_5) != 0) e(15); /* We need search permission for
					      * sub directory DIR_046 to be
					      * able to read the contents of
					      * DIR_046/sub */
	system("chmod g+x DIR_046");
	if(dotest(group_test_1) != 0) e(16);
	if(dotest(group_test_5) == 0) e(17);
	system("chmod g+r DIR_046");
	if(dotest(group_test_5) == 0) e(18);
  }
  system("rm -rf DIR_046");
  free(grouplist);
}

int dotest( void (*func)(void) ) {
  int test_result;

  if(fork() == 0) (*func)();
  else wait(&test_result);

  return(test_result);
}

void group_test_1() {
/* Test x bit for group access. Exit value is 1 when we were able to read from
 * the directory and 0 otherwise. */
  DIR *dirp = NULL;

  SET_CREDENTIALS;

  dirp = opendir("DIR_046");
  exit(dirp != NULL); /* If not NULL, we were able to access it */
}

void group_test_2() {
/* Test x bit for group access. Exit value is 1 when we were able to access 
 * inode data of the directory and 0 otherwise. */
  struct stat buf;
  int res;

  SET_CREDENTIALS;
 
  res = stat("DIR_046/.", &buf);
  exit(res == 0);
}

void group_test_3() {
/* Test wx bits for group access. Exit value is 1 when we were able to write to 
 * the directory and 0 otherwise. */
  int fd;
  
  SET_CREDENTIALS;

  fd = open("DIR_046/writetest", O_WRONLY|O_CREAT);

  exit(fd != -1); 
}

void group_test_4() {
/* Test w bit for group access. Exit value is 1 when we were able to rename a 
 * the directory and 0 otherwise. */
  int res;
  
  SET_CREDENTIALS;

  res = rename("DIR_046/writetest", "DIR_046/renametest");

  exit(res == 0); 
}

void group_test_5() {
/* Test x bit for group access. Exit value is 1 when we were able to read from
 * the directory and 0 otherwise. */
  DIR *dirp = NULL;

  SET_CREDENTIALS;

  dirp = opendir("DIR_046/sub");
  exit(dirp != NULL); /* If not NULL, we were able to access it */
}
