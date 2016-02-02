/*	$NetBSD: pread.c,v 1.1.1.5 2015/01/02 20:34:27 christos Exp $	*/

#include "file.h"
#ifndef lint
#if 0
FILE_RCSID("@(#)$File: pread.c,v 1.3 2014/09/15 19:11:25 christos Exp $")
#else
__RCSID("$NetBSD: pread.c,v 1.1.1.5 2015/01/02 20:34:27 christos Exp $");
#endif
#endif  /* lint */
#include <fcntl.h>
#include <unistd.h>

ssize_t
pread(int fd, void *buf, size_t len, off_t off) {
	off_t old;
	ssize_t rv;

	if ((old = lseek(fd, off, SEEK_SET)) == -1)
		return -1;

	if ((rv = read(fd, buf, len)) == -1)
		return -1;

	if (lseek(fd, old, SEEK_SET) == -1)
		return -1;

	return rv;
}
