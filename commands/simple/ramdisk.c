
#include <minix/paths.h>

#include <sys/ioc_memory.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>

int
main(int argc, char *argv[])
{
	int fd;
	signed long size;
	if((fd=open(_PATH_RAMDISK, O_RDONLY)) < 0) {
		perror(_PATH_RAMDISK);
		return 1;
	}

	if(argc != 2) {
		fprintf(stderr, "usage: %s <size in bytes>\n", argv[0]);
		return 1;
	}

	size = atol(argv[1]);

	if(size <= 0) {
		fprintf(stderr, "size should be positive.\n");
		return 1;
	}

	if(ioctl(fd, MIOCRAMSIZE, &size) < 0) {
		perror("MIOCRAMSIZE");
		return 1;
	}

	return 0;
}

