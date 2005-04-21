
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

int
main(int argc, char *argv[])
{
	if(argc != 3) {
		fprintf(stderr, "usage: %s <root> <command>\n", argv[0]);
		return 1;
	}

	if(chroot(argv[1]) < 0) {
		perror("chroot");
		return 1;
	}

	system(argv[2]);

	return 0;
}

