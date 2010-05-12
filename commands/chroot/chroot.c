
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/wait.h>

int
main(int argc, char *argv[])
{
	int status;

	if(argc != 3) {
		fprintf(stderr, "usage: %s <root> <command>\n", argv[0]);
		return 1;
	}

	if(chroot(argv[1]) < 0) {
		perror("chroot");
		return 1;
	}

	status = system(argv[2]);
	if(WIFEXITED(status))
		return WEXITSTATUS(status);
	return 1;
}

