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

#define ACTION_CID "cid"
#define ACTION_LOGLEVEL "loglevel"

static void
show_usage()
{
	fprintf(stderr, "Usage: mmcctl device action arguments\n");
	fprintf(stderr, " device \n");
	fprintf(stderr, "  The path to the mmc device to use\n");
	fprintf(stderr, " action cid \n");
	fprintf(stderr, "  cid print the CID of the device \n");
	fprintf(stderr, " action setloglevel [0-4]\n");
	fprintf(stderr,
	    "  setloglevel sets the logging level of the driver\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "examples:\n");
	fprintf(stderr, " mmcctl /dev/mmc0 cid\n");
	fprintf(stderr, " mmcctl /dev/mmc0 setloglevel 3\n");
}

int
do_cid(const char *device, const char *action, int optargc,
    const char **optargv)
{
	int err;
	int fd;
	fprintf(stdout, "getting cid\n");
	fd = open(device, O_RDONLY);
	if (fd == -1) {
		fprintf(stderr, "error can not open device '%s':%s\n",
		    device, strerror(errno));
		return EXIT_FAILURE;
	}

	uint8_t cid[16];
	err = ioctl(fd, MMCIOC_GETCID, cid);
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

int
do_setloglevel(const char *device, const char *action, int optargc,
    const char **optargv)
{
	int err;
	int fd;
	fd = open(device, O_RDONLY);
	if (fd == -1) {
		fprintf(stderr, "error can not open device '%s':%s\n",
		    device, strerror(errno));
		return EXIT_FAILURE;
	}

	if (optargc != 1) {
		fprintf(stderr,
		    "**setloglevel requires an additional parameter**\n");
		show_usage();
		return EXIT_FAILURE;
	}
	uint32_t level = atoi(optargv[0]);
	if (level < 0 || level > 4) {
		fprintf(stderr,
		    "log level must be a value between 0 (none) and 4(trace)\n");
		return EXIT_FAILURE;
	}
	fprintf(stdout, "setting log level to %d\n",level);
	err = ioctl(fd, MMCIOC_SETLOGLEVEL, &level);
	if (err) {
		fprintf(stderr,
		    "error doing ioctl on device '%s':%s\n", device,
		    strerror(errno));
		close(fd);
		return EXIT_FAILURE;
	}
	close(fd);
	return EXIT_SUCCESS;
}

int
main(int argc, const char **argv)
{
	const char *device;
	const char *action;
	int optargc;
	const char **optargv;
	device = action = NULL;

	if (argc < 3) {
		fprintf(stderr, "**Two few of arguments passed**\n");
		show_usage();
		return EXIT_SUCCESS;
	}

	device = argv[1];
	action = argv[2];
	optargv = argv + 3;
	optargc = argc - 3;

	if (device == NULL || action == NULL) {
		show_usage();
		return EXIT_FAILURE;
	}

	if (strncmp(action, ACTION_CID, strlen(ACTION_CID) + 1) == 0) {
		return do_cid(device, action, optargc, optargv);
	} else if (strncmp(action, ACTION_LOGLEVEL,
		strlen(ACTION_LOGLEVEL) + 1) == 0) {
		return do_setloglevel(device, action, optargc, optargv);
	}
	show_usage();
	fprintf(stderr, "**Action %s not recognized**\n", action);
	return EXIT_FAILURE;
}
