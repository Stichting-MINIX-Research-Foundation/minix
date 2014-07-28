#include <sys/types.h>
#include <sys/wait.h>
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

	/* If we open a new file, the fd we obtain should be fd_parent + 1 */
	fd = open("open_plusplus_fd", O_CREAT|O_RDWR, 0660);
	if (fd != fd_parent + 1) {
		return 2;
	}

	/* Also, writing to fd_parent should succeed */
	if (write(fd_parent, buf, sizeof(buf)) <= 0) {
		return 3;
	}

	return 0;
}

