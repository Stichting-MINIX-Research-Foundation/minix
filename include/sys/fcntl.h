#ifndef _SYS_FCNTL_H_
#define	_SYS_FCNTL_H_

/*
 * This file includes the definitions for open and fcntl
 * described by POSIX for <fcntl.h>.
 */
#include <sys/featuretest.h>
#include <sys/types.h>
#if defined(_XOPEN_SOURCE) || defined(_NETBSD_SOURCE)
#include <sys/stat.h>
#endif /* _XOPEN_SOURCE || _NETBSD_SOURCE */

/*
 * File status flags: these are used by open(2), fcntl(2).
 * They are also used (indirectly) in the kernel file structure f_flags,
 * which is a superset of the open/fcntl flags.  Open flags and f_flags
 * are inter-convertible using OFLAGS(fflags) and FFLAGS(oflags).
 * Open/fcntl flags begin with O_; kernel-internal flags begin with F.
 */
/* open-only flags */
#define	O_RDONLY	0x00000000	/* open for reading only */
#define	O_WRONLY	0x00000001	/* open for writing only */
#define	O_RDWR		0x00000002	/* open for reading and writing */
#define	O_ACCMODE	0x00000003	/* mask for above modes */

/* File status flags for open() and fcntl().  POSIX Table 6-5. */
#define O_APPEND       02000	/* set append mode */
#define O_NONBLOCK     04000	/* no delay */
#define O_REOPEN      010000	/* automatically re-open device after driver
				 * restart
				 */


#ifndef __minix  /* NOT SUPPORTED! */
#if defined(_NETBSD_SOURCE)
#define	O_SHLOCK	0x00000010	/* open with shared file lock */
#define	O_EXLOCK	0x00000020	/* open with exclusive file lock */
#define	O_ASYNC		0x00000040	/* signal pgrp when data ready */
#endif
#if (_POSIX_C_SOURCE - 0) >= 199309L || \
    (defined(_XOPEN_SOURCE) && defined(_XOPEN_SOURCE_EXTENDED)) || \
    (_XOPEN_SOURCE - 0) >= 500 || defined(_NETBSD_SOURCE)
#define	O_SYNC		0x00000080	/* synchronous writes */
#endif
#if defined(_NETBSD_SOURCE)
#define	O_NOFOLLOW	0x00000100	/* don't follow symlinks on the last */
					/* path component */
#endif
#endif /* !__minix */

/* Oflag values for open().  POSIX Table 6-4. */
#define O_CREAT        00100	/* creat file if it doesn't exist */
#define O_EXCL         00200	/* exclusive use flag */
#define O_NOCTTY       00400	/* do not assign a controlling terminal */
#define O_TRUNC        01000	/* truncate flag */

#ifndef __minix /* NOT SUPPORTED! */
#if (_POSIX_C_SOURCE - 0) >= 199309L || (_XOPEN_SOURCE - 0) >= 500 || \
    defined(_NETBSD_SOURCE)
#define	O_DSYNC		0x00010000	/* write: I/O data completion */
#define	O_RSYNC		0x00020000	/* read: I/O completion as for write */
#endif

#if defined(_NETBSD_SOURCE)
#define	O_ALT_IO	0x00040000	/* use alternate i/o semantics */
#define	O_DIRECT	0x00080000	/* direct I/O hint */
#endif
#endif /* !__minix */

/*
 * Constants used for fcntl(2)
 */

/* command values */
/* These values are used for cmd in fcntl().  POSIX Table 6-1.  */
#define F_DUPFD            0	/* duplicate file descriptor */
#define F_GETFD	           1	/* get file descriptor flags */
#define F_SETFD            2	/* set file descriptor flags */
#define F_GETFL            3	/* get file status flags */
#define F_SETFL            4	/* set file status flags */
#define F_GETLK            5	/* get record locking information */
#define F_SETLK            6	/* set record locking information */
#define F_SETLKW           7	/* set record locking info; wait if blocked */
#define F_FREESP           8	/* free a section of a regular file */

/* File descriptor flags used for fcntl().  POSIX Table 6-2. */
#define FD_CLOEXEC         1	/* close on exec flag for third arg of fcntl */

/* record locking flags (F_GETLK, F_SETLK, F_SETLKW) */
#define F_RDLCK            1	/* shared or read lock */
#define F_WRLCK            2	/* exclusive or write lock */
#define F_UNLCK            3	/* unlock */

/*
 * Advisory file segment locking data type -
 * information passed to system by user
 */
struct flock {
  short l_type;			/* type: F_RDLCK, F_WRLCK, or F_UNLCK */
  short l_whence;		/* flag for starting offset */
  off_t l_start;		/* relative offset in bytes */
  off_t l_len;			/* size; if 0, then until EOF */
  pid_t l_pid;			/* process id of the locks' owner */
};

#if defined(_NETBSD_SOURCE)
/* lock operations for flock(2) */
#define LOCK_SH		F_RDLCK		/* Shared lock */
#define LOCK_EX		F_WRLCK		/* Exclusive lock */
#define LOCK_NB		0x0080		/* Do not block when locking */
#define LOCK_UN		F_UNLCK		/* Unlock */
#endif

/* Always ensure that these are consistent with <stdio.h> and <unistd.h>! */
#ifndef	SEEK_SET
#define	SEEK_SET	0	/* set file offset to offset */
#endif
#ifndef	SEEK_CUR
#define	SEEK_CUR	1	/* set file offset to current plus offset */
#endif
#ifndef	SEEK_END
#define	SEEK_END	2	/* set file offset to EOF plus offset */
#endif

#include <sys/cdefs.h>

__BEGIN_DECLS
int	open(const char *, int, ...);
int	creat(const char *, mode_t);
int	fcntl(int, int, ...);
#if defined(_NETBSD_SOURCE)
int	flock(int, int);
#endif /* _NETBSD_SOURCE */
__END_DECLS

#endif /* !_SYS_FCNTL_H_ */
