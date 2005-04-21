/*	eject 1.3 - Eject removable media		Author: Kees J. Bot
 *								11 Dec 1993
 */
#define nil 0
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>

void fatal(char *label)
{
	fprintf(stderr, "eject: %s: %s\n", label, strerror(errno));
	exit(1);
}

void main(int argc, char **argv)
{
	char *device;
	int fd;

	if (argc != 2) {
		fprintf(stderr, "Usage: eject <device>\n");
		exit(1);
	}

	device= argv[1];

	/* Try to open it in whatever mode. */
	fd= open(device, O_RDONLY);
	if (fd < 0 && errno == EACCES) fd= open(device, O_WRONLY);
	if (fd < 0) fatal(device);

	/* Tell it to eject. */
	if (ioctl(fd, DIOCEJECT, nil) < 0) fatal(device);
	exit(0);
}
