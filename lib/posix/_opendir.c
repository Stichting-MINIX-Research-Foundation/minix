/*	opendir()					Author: Kees J. Bot
 *								24 Apr 1989
 */
#define nil 0
#include <lib.h>
#define close	_close
#define fcntl	_fcntl
#define fstat	_fstat
#define open	_open
#define opendir	_opendir
#define stat	_stat
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>

DIR *opendir(const char *name)
/* Open a directory for reading. */
{
	int d, f;
	DIR *dp;
	struct stat st;

	/* Only read directories. */
	if (stat(name, &st) < 0) return nil;
	if (!S_ISDIR(st.st_mode)) { errno= ENOTDIR; return nil; }

	if ((d= open(name, O_RDONLY | O_NONBLOCK)) < 0) return nil;

	/* Check the type again, mark close-on-exec, get a buffer. */
	if (fstat(d, &st) < 0
		|| (errno= ENOTDIR, !S_ISDIR(st.st_mode))
		|| (f= fcntl(d, F_GETFD)) < 0
		|| fcntl(d, F_SETFD, f | FD_CLOEXEC) < 0
		|| (dp= (DIR *) malloc(sizeof(*dp))) == nil
	) {
		int err= errno;
		(void) close(d);
		errno= err;
		return nil;
	}

	dp->_fd= d;
	dp->_v7= -1;
	dp->_count= 0;
	dp->_pos= 0;

	return dp;
}

