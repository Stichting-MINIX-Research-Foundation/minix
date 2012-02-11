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

#define VECTORIO_READ	1
#define VECTORIO_WRITE	2

#ifdef __weak_alias
__weak_alias(writev, _writev)
__weak_alias(readv, _readv)
#endif

static ssize_t vectorio_buffer(int fildes, const struct iovec *iov, 
	int iovcnt, int readwrite, ssize_t totallen)
{
	char *buffer;
	int iovidx, errno_saved;
	ssize_t copied, len, r;

	/* allocate buffer */
	buffer = (char *) malloc(totallen);
	if (!buffer)
		return -1;

	/* perform the actual read/write for the entire buffer */
	switch (readwrite)
	{
		case VECTORIO_READ:
			/* first read, then copy buffers (only part read) */
			r = read(fildes, buffer, totallen);

			copied = 0;
			iovidx = 0;
			while (copied < r)
			{
				assert(iovidx < iovcnt);
				len = MIN(r - copied, iov[iovidx].iov_len);
				memcpy(iov[iovidx++].iov_base, buffer + copied, len);
				copied += len;
			}
			assert(r < 0 || r == copied);
			break;

		case VECTORIO_WRITE: 
			/* first copy buffers, then write */
			copied = 0;
			for (iovidx = 0; iovidx < iovcnt; iovidx++)
			{
				memcpy(buffer + copied, iov[iovidx].iov_base, 
					iov[iovidx].iov_len);
				copied += iov[iovidx].iov_len;
			}
			assert(copied == totallen);

			r = write(fildes, buffer, totallen);
			break;

		default:        
			assert(0);
			errno = EINVAL;
			r = -1;
	}

	/* free the buffer, keeping errno unchanged */
	errno_saved = errno;
	free(buffer);
	errno = errno_saved;

	return r;
}

static ssize_t vectorio(int fildes, const struct iovec *iov, 
	int iovcnt, int readwrite)
{
	int i;
	ssize_t totallen;

	/* parameter sanity checks */
	if (iovcnt > IOV_MAX)
	{
		errno = EINVAL;
		return -1;
	}

	totallen = 0;
	for (i = 0; i < iovcnt; i++)
	{
		/* don't read/write anything in case of possible overflow */
		if ((ssize_t) (totallen + iov[i].iov_len) < totallen)
		{
			errno = EINVAL;
			return -1;
		}
		totallen += iov[i].iov_len;

		/* report on NULL pointers */
		if (iov[i].iov_len && !iov[i].iov_base)
		{
			errno = EFAULT;
			return -1;
		}
	}

	/* anything to do? */
	if (totallen == 0)
		return 0;

	/* 
	 * there aught to be a system call here; instead we use an intermediate 
	 * buffer; this is preferred over multiple read/write calls because 
	 * this function has to be atomic
	 */
	return vectorio_buffer(fildes, iov, iovcnt, readwrite, totallen);
}

ssize_t readv(int fildes, const struct iovec *iov, int iovcnt)
{
	return vectorio(fildes, iov, iovcnt, VECTORIO_READ);	
}

ssize_t writev(int fildes, const struct iovec *iov, int iovcnt)
{
	return vectorio(fildes, iov, iovcnt, VECTORIO_WRITE);	
}

