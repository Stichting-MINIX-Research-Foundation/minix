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
 *	<define> EPERM		(_SIGN 1 )
 *
 * If the macro _SYSTEM is defined, then  _SIGN is set to "-", otherwise it is
 * set to "".  Thus when compiling the operating system, the  macro _SYSTEM
 * will be defined, setting EPERM to (- 1), whereas when when this
 * file is included in an ordinary user program, EPERM has the value ( 1).
 */

#ifndef _SYS_ERRNO_H_
#define _SYS_ERRNO_H_

/* Now define _SIGN as "" or "-" depending on _SYSTEM. */
#ifdef _SYSTEM
#   define _SIGN         -
#   define OK            0
#else
#   define _SIGN         
#endif



#define EPERM         (_SIGN  1 )  /* operation not permitted */
#define ENOENT        (_SIGN  2 )  /* no such file or directory */
#define ESRCH         (_SIGN  3 )  /* no such process */
#define EINTR         (_SIGN  4 )  /* interrupted function call */
#define EIO           (_SIGN  5 )  /* input/output error */
#define ENXIO         (_SIGN  6 )  /* no such device or address */
#define E2BIG         (_SIGN  7 )  /* arg list too long */
#define ENOEXEC       (_SIGN  8 )  /* exec format error */
#define EBADF         (_SIGN  9 )  /* bad file descriptor */
#define ECHILD        (_SIGN 10 )  /* no child process */
#define EAGAIN        (_SIGN 11 )  /* resource temporarily unavailable */
#define ENOMEM        (_SIGN 12 )  /* not enough space */
#define EACCES        (_SIGN 13 )  /* permission denied */
#define EFAULT        (_SIGN 14 )  /* bad address */
#define ENOTBLK       (_SIGN 15 )  /* Extension: not a block special file */
#define EBUSY         (_SIGN 16 )  /* resource busy */
#define EEXIST        (_SIGN 17 )  /* file exists */
#define EXDEV         (_SIGN 18 )  /* improper link */
#define ENODEV        (_SIGN 19 )  /* no such device */
#define ENOTDIR       (_SIGN 20 )  /* not a directory */
#define EISDIR        (_SIGN 21 )  /* is a directory */
#define EINVAL        (_SIGN 22 )  /* invalid argument */
#define ENFILE        (_SIGN 23 )  /* too many open files in system */
#define EMFILE        (_SIGN 24 )  /* too many open files */
#define ENOTTY        (_SIGN 25 )  /* inappropriate I/O control operation */
#define ETXTBSY       (_SIGN 26 )  /* no longer used */
#define EFBIG         (_SIGN 27 )  /* file too large */
#define ENOSPC        (_SIGN 28 )  /* no space left on device */
#define ESPIPE        (_SIGN 29 )  /* invalid seek */
#define EROFS         (_SIGN 30 )  /* read-only file system */
#define EMLINK        (_SIGN 31 )  /* too many links */
#define EPIPE         (_SIGN 32 )  /* broken pipe */
#define EDOM          (_SIGN 33 )  /* domain error    	(from ANSI C std ) */
#define ERANGE        (_SIGN 34 )  /* result too large	(from ANSI C std ) */
#define EDEADLK       (_SIGN 35 )  /* resource deadlock avoided */
#define ENAMETOOLONG  (_SIGN 36 )  /* file name too long */
#define ENOLCK        (_SIGN 37 )  /* no locks available */
#define ENOSYS        (_SIGN 38 )  /* function not implemented */
#define ENOTEMPTY     (_SIGN 39 )  /* directory not empty */
#define ELOOP         (_SIGN 40 )  /* too many levels of symlinks detected */
#define ERESTART      (_SIGN 41 )  /* service restarted */
#define EIDRM         (_SIGN 43 )  /* Identifier removed */
#define EILSEQ        (_SIGN 44 )  /* illegal byte sequence */
#define	ENOMSG	      (_SIGN 45 )  /* No message of desired type */
#define EOVERFLOW     (_SIGN 46 )  /* Value too large to be stored in data type */

