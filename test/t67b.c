#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int
main(int argc, char *argv[])
{
	int fd, fd_parent;
	char buf[1];

	if (argc != 2) {
		return 1;
	}

	fd_parent = atoi(argv[1]);

	/* Writing to fd_parent should fail as it has to be closed at this
	 * point */
	if (write(fd_parent, buf, sizeof(buf)) != -1) {
		return 2;
	}
	if (errno != EBADF) {
		return 3;
	}

	/* If we open a new file, the fd we obtain should be identical to
	 * fd_parent */
	fd = open("open_identical_fd", O_CREAT|O_RDWR, 0660);
	if (fd != fd_parent) {
		return 4;
	}

	return 0;
}

