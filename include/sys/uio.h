#ifndef _SYS_UIO_H_
#define	_SYS_UIO_H_

#include <machine/ansi.h>
#include <sys/featuretest.h>

#ifdef	_BSD_SIZE_T_
typedef	_BSD_SIZE_T_	size_t;
#undef	_BSD_SIZE_T_
#endif

#ifdef	_BSD_SSIZE_T_
typedef	_BSD_SSIZE_T_	ssize_t;
#undef	_BSD_SSIZE_T_
#endif

struct iovec {
	void	*iov_base;	/* Base address. */
	size_t	 iov_len;	/* Length. */
};

#if defined(_NETBSD_SOURCE)
/*
 * Limits
 */
/* Deprecated: use IOV_MAX from <limits.h> instead. */
#define UIO_MAXIOV	1024		/* max 1K of iov's */
#endif /* _NETBSD_SOURCE */

#include <sys/cdefs.h>

__BEGIN_DECLS
ssize_t	readv(int, const struct iovec *, int);
ssize_t	writev(int, const struct iovec *, int);
__END_DECLS

#endif /* !_SYS_UIO_H_ */
