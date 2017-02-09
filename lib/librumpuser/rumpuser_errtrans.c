/*	$NetBSD: rumpuser_errtrans.c,v 1.1 2013/04/30 12:39:20 pooka Exp $	*/

/*
 * pseudo-automatically generated.  PLEASE DO EDIT (e.g. in case there
 * are errnos which are defined to be the same value)
 *
 *   awk '/^#define/{printf "#ifdef %s\n\tcase %-15s: return %s;\n#endif\n", \
 *        $2, $2, $3}' < errno.h
 *
 */

#include <errno.h>

/*
 * Translate host errno to rump kernel errno
 */
int rumpuser__errtrans(int); /* a naughty decouple */
int
rumpuser__errtrans(int hosterr)
{

	/* just in case the vompiler is being silly */
	if (hosterr == 0)
		return 0;

	switch (hosterr) {
#ifdef EPERM
	case EPERM          : return 1;
#endif
#ifdef ENOENT
	case ENOENT         : return 2;
#endif
#ifdef ESRCH
	case ESRCH          : return 3;
#endif
#ifdef EINTR
	case EINTR          : return 4;
#endif
#ifdef EIO
	case EIO            : return 5;
#endif
#ifdef ENXIO
	case ENXIO          : return 6;
#endif
#ifdef E2BIG
	case E2BIG          : return 7;
#endif
#ifdef ENOEXEC
	case ENOEXEC        : return 8;
#endif
#ifdef EBADF
	case EBADF          : return 9;
#endif
#ifdef ECHILD
	case ECHILD         : return 10;
#endif
#ifdef EDEADLK
	case EDEADLK        : return 11;
#endif
#ifdef ENOMEM
	case ENOMEM         : return 12;
#endif
#ifdef EACCES
	case EACCES         : return 13;
#endif
#ifdef EFAULT
	case EFAULT         : return 14;
#endif
#ifdef ENOTBLK
	case ENOTBLK        : return 15;
#endif
#ifdef EBUSY
	case EBUSY          : return 16;
#endif
#ifdef EEXIST
	case EEXIST         : return 17;
#endif
#ifdef EXDEV
	case EXDEV          : return 18;
#endif
#ifdef ENODEV
	case ENODEV         : return 19;
#endif
#ifdef ENOTDIR
	case ENOTDIR        : return 20;
#endif
#ifdef EISDIR
	case EISDIR         : return 21;
#endif
#ifdef EINVAL
	case EINVAL         : return 22;
#endif
#ifdef ENFILE
	case ENFILE         : return 23;
#endif
#ifdef EMFILE
	case EMFILE         : return 24;
#endif
#ifdef ENOTTY
	case ENOTTY         : return 25;
#endif
#ifdef ETXTBSY
	case ETXTBSY        : return 26;
#endif
#ifdef EFBIG
	case EFBIG          : return 27;
#endif
#ifdef ENOSPC
	case ENOSPC         : return 28;
#endif
#ifdef ESPIPE
	case ESPIPE         : return 29;
#endif
#ifdef EROFS
	case EROFS          : return 30;
#endif
#ifdef EMLINK
	case EMLINK         : return 31;
#endif
#ifdef EPIPE
	case EPIPE          : return 32;
#endif
#ifdef EDOM
	case EDOM           : return 33;
#endif
#ifdef ERANGE
	case ERANGE         : return 34;
#endif
#ifdef EAGAIN
	case EAGAIN         : return 35;
#endif
#if defined(EWOULDBLOCK) && EWOULDBLOCK != EAGAIN
	case EWOULDBLOCK    : return 35;
#endif
#ifdef EINPROGRESS
	case EINPROGRESS    : return 36;
#endif
#ifdef EALREADY
	case EALREADY       : return 37;
#endif
#ifdef ENOTSOCK
	case ENOTSOCK       : return 38;
#endif
#ifdef EDESTADDRREQ
	case EDESTADDRREQ   : return 39;
#endif
#ifdef EMSGSIZE
	case EMSGSIZE       : return 40;
#endif
#ifdef EPROTOTYPE
	case EPROTOTYPE     : return 41;
#endif
#ifdef ENOPROTOOPT
	case ENOPROTOOPT    : return 42;
#endif
#ifdef EPROTONOSUPPORT
	case EPROTONOSUPPORT: return 43;
#endif
#ifdef ESOCKTNOSUPPORT
	case ESOCKTNOSUPPORT: return 44;
#endif
#ifdef EOPNOTSUPP
	case EOPNOTSUPP     : return 45;
#endif
#ifdef EPFNOSUPPORT
	case EPFNOSUPPORT   : return 46;
#endif
#ifdef EAFNOSUPPORT
	case EAFNOSUPPORT   : return 47;
#endif
#ifdef EADDRINUSE
	case EADDRINUSE     : return 48;
#endif
#ifdef EADDRNOTAVAIL
	case EADDRNOTAVAIL  : return 49;
#endif
#ifdef ENETDOWN
	case ENETDOWN       : return 50;
#endif
#ifdef ENETUNREACH
	case ENETUNREACH    : return 51;
#endif
#ifdef ENETRESET
	case ENETRESET      : return 52;
#endif
#ifdef ECONNABORTED
	case ECONNABORTED   : return 53;
#endif
#ifdef ECONNRESET
	case ECONNRESET     : return 54;
#endif
#ifdef ENOBUFS
	case ENOBUFS        : return 55;
#endif
#ifdef EISCONN
	case EISCONN        : return 56;
#endif
#ifdef ENOTCONN
	case ENOTCONN       : return 57;
#endif
#ifdef ESHUTDOWN
	case ESHUTDOWN      : return 58;
#endif
#ifdef ETOOMANYREFS
	case ETOOMANYREFS   : return 59;
#endif
#ifdef ETIMEDOUT
	case ETIMEDOUT      : return 60;
#endif
#ifdef ECONNREFUSED
	case ECONNREFUSED   : return 61;
#endif
#ifdef ELOOP
	case ELOOP          : return 62;
#endif
#ifdef ENAMETOOLONG
	case ENAMETOOLONG   : return 63;
#endif
#ifdef EHOSTDOWN
	case EHOSTDOWN      : return 64;
#endif
#ifdef EHOSTUNREACH
	case EHOSTUNREACH   : return 65;
#endif
#ifdef ENOTEMPTY
	case ENOTEMPTY      : return 66;
#endif
#ifdef EPROCLIM
	case EPROCLIM       : return 67;
#endif
#ifdef EUSERS
	case EUSERS         : return 68;
#endif
#ifdef EDQUOT
	case EDQUOT         : return 69;
#endif
#ifdef ESTALE
	case ESTALE         : return 70;
#endif
#ifdef EREMOTE
	case EREMOTE        : return 71;
#endif
#ifdef EBADRPC
	case EBADRPC        : return 72;
#endif
#ifdef ERPCMISMATCH
	case ERPCMISMATCH   : return 73;
#endif
#ifdef EPROGUNAVAIL
	case EPROGUNAVAIL   : return 74;
#endif
#ifdef EPROGMISMATCH
	case EPROGMISMATCH  : return 75;
#endif
#ifdef EPROCUNAVAIL
	case EPROCUNAVAIL   : return 76;
#endif
#ifdef ENOLCK
	case ENOLCK         : return 77;
#endif
#ifdef ENOSYS
	case ENOSYS         : return 78;
#endif
#ifdef EFTYPE
	case EFTYPE         : return 79;
#endif
#ifdef EAUTH
	case EAUTH          : return 80;
#endif
#ifdef ENEEDAUTH
	case ENEEDAUTH      : return 81;
#endif
#ifdef EIDRM
	case EIDRM          : return 82;
#endif
#ifdef ENOMSG
	case ENOMSG         : return 83;
#endif
#ifdef EOVERFLOW
	case EOVERFLOW      : return 84;
#endif
#ifdef EILSEQ
	case EILSEQ         : return 85;
#endif
#if defined(ENOTSUP) && (!defined(EOPNOTSUPP) || ENOTSUP != EOPNOTSUPP)
	case ENOTSUP        : return 86;
#endif
#ifdef ECANCELED
	case ECANCELED      : return 87;
#endif
#ifdef EBADMSG
	case EBADMSG        : return 88;
#endif
#ifdef ENODATA
	case ENODATA        : return 89;
#endif
#ifdef ENOSR
	case ENOSR          : return 90;
#endif
#ifdef ENOSTR
	case ENOSTR         : return 91;
#endif
#ifdef ETIME
	case ETIME          : return 92;
#endif
#ifdef ENOATTR
	case ENOATTR        : return 93;
#endif
#ifdef EMULTIHOP
	case EMULTIHOP      : return 94;
#endif
#ifdef ENOLINK
	case ENOLINK        : return 95;
#endif
#ifdef EPROTO
	case EPROTO         : return 96;
#endif

	default             : return 22; /* EINVAL */
	}
}
