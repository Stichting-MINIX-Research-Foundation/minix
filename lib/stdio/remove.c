/*
 * remove.c - remove a file
 */
/* $Header$ */

#include	<stdio.h>
#include	<errno.h>

int _rmdir(const char *path);
int _unlink(const char *path);

int
remove(const char *filename) {
	int saved_errno, retval;

	saved_errno = errno;

	retval = _rmdir(filename);

	if (retval == -1 && errno == ENOTDIR) {
		errno = saved_errno;

		retval = _unlink(filename);
	}

	return retval;
}
