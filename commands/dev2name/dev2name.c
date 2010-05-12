
/* Translate internal FS device number to a /dev/ name. */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <minix/config.h>
#include <minix/const.h>

#define PATH_DEV "/dev"

int
main(int argc, char *argv[])
{
	DIR *dev;
	struct dirent *e;
	int dev_n;
	if(argc <= 1 || argc > 3) {
		fprintf(stderr, "Usage: \n"
			"%s <major> <minor>\n"
			"%s <devicenumber>\n", argv[0], argv[0]);
		return 1;
	} else if(argc == 2) dev_n = atoi(argv[1]);
	else if(argc == 3) dev_n = (atoi(argv[1]) << MAJOR) | atoi(argv[2]);

	if(chdir(PATH_DEV) < 0) {
		perror(PATH_DEV " chdir");
		return 1;
	}

	if(!(dev=opendir("."))) {
		perror(". in " PATH_DEV);
		return 1;
	}

	while((e=readdir(dev))) {
		struct stat st;
		if(stat(e->d_name, &st) < 0) {
			continue;
		}
		if((st.st_mode & (S_IFBLK | S_IFCHR)) && dev_n == st.st_rdev) {
			printf("%s/%s\n", PATH_DEV, e->d_name);
			return 0;
		}
	}

	return 1;
}

