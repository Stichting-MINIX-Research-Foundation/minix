/*	seekdir()					Author: Kees J. Bot
 *								24 Apr 1989
 */
#include <lib.h>
#define lseek	_lseek
#define readdir	_readdir
#define seekdir	_seekdir
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>

int seekdir(DIR *dp, off_t pos)
/* Seek to position pos in a directory. */
{
	if (dp == NULL) { errno= EBADF; return -1; }

	dp->_count= 0;

	if (lseek(dp->_fd, pos, SEEK_SET) == -1) return -1;

	return 0;
}
