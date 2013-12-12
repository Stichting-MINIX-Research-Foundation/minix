#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/syslimits.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

int max_error = 5;
#include "common.h"


void dangling_slink(int sub_test, char const slink_to[PATH_MAX]);

void dangling_slink(int sub_test, char const slink_to[PATH_MAX])
{
  pid_t child;

  subtest = sub_test;

  child = fork();
  if (child == (pid_t) -1) {
	e(1);
	return;
  } else if (child == (pid_t) 0) {
	/* I'm the child. Create a dangling symlink with an absolute path */
	int fd;
	char buf[4];


	/* We don't want to actually write to '/', so instead we pretend */
	if (chroot(".") != 0) e(2);

	/* Create file 'slink_to' with contents "bar" */
	if ((fd = open(slink_to, O_CREAT|O_WRONLY)) == -1) e(3);
	if (write(fd, "bar", strlen("bar")) != strlen("bar")) e(4);
	close(fd);

	if (symlink(slink_to, "a") == -1) e(5); /* Create the symlink */
	if (rename(slink_to, "c") == -1) e(6); /* Make it a dangling symlink */

	/* Write "foo" to symlink; this should recreate file 'slink_to' with
	 * contents "foo" */
	if ((fd = open("a", O_CREAT|O_WRONLY)) == -1) e(7);
	if (write(fd, "foo", strlen("foo")) != strlen("foo")) e(8);
	close(fd);

	/* Verify 'a' and 'slink_to' contain "foo" */
	memset(buf, '\0', sizeof(buf));
	if ((fd = open("a", O_RDONLY)) == -1) e(9);
	if (read(fd, buf, 3) != 3) e(10);
	if (strncmp(buf, "foo", strlen("foo"))) e(11);
	close(fd);
	memset(buf, '\0', sizeof(buf));
	if ((fd = open(slink_to, O_RDONLY)) == -1) e(12);
	if (read(fd, buf, 3) != 3) e(13);
	if (strncmp(buf, "foo", strlen("foo"))) e(14);
	close(fd);

	/* Verify 'c' contains 'bar' */
	memset(buf, '\0', sizeof(buf));
	if ((fd = open("c", O_RDONLY)) == -1) e(15);
	if (read(fd, buf, 3) != 3) e(16);
	if (strncmp(buf, "bar", strlen("bar"))) e(17);
	close(fd);

	/* Cleanup created files */
	if (unlink(slink_to) == -1) e(18);
	if (unlink("a") == -1) e(19);
	if (unlink("c") == -1) e(20);

	exit(EXIT_SUCCESS);
  } else {
	int status;
	if (wait(&status) == -1) e(7);
  }


}

int main(int argc, char *argv[])
{
  start(61);
  dangling_slink(1, "/abs"); /* Create dangling symlink with absolute path */
  dangling_slink(2, "rel"); /* Create dangling symlink with relative path */
  quit();
  return(-1);	/* Unreachable */
}

