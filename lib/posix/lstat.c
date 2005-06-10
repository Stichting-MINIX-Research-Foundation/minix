/*
lstat.c
*/

#define stat _stat

#include <sys/stat.h>

int lstat(const char *path, struct stat *sb)
{
	/* Without symlinks, lstat is equal to stat */
	return stat(path, sb);
}
