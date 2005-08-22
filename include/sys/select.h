#ifndef _SYS_SELECT_H
#define _SYS_SELECT_H 1

#include <sys/time.h>
#include <sys/types.h>
#include <limits.h>
#include <string.h>

/* Use this datatype as basic storage unit in fd_set */
typedef u32_t fd_mask;	

/* This many bits fit in an fd_set word. */
#define _FDSETBITSPERWORD	(sizeof(fd_mask)*8)

/* Bit manipulation macros */
#define _FD_BITMASK(b)	(1L << ((b) % _FDSETBITSPERWORD))
#define _FD_BITWORD(b)	((b)/_FDSETBITSPERWORD)

/* Default FD_SETSIZE is OPEN_MAX. */
#ifndef FD_SETSIZE
#define FD_SETSIZE		OPEN_MAX
#endif

/* We want to store FD_SETSIZE bits. */
#define _FDSETWORDS	((FD_SETSIZE+_FDSETBITSPERWORD-1)/_FDSETBITSPERWORD)

typedef struct {
	fd_mask	fds_bits[_FDSETWORDS];
} fd_set;

_PROTOTYPE( int select, (int nfds, fd_set *readfds, fd_set *writefds, fd_set *errorfds, struct timeval *timeout) );

#define FD_ZERO(s) do { int _i; for(_i = 0; _i < _FDSETWORDS; _i++) { (s)->fds_bits[_i] = 0; } } while(0)
#define FD_SET(f, s) do { (s)->fds_bits[_FD_BITWORD(f)] |= _FD_BITMASK(f); } while(0)
#define FD_CLR(f, s) do { (s)->fds_bits[_FD_BITWORD(f)] &= ~(_FD_BITMASK(f)); } while(0)
#define FD_ISSET(f, s) ((s)->fds_bits[_FD_BITWORD(f)] & _FD_BITMASK(f))

/* possible select() operation types; read, write, errors */
/* (FS/driver internal use only) */
#define SEL_RD		(1 << 0)
#define SEL_WR		(1 << 1)
#define SEL_ERR		(1 << 2)
#define SEL_NOTIFY	(1 << 3) /* not a real select operation */

#endif /* _SYS_SELECT_H */

