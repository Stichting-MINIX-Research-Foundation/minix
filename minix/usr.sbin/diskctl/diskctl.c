/* diskctl - control disk device driver parameters - by D.C. van Moolenbroek */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>

static void __dead
usage(void)
{
	fprintf(stderr,
	    "usage: %s <device> <command> [args]\n"
	    "\n"
	    "supported commands:\n"
	    "  getwcache           return write cache status\n"
	    "  setwcache [on|off]  set write cache status\n"
	    "  flush               flush write cache\n",
	    getprogname());

	exit(EXIT_FAILURE);
}

static int
open_dev(const char * dev, int flags)
{
	int fd;

	fd = open(dev, flags);

	if (fd < 0) {
		perror("open");

		exit(EXIT_FAILURE);
	}

	return fd;
}

int
main(int argc, char ** argv)
{
	int fd, val;

	setprogname(argv[0]);

	if (argc < 3) usage();

	if (!strcasecmp(argv[2], "getwcache")) {
		if (argc != 3) usage();

		fd = open_dev(argv[1], O_RDONLY);

		if (ioctl(fd, DIOCGETWC, &val) != 0) {
			perror("ioctl");

			return EXIT_FAILURE;
		}

		close(fd);

		printf("write cache is %s\n", val ? "on" : "off");

	} else if (!strcasecmp(argv[2], "setwcache")) {
		if (argc != 4) usage();

		if (!strcasecmp(argv[3], "on"))
			val = 1;
		else if (!strcasecmp(argv[3], "off"))
			val = 0;
		else
			usage();

		fd = open_dev(argv[1], O_WRONLY);

		if (ioctl(fd, DIOCSETWC, &val) != 0) {
			perror("ioctl");

			return EXIT_FAILURE;
		}

		close(fd);

		printf("write cache %sabled\n", val ? "en" : "dis");

	} else if (!strcasecmp(argv[2], "flush")) {
		if (argc != 3) usage();

		fd = open_dev(argv[1], O_WRONLY);

		if (ioctl(fd, DIOCFLUSH, NULL) != 0) {
			perror("ioctl");

			return EXIT_FAILURE;
		}

		close(fd);

		printf("write cache flushed\n");

	} else
		usage();

	return EXIT_SUCCESS;
}
