#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include <sys/ioc_mmc.h>

static void
show_usage()
{
	fprintf(stderr, "Usage: mmcctl device action\n");
}

int
main(int argc, char **argv)
{
	int err;
	int fd;
	const char *device;
	const char *action;
	struct stat stats;

	if (argc != 3) {
		show_usage();
		return EXIT_FAILURE;
	}
	device = argv[1];
	action = argv[2];

	err = stat(device, &stats);
	if (err == -1) {
		fprintf(stderr, "error can not open device '%s':%s\n", device,
		    strerror(errno));
		return EXIT_FAILURE;
	}

	if (!S_ISBLK(stats.st_mode)) {
		fprintf(stderr, "error device '%s' is not a block device\n",
		    device);
		return EXIT_FAILURE;
	}

	if (strncmp(action, "cid", 4) == 0) {
		fprintf(stdout, "getting cid\n");
		fd = open(device, O_RDONLY);
		if (fd == -1) {
			fprintf(stderr, "error can not open device '%s':%s\n",
			    device, strerror(errno));
			return EXIT_FAILURE;
		}

		uint8_t cid[16];
		err = ioctl(fd, MIOGETCID, cid);
		if (err) {
			fprintf(stderr,
			    "error doing ioctl on device '%s':%s\n", device,
			    strerror(errno));
			close(fd);
			return EXIT_FAILURE;
		}
		printf("CID:"
		    "%02X%02X%02X%02X"
		    "%02X%02X%02X%02X"
		    "%02X%02X%02X%02X"
		    "%02X%02X%02X%02X\n", cid[0], cid[1], cid[2], cid[3],
		    cid[4], cid[5], cid[6], cid[7], cid[8], cid[9], cid[10],
		    cid[11], cid[12], cid[13], cid[14], cid[15]);

		close(fd);
		return EXIT_SUCCESS;
	}

	show_usage();
	return EXIT_FAILURE;
}
