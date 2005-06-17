/*
readlink.c
*/

#include <unistd.h>
#include <errno.h>

int readlink(const char *path, char *buf, int bufsiz)
{
	errno = EINVAL;	/* "The named file is not a symbolic link" */
	return -1;
}
