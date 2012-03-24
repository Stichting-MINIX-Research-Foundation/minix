/* This tests the behavior of Minix when the current working dir (cwd) doesn't
 * actually exist and we either:
 *   - create a new file
 *   - make a new directory
 *   - make a special file (mknod)
 *   - create a hard link
 *   - create a symbolic link, or
 *   - rename a file
 * In each case, `a component of the path does not name an existing file', and
 * the operation should fail with ENOENT. These tests should actually be 
 * distributed over the other tests that actually test the specific system 
 * calls.
 */

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>

int subtest = -1;
#define MAX_ERROR 999	/* Effectively no limit. This is necessary as this
			 * test tries to undo errors and should therefore not
			 * preemptively exit, as that would leave the FS
			 * in a corrupted state. */
#include "common.c"

#define TEST_PATH "a/b/c"
#define INTEGR_MSG "You might want to check fs integrity\n"

void do_test(void);

void do_test(void)
{
  int r, fd;
  int s[2];
  char buf[1], testroot[PATH_MAX+1], renamebuf[PATH_MAX+1];

  subtest = 1;
  if (socketpair(PF_UNIX, SOCK_STREAM, 0, s) == -1) e(1);
  if (system("mkdir -p " TEST_PATH) == -1) e(2);
  if (realpath(".", testroot) == NULL) e(3);

  r = fork();
  if (r == -1) e(4);
  else if (r == 0) { /* Child */
  	/* Change child's cwd to TEST_PATH */
  	if (chdir(TEST_PATH) == -1) e(5);

	/* Signal parent we're ready for the test */
	buf[0] = 'a';
	if (write(s[0], buf, sizeof(buf)) != sizeof(buf)) e(6);

	/* Wait for parent to remove my cwd */
	if (read(s[0], buf, sizeof(buf)) != sizeof(buf)) e(7);

	/* Try to create a file */
	if ((fd = open("testfile", O_RDWR | O_CREAT)) != -1) {
		e(8);
		/* Uh oh. We created a file?! Try to remove it. */
		(void) close(fd);
		if (unlink("testfile") != 0) {
			/* This is not good. We created a file, but we can
			 * never access it; we have a spurious inode.
			 */
			e(9);
			printf(INTEGR_MSG);
			exit(errct);
		}
	}
	if (errno != ENOENT) e(10);

	/* Try to create a dir */
	errno = 0;
	if (mkdir("testdir", 0777) == 0) {
		e(11);
		/* Uh oh. This shouldn't have been possible. Try to undo. */
		if (rmdir("testdir") != 0) {
			/* Not good. */
			e(12);
			printf(INTEGR_MSG);
			exit(errct);
		}
	}
	if (errno != ENOENT) e(13);

	/* Try to create a special file */
	errno = 0;
	if (mknod("testnode", 0777 | S_IFIFO, 0) == 0) {
		e(14);
		/* Impossible. Try to make it unhappen. */
		if (unlink("testnode") != 0) {
			/* Not good. */
			e(15);
			printf(INTEGR_MSG);
			exit(errct);
		}
	}
	if (errno != ENOENT) e(16);

	/* Try to rename a file */
	errno = 0;
	/* First create a file in the test dir */
	snprintf(renamebuf, PATH_MAX, "%s/oldname", testroot);
	if ((fd = open(renamebuf, O_RDWR | O_CREAT)) == -1) e(17);
	if (close(fd) != 0) e(18);

	/* Now try to rename that file to an entry in the current, non-existing
	 * working directory.
	 */
	if (rename(renamebuf, "testrename") == 0) {
		e(19);
		/* This shouldn't have been possible. Revert the name change.
		 */
		if (rename("testrename", renamebuf) != 0) {
			/* Failed */
			e(20);
			printf(INTEGR_MSG);
			exit(errct);
		}
	}

	/* Try to create a hard link to that file */
	errno = 0;
	if (link(renamebuf, "testhlink") == 0) {
		e(21);
		/* Try to undo the hard link to prevent fs corruption. */
		if (unlink("testhlink") != 0) {
			/* Failed. */
			e(22);
			printf(INTEGR_MSG);
			exit(errct);
		}
	}
	if (errno != ENOENT) e(23);

	/* Try to create a symlink */
	errno = 0;
	if (symlink(testroot, "testslink") == 0) {
		e(24);
		/* Try to remove the symlink to prevent fs corruption. */
		if (unlink("testslink") != 0) {
			/* Failed. */
			e(25);
			printf(INTEGR_MSG);
			exit(errct);
		}
	}
	if (errno != ENOENT) e(26);

	exit(errct);
  } else { /* Parent */
  	int status;

  	/* Wait for the child to enter the TEST_PATH dir */
  	if (read(s[1], buf, sizeof(buf)) != sizeof(buf)) e(27);

  	/* Delete TEST_PATH */
  	if (rmdir(TEST_PATH) != 0) e(28);

  	/* Tell child we removed its cwd */
  	buf[0] = 'b';
  	if (write(s[1], buf, sizeof(buf)) != sizeof(buf)) e(29);

  	wait(&status);
  	errct += WEXITSTATUS(status);	/* Count errors */
  }
}

int main(int argc, char* argv[])
{
  start(58);
  do_test();
  quit();
  return(-1);	/* Unreachable */
}


