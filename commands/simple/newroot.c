/*
newroot.c

Replace the current root with a new one
*/

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>

void usage(void) {
	fprintf(stderr, "Usage: newroot [-i] <block-special>\n");
	fprintf(stderr, "-i: copy mfs binary from boot image to memory\n");
	exit(1);	
}

int main(int argc, char *argv[])
{
	int r;
	char *dev;
	int mountflags;

	r = 0;
	mountflags = 0; /* !read-only */
	
	if (argc != 2 && argc != 3) usage();
	if(argc == 2) {
		dev = argv[1];
	} else if(argc == 3) {
		/* -i flag was supposedly entered. Verify.*/		
		if(strcmp(argv[1], "-i") != 0) usage();
		mountflags |= MS_REUSE;
		dev = argv[2];
	}
	
	r = mount(dev, "/", mountflags, NULL, NULL);
	if (r != 0) {
		fprintf(stderr, "newroot: mount failed: %s\n",strerror(errno));
		exit(1);
	}
	
	return 0;
}
