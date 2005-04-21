/*	seekdir()					Author: Kees J. Bot
 *								24 Apr 1989
 */
#define nil 0
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
	int off;

	if (dp == nil) { errno= EBADF; return -1; }

	dp->_count= 0;
	dp->_ptr= dp->_buf;

	off= pos & (sizeof(dp->_buf) - 1);
	dp->_pos= pos - off;

	if (lseek(dp->_fd, dp->_pos, SEEK_SET) == -1) return -1;

	while (dp->_pos < pos && readdir(dp) != nil) {}

	return 0;
}
