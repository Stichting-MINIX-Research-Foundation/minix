/*	$NetBSD: pread.c,v 1.1.1.1 2013/03/23 15:49:15 christos Exp $	*/

#include "file.h"
#ifndef lint
#if 0
FILE_RCSID("@(#)$File: pread.c,v 1.1 2013/02/18 15:40:59 christos Exp $")
#else
__RCSID("$NetBSD: pread.c,v 1.1.1.1 2013/03/23 15:49:15 christos Exp $");
#endif
#endif  /* lint */
#include <fcntl.h>
#include <unistd.h>

ssize_t
pread(int fd, void *buf, ssize_t len, off_t off) {
	if (lseek(fd, off, SEEK_SET) == (off_t)-1)
		return -1;

	return read(fd, buf, len);
}
