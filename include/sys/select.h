
#ifndef _SYS_SELECT_H
#define _SYS_SELECT_H 1

#ifdef _POSIX_SOURCE

#include <sys/time.h>
#include <sys/types.h>
#include <limits.h>

/* Use this datatype as basic storage unit in fd_set */
typedef u32_t _fdsetword;	

/* This many bits fit in an fd_set word. */
#define _FDSETBITSPERWORD	(sizeof(_fdsetword)*8)

/* We want to store OPEN_MAX fd bits. */
#define _FDSETWORDS	((OPEN_MAX+_FDSETBITSPERWORD-1)/_FDSETBITSPERWORD)

/* This means we can store all of OPEN_MAX. */
#define FD_SETSIZE		OPEN_MAX

typedef struct {
	_fdsetword	_fdsetval[_FDSETWORDS];
} fd_set;

_PROTOTYPE( int select, (int nfds, fd_set *readfds, fd_set *writefds, fd_set *errorfds, struct timeval *timeout) );

_PROTOTYPE( void FD_CLR, (int fd, fd_set *fdset));
_PROTOTYPE( int FD_ISSET, (int fd, fd_set *fdset));
_PROTOTYPE( void FD_SET, (int fd, fd_set *fdset));
_PROTOTYPE( void FD_ZERO, (fd_set *fdset));

/* possible select() operation types; read, write, errors */
/* (FS/driver internal use only) */
#define SEL_RD		(1 << 0)
#define SEL_WR		(1 << 1)
#define SEL_ERR		(1 << 2)
#define SEL_NOTIFY	(1 << 3) /* not a real select operation */

#endif /* _POSIX_SOURCE */

#endif /* _SYS_SELECT_H */