/* The following errors relate to networking. */
#define EPACKSIZE     (_SIGN 50 )  /* invalid packet size for some protocol */
#define ENOBUFS       (_SIGN 51 )  /* not enough buffers left */
#define EBADIOCTL     (_SIGN 52 )  /* illegal ioctl for device */
#define EBADMODE      (_SIGN 53 )  /* badmode in ioctl */
#define EWOULDBLOCK   (_SIGN 54 )  /* call would block on nonblocking socket */
#define ENETUNREACH   (_SIGN 55 )  /* network unreachable */
#define EHOSTUNREACH  (_SIGN 56 )  /* host unreachable */
#define EISCONN	      (_SIGN 57 )  /* already connected */
#define EADDRINUSE    (_SIGN 58 )  /* address in use */
#define ECONNREFUSED  (_SIGN 59 )  /* connection refused */
#define ECONNRESET    (_SIGN 60 )  /* connection reset */
#define ETIMEDOUT     (_SIGN 61 )  /* connection timed out */
#define EURG	      (_SIGN 62 )  /* urgent data present */
#define ENOURG	      (_SIGN 63 )  /* no urgent data present */
#define ENOTCONN      (_SIGN 64 )  /* no connection (yet or anymore ) */
#define ESHUTDOWN     (_SIGN 65 )  /* a write call to a shutdown connection */
#define ENOCONN       (_SIGN 66 )  /* no such connection */
#define EAFNOSUPPORT  (_SIGN 67 )  /* address family not supported */
#define EPROTONOSUPPORT (_SIGN 68 ) /* protocol not supported by AF */
#define EPROTOTYPE    (_SIGN 69 )  /* Protocol wrong type for socket */
#define EINPROGRESS   (_SIGN 70 )  /* Operation now in progress */
#define EADDRNOTAVAIL (_SIGN 71 )  /* Can't assign requested address */
#define EALREADY      (_SIGN 72 )  /* Connection already in progress */
#define EMSGSIZE      (_SIGN 73 )  /* Message too long */
#define ENOTSOCK      (_SIGN 74 )  /* Socket operation on non-socket */
#define ENOPROTOOPT   (_SIGN 75 )  /* Protocol not available */
#define EOPNOTSUPP    (_SIGN 76 )  /* Operation not supported */
#define ENOTSUP       ( EOPNOTSUPP )  /* Not supported */
#define ENETDOWN      (_SIGN 77 )  /* network is down */
#define	EPFNOSUPPORT  (_SIGN 78 ) /* Protocol family not supported */
#define	EDESTADDRREQ  (_SIGN 79 )  /* Destination address required */
#define EHOSTDOWN     (_SIGN 80 )  /* Host is down */
#define ENETRESET     (_SIGN 81 )  /* Network dropped connection on reset */
#define	ESOCKTNOSUPPORT	(_SIGN 82 ) /* Socket type not supported */
#define	ECONNABORTED  (_SIGN 83 )  /* Software caused connection abort */
#define	ETOOMANYREFS   (_SIGN 84 ) /* Too many references: can't splice */

#define EGENERIC      (_SIGN 99 )  /* generic error */

/* The following are not POSIX errors, but they can still happen. 
 * All of these are generated by the kernel and relate to message passing.
 */
#define ELOCKED      (_SIGN 101 )  /* can't send message due to deadlock */
#define EBADCALL     (_SIGN 102 )  /* illegal system call number */
#define EBADSRCDST   (_SIGN 103 )  /* bad source or destination process */
#define ECALLDENIED  (_SIGN 104 )  /* no permission for system call */
#define EDEADSRCDST  (_SIGN 105 )  /* source or destination is not alive */
#define ENOTREADY    (_SIGN 106 )  /* source or destination is not ready */
#define EBADREQUEST  (_SIGN 107 )  /* destination cannot handle request */
#define ETRAPDENIED  (_SIGN 110 )  /* IPC trap not allowed */

/* The following errors are NetBSD errors. */
#define	EFTYPE	     (_SIGN 150 )  /* Inappropriate file type or format */
#define	EAUTH	     (_SIGN 151 )  /* Authentication error */
#define	ENEEDAUTH    (_SIGN 152 )  /* Need authenticator */
/* Realtime option errors */
#define ECANCELED    (_SIGN 153 )  /* Operation canceled */

/* Network File System */
#define	ESTALE	     (_SIGN 160 )  /* Stale NFS file handle */
#define	EREMOTE	     (_SIGN 161 )  /* Too many levels of remote in path */
#define	EBADRPC	     (_SIGN 162 )  /* RPC struct is bad */
#define	ERPCMISMATCH (_SIGN 163 )  /* RPC version wrong */
#define	EPROGUNAVAIL (_SIGN 164 )  /* RPC prog. not avail */
#define	EPROGMISMATCH (_SIGN 165 ) /* Program version wrong */
#define	EPROCUNAVAIL (_SIGN 166 )  /* Bad procedure for program */

/* Realtime, XSI STREAMS option errors */
#define EBADMSG	     (_SIGN 170 )  /* Bad or Corrupt message */

/* quotas & mush */
#define	EPROCLIM     (_SIGN 175 )  /* Too many processes */
#define	EUSERS	     (_SIGN 176 )  /* Too many users */
#define	EDQUOT	     (_SIGN 177 )  /* Disc quota exceeded */

/* Realtime, XSI STREAMS option errors */
#define	EMULTIHOP    (_SIGN 180 )  /* Multihop attempted */ 
#define	ENOLINK	     (_SIGN 181 )  /* Link has been severed */
#define	EPROTO	     (_SIGN 182 )  /* Protocol error */

/* File system extended attribute errors */
#define	ENOATTR	     (_SIGN 185 )  /* Attribute not found */

/* XSI STREAMS option errors  */
#define ENODATA	     (_SIGN 190 )  /* No message available */
#define ENOSR	     (_SIGN 191 )  /* No STREAM resources */
#define ENOSTR	     (_SIGN 192 )  /* Not a STREAM */
#define ETIME	     (_SIGN 193 )  /* STREAM ioctl timeout */

#define EDONTREPLY   (_SIGN 201 )  /* pseudo-code: don't send a reply */

/* The following are non-POSIX server responses */
#define EBADEPT      (_SIGN 301 )  /* specified endpoint is bad */
#define EDEADEPT     (_SIGN 302 )  /* specified endpoint is not alive */
#define EBADCPU	     (_SIGN 303 )  /* requested CPU does not work */

#endif /* !_SYS_ERRNO_H_ */
