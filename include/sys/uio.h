/*
sys/uio.h

definitions for vector I/O operations
*/

#ifndef _SYS_UIO_H
#define _SYS_UIO_H

/* Open Group Base Specifications Issue 6 (not complete) */

struct iovec
{
	void	*iov_base;
	size_t	iov_len;
};

_PROTOTYPE(ssize_t readv, (int _fildes, const struct iovec *_iov,
							int _iovcnt)	);
_PROTOTYPE(ssize_t writev, (int _fildes, const struct iovec *_iov,
							int iovcnt)	);

#endif /* _SYS_UIO_H */
