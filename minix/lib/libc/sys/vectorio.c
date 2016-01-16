#include <sys/cdefs.h>
#include <lib.h>
#include "namespace.h"

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/uio.h>
#include <unistd.h>

/*
 * Create a single temporary buffer for the entire vector.  For writes, also
 * copy the actual data into the temporary buffer.
 */
ssize_t
_vectorio_setup(const struct iovec * iov, int iovcnt, char ** ptr, int op)
{
	char *buffer;
	ssize_t totallen, copied;
	int i;

	/* Parameter sanity checks. */
	if (iovcnt < 0 || iovcnt > IOV_MAX) {
		errno = EINVAL;
		return -1;
	}

	totallen = 0;
	for (i = 0; i < iovcnt; i++) {
		/* Do not read/write anything in case of possible overflow. */
		if ((size_t)SSIZE_MAX - totallen < iov[i].iov_len) {
			errno = EINVAL;
			return -1;
		}
		totallen += iov[i].iov_len;

		/* Report on NULL pointers. */
		if (iov[i].iov_len > 0 && iov[i].iov_base == NULL) {
			errno = EFAULT;
			return -1;
		}
	}

	/* Anything to do? */
	if (totallen == 0) {
		*ptr = NULL;
		return 0;
	}

	/* Allocate a temporary buffer. */
	buffer = (char *)malloc(totallen);
	if (buffer == NULL)
		return -1;

	/* For writes, copy over the buffer contents before the call. */
	if (op == _VECTORIO_WRITE) {
		copied = 0;
		for (i = 0; i < iovcnt; i++) {
			memcpy(buffer + copied, iov[i].iov_base,
			    iov[i].iov_len);
			copied += iov[i].iov_len;
		}
		assert(copied == totallen);
	}

	/* Return the temporary buffer and its size. */
	*ptr = buffer;
	return totallen;
}

/*
 * Clean up the temporary buffer created for the vector.  For successful reads,
 * also copy out the retrieved buffer contents.
 */
void
_vectorio_cleanup(const struct iovec * iov, int iovcnt, char * buffer,
	ssize_t r, int op)
{
	int i, errno_saved;
	ssize_t copied, len;

	/* Make sure to retain the original errno value in case of failure. */
	errno_saved = errno;

	/*
	 * If this was for a read and the read call succeeded, copy out the
	 * resulting data.
	 */
	if (op == _VECTORIO_READ && r > 0) {
		assert(buffer != NULL);
		copied = 0;
		i = 0;
		while (copied < r) {
			assert(i < iovcnt);
			len = iov[i].iov_len;
			if (len > r - copied)
				len = r - copied;
			memcpy(iov[i++].iov_base, buffer + copied, len);
			copied += len;
		}
		assert(r < 0 || r == copied);
	}

	/* Free the temporary buffer. */
	if (buffer != NULL)
		free(buffer);

	errno = errno_saved;
}

/*
 * Read a vector.
 */
ssize_t
readv(int fd, const struct iovec * iov, int iovcnt)
{
	char *ptr;
	ssize_t r;

	/*
	 * There ought to be just a readv system call here.  Instead, we use an
	 * intermediate buffer.  This approach is preferred over multiple read
	 * calls, because the actual I/O operation has to be atomic.
	 */
	if ((r = _vectorio_setup(iov, iovcnt, &ptr, _VECTORIO_READ)) <= 0)
		return r;

	r = read(fd, ptr, r);

	_vectorio_cleanup(iov, iovcnt, ptr, r, _VECTORIO_READ);

	return r;
}

/*
 * Write a vector.
 */
ssize_t
writev(int fd, const struct iovec * iov, int iovcnt)
{
	char *ptr;
	ssize_t r;

	/*
	 * There ought to be just a writev system call here.  Instead, we use
	 * an intermediate buffer.  This approach is preferred over multiple
	 * write calls, because the actual I/O operation has to be atomic.
	 */
	if ((r = _vectorio_setup(iov, iovcnt, &ptr, _VECTORIO_WRITE)) <= 0)
		return r;

	r = write(fd, ptr, r);

	_vectorio_cleanup(iov, iovcnt, ptr, r, _VECTORIO_WRITE);

	return r;
}
