/* The <errno.h> header defines the numbers of the various errors that can
 * occur during program execution.  They are visible to user programs and 
 * should be small positive integers.  However, they are also used within 
 * MINIX, where they must be negative.  For example, the READ system call is 
 * executed internally by calling do_read().  This function returns either a 
 * (negative) error number or a (positive) number of bytes actually read.
 *
 * To solve the problem of having the error numbers be negative inside the
 * the system and positive outside, the following mechanism is used.  All the
 * definitions are are the form:
 *
 *	#define EPERM		(_SIGN 1)
 *
 * If the macro _SYSTEM is defined, then  _SIGN is set to "-", otherwise it is
 * set to "".  Thus when compiling the operating system, the  macro _SYSTEM
 * will be defined, setting EPERM to (- 1), whereas when when this
 * file is included in an ordinary user program, EPERM has the value ( 1).
 */

#ifndef _ERRNO_H		/* check if <errno.h> is already included */
#define _ERRNO_H		/* it is not included; note that fact */

/* Now define _SIGN as "" or "-" depending on _SYSTEM. */
#ifdef _SYSTEM
#   define _SIGN         -
#   define OK            0
#else
#   define _SIGN         
#endif

extern int errno;		  /* place where the error numbers go */

/* Here are the numerical values of the error numbers. */
#define _NERROR               70  /* number of errors */  

#define EGENERIC      (_SIGN 99)  /* generic error */
#define EPERM         (_SIGN  1)  /* operation not permitted */
#define ENOENT        (_SIGN  2)  /* no such file or directory */
#define ESRCH         (_SIGN  3)  /* no such process */
#define EINTR         (_SIGN  4)  /* interrupted function call */
#define EIO           (_SIGN  5)  /* input/output error */
#define ENXIO         (_SIGN  6)  /* no such device or address */
#define E2BIG         (_SIGN  7)  /* arg list too long */
#define ENOEXEC       (_SIGN  8)  /* exec format error */
#define EBADF         (_SIGN  9)  /* bad file descriptor */
#define ECHILD        (_SIGN 10)  /* no child process */
#define EAGAIN        (_SIGN 11)  /* resource temporarily unavailable */
#define ENOMEM        (_SIGN 12)  /* not enough space */
#define EACCES        (_SIGN 13)  /* permission denied */
#define EFAULT        (_SIGN 14)  /* bad address */
#define ENOTBLK       (_SIGN 15)  /* Extension: not a block special file */
#define EBUSY         (_SIGN 16)  /* resource busy */
#define EEXIST        (_SIGN 17)  /* file exists */
#define EXDEV         (_SIGN 18)  /* improper link */
#define ENODEV        (_SIGN 19)  /* no such device */
#define ENOTDIR       (_SIGN 20)  /* not a directory */
#define EISDIR        (_SIGN 21)  /* is a directory */
#define EINVAL        (_SIGN 22)  /* invalid argument */
#define ENFILE        (_SIGN 23)  /* too many open files in system */
#define EMFILE        (_SIGN 24)  /* too many open files */
#define ENOTTY        (_SIGN 25)  /* inappropriate I/O control operation */
#define ETXTBSY       (_SIGN 26)  /* no longer used */
#define EFBIG         (_SIGN 27)  /* file too large */
#define ENOSPC        (_SIGN 28)  /* no space left on device */
#define ESPIPE        (_SIGN 29)  /* invalid seek */
#define EROFS         (_SIGN 30)  /* read-only file system */
#define EMLINK        (_SIGN 31)  /* too many links */
#define EPIPE         (_SIGN 32)  /* broken pipe */
#define EDOM          (_SIGN 33)  /* domain error    	(from ANSI C std) */
#define ERANGE        (_SIGN 34)  /* result too large	(from ANSI C std) */
#define EDEADLK       (_SIGN 35)  /* resource deadlock avoided */
#define ENAMETOOLONG  (_SIGN 36)  /* file name too long */
#define ENOLCK        (_SIGN 37)  /* no locks available */
#define ENOSYS        (_SIGN 38)  /* function not implemented */
#define ENOTEMPTY     (_SIGN 39)  /* directory not empty */

/* The following errors relate to networking. */
#define EPACKSIZE     (_SIGN 50)  /* invalid packet size for some protocol */
#define EOUTOFBUFS    (_SIGN 51)  /* not enough buffers left */
#define EBADIOCTL     (_SIGN 52)  /* illegal ioctl for device */
#define EBADMODE      (_SIGN 53)  /* badmode in ioctl */
#define EWOULDBLOCK   (_SIGN 54)
#define EBADDEST      (_SIGN 55)  /* not a valid destination address */
#define EDSTNOTRCH    (_SIGN 56)  /* destination not reachable */
#define EISCONN	      (_SIGN 57)  /* all ready connected */
#define EADDRINUSE    (_SIGN 58)  /* address in use */
#define ECONNREFUSED  (_SIGN 59)  /* connection refused */
#define ECONNRESET    (_SIGN 60)  /* connection reset */
#define ETIMEDOUT     (_SIGN 61)  /* connection timed out */
#define EURG	      (_SIGN 62)  /* urgent data present */
#define ENOURG	      (_SIGN 63)  /* no urgent data present */
#define ENOTCONN      (_SIGN 64)  /* no connection (yet or anymore) */
#define ESHUTDOWN     (_SIGN 65)  /* a write call to a shutdown connection */
#define ENOCONN       (_SIGN 66)  /* no such connection */
#define EAFNOSUPPORT  (_SIGN 67)  /* address family not supported */
#define EPROTONOSUPPORT (_SIGN 68) /* protocol not supported by AF */
#define EPROTOTYPE    (_SIGN 69)  /* Protocol wrong type for socket */
#define EINPROGRESS   (_SIGN 70)  /* Operation now in progress */
#define EADDRNOTAVAIL (_SIGN 71)  /* Can't assign requested address */
#define EALREADY      (_SIGN 72)  /* Connection already in progress */
#define EMSGSIZE      (_SIGN 73)  /* Message too long */

/* The following are not POSIX errors, but they can still happen. 
 * All of these are generated by the kernel and relate to message passing.
 */
#define ELOCKED      (_SIGN 101)  /* can't send message due to deadlock */
#define EBADCALL     (_SIGN 102)  /* illegal system call number */
#define EBADSRCDST   (_SIGN 103)  /* bad source or destination process */
#define ECALLDENIED  (_SIGN 104)  /* no permission for system call */
#define EDEADDST     (_SIGN 105)  /* send destination is not alive */
#define ENOTREADY    (_SIGN 106)  /* source or destination is not ready */
#define EBADREQUEST  (_SIGN 107)  /* destination cannot handle request */
#define EDONTREPLY   (_SIGN 201)  /* pseudo-code: don't send a reply */

#endif /* _ERRNO_H */
