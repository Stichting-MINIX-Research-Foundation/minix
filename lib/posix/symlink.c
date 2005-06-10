/*
symlink.c
*/

#include <errno.h>
#include <unistd.h>

int symlink(const char *path1, const char *path2)
{
	errno= ENOSYS;
	return -1;
}
