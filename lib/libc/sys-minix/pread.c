#include <sys/cdefs.h>
#include "namespace.h"
#include <lib.h>

#include <unistd.h>

#ifdef __weak_alias
__weak_alias(pread, _pread)
#endif

#include <lib.h>
#include <unistd.h>
#include <minix/u64.h>

ssize_t pread64(int fd, void *buffer, size_t nbytes, u64_t where)
{
	u64_t here;
	ssize_t r;

	if (lseek64(fd, make64(0,0), SEEK_CUR, &here) < 0) return(-1);
	if (lseek64(fd, where, SEEK_SET, NULL) < 0) return(-1);
	if ((r = read(fd, buffer, nbytes)) < 0) {
		int e = errno;
		lseek64(fd, here, SEEK_SET, NULL);
		errno = e;
		return(-1);
	}

	if (lseek64(fd, here, SEEK_SET, NULL) < 0) return(-1);

	return(r);
}

ssize_t pread(int fd, void *buffer, size_t nbytes, off_t where)
{
	off_t here;
	ssize_t r;

	if((here = lseek(fd, 0, SEEK_CUR)) < 0)
		return -1;

	if(lseek(fd, where, SEEK_SET) < 0)
		return -1;

	if((r=read(fd, buffer, nbytes)) < 0) {
		int e = errno;
		lseek(fd, here, SEEK_SET);
		errno = e;
		return -1;
	}

	if(lseek(fd, here, SEEK_SET) < 0)
		return -1;

	return r;
}

