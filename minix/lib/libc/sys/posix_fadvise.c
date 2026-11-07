/*
 * posix_fadvise(2) for MINIX.
 *
 * MINIX has no read-ahead or cache-dropping policy to tune, and POSIX lets
 * an implementation ignore the advice, so this only validates the
 * arguments.  Like NetBSD's, it returns an error number rather than
 * setting errno.
 */

#include <sys/cdefs.h>
#include "namespace.h"

#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>

int
posix_fadvise(int fd, off_t offset, off_t len, int advice)
{
	struct stat st;

	(void)offset;

	if (fstat(fd, &st) != 0)
		return EBADF;
	if (S_ISFIFO(st.st_mode) || S_ISSOCK(st.st_mode))
		return ESPIPE;
	if (len < 0)
		return EINVAL;
	switch (advice) {
	case POSIX_FADV_NORMAL:
	case POSIX_FADV_RANDOM:
	case POSIX_FADV_SEQUENTIAL:
	case POSIX_FADV_WILLNEED:
	case POSIX_FADV_DONTNEED:
	case POSIX_FADV_NOREUSE:
		return 0;
	default:
		return EINVAL;
	}
}
