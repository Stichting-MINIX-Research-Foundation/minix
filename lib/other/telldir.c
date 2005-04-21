/*	telldir()					Author: Kees J. Bot
 *								24 Apr 1989
 */
#define nil 0
#include <lib.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>

off_t telldir(DIR *dp)
/* Return the current read position in a directory. */
{
	if (dp == nil) { errno= EBADF; return -1; }

	return dp->_pos;
}
