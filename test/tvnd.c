/* Tests for the VND driver API.  Part of testvnd.sh. */
#include <stdlib.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

/*
 * For now, just test the most dreaded case: the driver being told to use the
 * file descriptor used to configure the device.  Without appropriate checks,
 * this would cause a deadlock in VFS, since the corresponding filp object is
 * locked to perform the ioctl(2) call when VFS gets the copyfd(2) back-call.
 */
int
main(int argc, char **argv)
{
	struct vnd_ioctl vnd;
	int fd;

	if (argc != 2) {
		fprintf(stderr, "usage: %s /dev/vnd0\n", argv[0]);
		return EXIT_FAILURE;
	}

	if ((fd = open(argv[1], O_RDONLY)) < 0) {
		perror("open");
		return EXIT_FAILURE;
	}

	memset(&vnd, 0, sizeof(vnd));

	if (ioctl(fd, VNDIOCCLR, &vnd) < 0) {
		perror("ioctl(VNDIOCCLR)");
		return EXIT_FAILURE;
	}

	vnd.vnd_fildes = fd;

	if (ioctl(fd, VNDIOCSET, &vnd) >= 0) {
		fprintf(stderr, "ioctl(VNDIOCSET): unexpected success\n");
		return EXIT_FAILURE;
	}

	if (errno != EBADF) {
		perror("ioctl(VNDIOCSET)");
		return EXIT_FAILURE;
	}

	close(fd);

	return EXIT_SUCCESS;
}
