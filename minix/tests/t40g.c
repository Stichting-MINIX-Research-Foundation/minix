/* t40g.c
 *
 * Test select on character driver that does not support select
 *
 * We use /dev/zero for this right now.  If the memory driver ever implements
 * select support, this test should be changed to use another character driver.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>

#include "common.h"

int
main(int argc, char **argv)
{
	fd_set set;
	int fd, retval;

	/* Get subtest number */
	if (argc != 2 || sscanf(argv[1], "%d", &subtest) != 1) {
		printf("Usage: %s subtest_no\n", argv[0]);
		exit(-2);
	}

	/*
	 * Do a select on /dev/zero, with the expectation that it will fail
	 * with an EBADF error code.
	 */
	fd = open("/dev/zero", O_RDONLY);
	if (fd < 0) em(1, "unable to open /dev/zero");

	FD_ZERO(&set);
	FD_SET(fd, &set);

	retval = select(fd + 1, &set, NULL, NULL, NULL);
	if (retval != -1) em(2, "select call was expected to fail");
	if (errno != EBADF) em(3, "error code other than EBADF returned");
	if (!FD_ISSET(fd, &set)) em(4, "file descriptor set was modified");

	exit(errct);
}
