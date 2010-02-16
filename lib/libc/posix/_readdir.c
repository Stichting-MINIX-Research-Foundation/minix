/*	readdir()					Author: Kees J. Bot
 *								24 Apr 1989
 */
#define nil 0
#define readdir _readdir
#define getdents _getdents
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <stddef.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

struct dirent *readdir(DIR *dp)
/* Return the next entry in a directory. */
{
	struct dirent *entp;
	int count, pos, reclen;

	if (dp == nil) { errno= EBADF; return nil; }

	count= dp->_count;
	pos= dp->_pos;
	if (count == 0 || pos >= count)
	{
		count= getdents(dp->_fd, (struct dirent *)dp->_buf,
			sizeof(dp->_buf));
		if (count <= 0) return nil;
		dp->_count= count;
		dp->_pos= pos= 0;
	}
	entp= (struct dirent *)&((char *)dp->_buf)[pos];
	reclen= entp->d_reclen;
	dp->_pos= pos+reclen;

	return entp;
}

/*
 * $PchId: _readdir.c,v 1.6 2005/01/27 21:46:42 philip Exp $
 */
