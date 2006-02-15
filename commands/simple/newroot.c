/*
newroot.c

Replace the current root with a new one
*/

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
	int r;
	char *dev;

	if (argc != 2)
	{
		fprintf(stderr, "Usage: newroot <block-special>\n");
		exit(1);
	}
	dev= argv[1];
	r= mount(dev, "/", 0 /* !ro */);
	if (r != 0)
	{
		fprintf(stderr, "newroot: mount failed: %s\n", strerror(errno));
		exit(1);
	}
	return 0;
}
