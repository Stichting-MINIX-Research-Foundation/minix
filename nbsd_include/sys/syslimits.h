#ifndef _SYS_SYSLIMITS_H_
#define _SYS_SYSLIMITS_H_

#include <minix/limits.h>
#include <sys/featuretest.h>

/* Values actually implemented by MINIX (Tables 2-4, 2-5, 2-6, and 2-7). */
/* Some of these old names had better be defined when not POSIX. */
#define _NO_LIMIT          160	/* arbitrary number; limit not enforced */


#if defined(_POSIX_C_SOURCE) || defined(_XOPEN_SOURCE) || \
    defined(_NETBSD_SOURCE)

#if _EM_WSIZE > 2
#define ARG_MAX 	262144 /* # bytes of args + environ for exec() */
#else
#define ARG_MAX 	4096	/* args + environ on small machines */
#endif

#ifndef CHILD_MAX
#define	CHILD_MAX	_NO_LIMIT /* max simultaneous processes */
#endif

#define GID_MAX	      USHRT_MAX  /* max value for a gid_t */
#define LINK_MAX      SHRT_MAX	/* # links a file may have */
#define MAX_CANON          255	/* size of the canonical input queue */
#define MAX_INPUT          255	/* size of the type-ahead buffer */
#define NAME_MAX           255	/* system-wide max # chars in a file name */
#define NGROUPS_MAX          8	/* max. number of supplemental groups */
#define UID_MAX       USHRT_MAX  /* max value for a uid_t */
#ifndef OPEN_MAX
#define	OPEN_MAX __MINIX_OPEN_MAX /* max open files per process */
#endif
#define PATH_MAX __MINIX_PATH_MAX /* # chars in a path name */
#define PIPE_BUF          7168	/* # bytes in atomic write to a pipe */

#define	BC_BASE_MAX	      INT_MAX	/* max ibase/obase values in bc(1) */
#define	BC_DIM_MAX		65535	/* max array elements in bc(1) */
#define	BC_SCALE_MAX	      INT_MAX	/* max scale value in bc(1) */
#define	BC_STRING_MAX	      INT_MAX	/* max const string length in bc(1) */
#define	COLL_WEIGHTS_MAX	    2	/* max weights for order keyword */
#define	EXPR_NEST_MAX		   32	/* max expressions nested in expr(1) */
#define	LINE_MAX		 2048	/* max bytes in an input line */
#define	RE_DUP_MAX		  255	/* max RE's in interval notation */

/*
 * IEEE Std 1003.1c-95, adopted in X/Open CAE Specification Issue 5 Version 2
 */
#if (_POSIX_C_SOURCE - 0) >= 199506L || (_XOPEN_SOURCE - 0) >= 500 || \
    defined(_NETBSD_SOURCE)
#define	LOGIN_NAME_MAX		   17	/* max login name length incl. NUL */
#endif

/*
 * X/Open CAE Specification Issue 5 Version 2
 */
#if defined(_XOPEN_SOURCE) || defined(_NETBSD_SOURCE)
#define	IOV_MAX			 1024	/* max # of iovec's for readv(2) etc. */
#define	NZERO			   20	/* default "nice" */
#endif /* _XOPEN_SOURCE || _NETBSD_SOURCE */

#endif /* !_ANSI_SOURCE */

#ifdef __minix
#define STREAM_MAX 8 /* == _POSIX_STREAM_MAX */
#define TZNAME_MAX 6 /* == _POSIX_TZNAME_MAX */
#define TIME_MAX  LONG_MAX
#endif

#endif /* !_SYS_SYSLIMITS_H_ */

