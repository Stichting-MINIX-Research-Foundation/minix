/*	$NetBSD: compat_defs.h,v 1.103 2015/09/21 21:50:16 pooka Exp $	*/

#ifndef	__NETBSD_COMPAT_DEFS_H__
#define	__NETBSD_COMPAT_DEFS_H__

/*
 * On NetBSD, ensure that _NETBSD_SOURCE does not get defined, so that
 * accidental attempts to use NetBSD-specific features instead of more
 * portable features is likely to be noticed when the tools are built
 * on NetBSD.  Define enough other feature test macros to expose the
 * features we need.
 */
#ifdef __NetBSD__
#define	_ISOC99_SOURCE
#define _POSIX_SOURCE	1
#define _POSIX_C_SOURCE	200112L
#define _XOPEN_SOURCE 600
#endif /* __NetBSD__ */

/*
 * Linux: <features.h> turns on _POSIX_SOURCE by default, even though the
 * program (not the OS) should do that.  Preload <features.h> and
 * then override some of the feature test macros.
 */

#if defined(__linux__) && HAVE_FEATURES_H
#include <features.h>
#undef _POSIX_SOURCE
#undef _POSIX_C_SOURCE
#define __USE_ISOC99 1
#endif	/* __linux__ && HAVE_FEATURES_H */

/* System headers needed for (re)definitions below. */

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/param.h>
/* time.h needs to be pulled in first at least on netbsd w/o _NETBSD_SOURCE */
#include <sys/time.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <paths.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if HAVE_SYS_CDEFS_H
#include <sys/cdefs.h>
#endif
#if HAVE_SYS_SYSLIMITS_H
#include <sys/syslimits.h>
#endif
#if HAVE_SYS_SYSMACROS_H
/* major(), minor() on SVR4 */
#include <sys/sysmacros.h>
#endif
#if HAVE_INTTYPES_H
#include <inttypes.h>
#endif
#if HAVE_STDDEF_H
#include <stddef.h>
#endif

#if HAVE_RPC_TYPES_H
#include <rpc/types.h>
#endif

#ifdef _NETBSD_SOURCE
#error _NETBSD_SOURCE is *not* to be defined.
#endif

/* Need this since we can't depend on NetBSD's version to be around */
#ifdef __UNCONST
#undef __UNCONST
#endif
#define __UNCONST(a)   ((void *)(unsigned long)(const void *)(a))
#ifdef __UNVOLATILE
#undef __UNVOLATILE
#endif
#define __UNVOLATILE(a)        ((void *)(unsigned long)(volatile void *)(a))


#undef __predict_false
#define __predict_false(x) (x)
#undef __predict_true
#define __predict_true(x) (x)

/* We don't include <pwd.h> here, so that "compat_pwd.h" works. */
struct passwd;

/* We don't include <grp.h> either */
struct group;

/* Assume an ANSI compiler for the host. */

#undef __P
#define __P(x) x

#ifndef __BEGIN_DECLS
#define __BEGIN_DECLS
#endif
#ifndef __END_DECLS
#define __END_DECLS
#endif

/* Some things in NetBSD <sys/cdefs.h>. */

#ifndef __CONCAT
#define	__CONCAT(x,y)	x ## y
#endif
#if !defined(__attribute__) && !defined(__GNUC__)
#define __attribute__(x)
#endif
#if !defined(__packed)
#if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 7)
#define __packed	__attribute__((__packed__))
#elif defined(__SUNPRO_C) && (__SUNPRO_C >= 0x590)
#define __packed	__attribute__((__packed__))
#else
#define	__packed	error: no __packed for this compiler
#endif
#endif /* !__packed */
#ifndef __RENAME
#define __RENAME(x)
#endif
#undef __aconst
#define __aconst
#undef __dead
#define __dead
#undef __printflike
#define __printflike(x,y)
#undef __format_arg
#define __format_arg(x)
#undef __restrict
#define __restrict
#undef __unused
#define __unused
#undef __arraycount
#define	__arraycount(__x)	(sizeof(__x) / sizeof(__x[0]))
#undef __USE
#define __USE(a) ((void)(a))
#undef __type_min_s
#define __type_min_s(t) ((t)((1ULL << (sizeof(t) * NBBY - 1))))
#undef __type_max_s
#define __type_max_s(t) ((t)~((1ULL << (sizeof(t) * NBBY - 1))))
#undef __type_min_u
#define __type_min_u(t) ((t)0ULL)
#undef __type_max_u
#define __type_max_u(t) ((t)~0ULL)
#undef __type_is_signed
#define __type_is_signed(t) (/*LINTED*/__type_min_s(t) + (t)1 < (t)1)
#undef __type_min
#define __type_min(t) (__type_is_signed(t) ? __type_min_s(t) : __type_min_u(t))
#undef __type_max
#define __type_max(t) (__type_is_signed(t) ? __type_max_s(t) : __type_max_u(t))

/* Dirent support. */

#if HAVE_DIRENT_H
# if defined(__linux__) && defined(__USE_BSD)
#  undef __USE_BSD
#  include <dirent.h>
#  define __USE_BSD 1
#  undef d_fileno
# else
#  include <dirent.h>
#  if defined(__DARWIN_UNIX03)
#   undef d_fileno
#  endif
# endif
# define NAMLEN(dirent) (strlen((dirent)->d_name))
#else
# define dirent direct
# define NAMLEN(dirent) ((dirent)->d_namlen)
# if HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif
# if HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif
# if HAVE_NDIR_H
#  include <ndir.h>
# endif
#endif

/* Type substitutes. */

#if !HAVE_ID_T
typedef unsigned int id_t;
#endif

#if !HAVE_SOCKLEN_T
/*
 * This is defined as int for compatibility with legacy systems (and not
 * unsigned int), since universally it was int in most systems that did not
 * define it.
 */
typedef int socklen_t;
#endif

#if !HAVE_U_LONG
typedef unsigned long u_long;
#endif

#if !HAVE_U_CHAR
typedef unsigned char u_char;
#endif

#if !HAVE_U_INT
typedef unsigned int u_int;
#endif

#if !HAVE_U_SHORT
typedef unsigned short u_short;
#endif

/* Prototypes for replacement functions. */

#if !HAVE_ATOLL
long long int atoll(const char *);
#endif

#if !HAVE_ASPRINTF
int asprintf(char **, const char *, ...);
#endif

#if !HAVE_ASNPRINTF
int asnprintf(char **, size_t, const char *, ...);
#endif

#if !HAVE_BASENAME
char *basename(char *);
#endif

#if !HAVE_DECL_OPTIND
int getopt(int, char *const *, const char *);
extern char *optarg;
extern int optind, opterr, optopt;
#endif

#if !HAVE_DIRNAME
char *dirname(char *);
#endif

#if !HAVE_DIRFD
#if HAVE_DIR_DD_FD
#define dirfd(dirp) ((dirp)->dd_fd)
#elif HAVE_DIR___DD_FD
#define dirfd(dirp) ((dirp)->__dd_fd)
#else
/*XXX: Very hacky but no other way to bring this into scope w/o defining
  _NETBSD_SOURCE which we're avoiding. */
#if defined(__NetBSD__) || defined(__minix)
struct _dirdesc {
        int     dd_fd;          /* file descriptor associated with directory */
	long    dd_loc;         /* offset in current buffer */
	long    dd_size;        /* amount of data returned by getdents */
	char    *dd_buf;        /* data buffer */
	int     dd_len;         /* size of data buffer */
	off_t   dd_seek;        /* magic cookie returned by getdents */
	long    dd_rewind;      /* magic cookie for rewinding */
	int     dd_flags;       /* flags for readdir */
	void    *dd_lock;       /* lock for concurrent access */
};
#define dirfd(dirp)     (((struct _dirdesc *)dirp)->dd_fd)
#else
#error cannot figure out how to turn a DIR * into a fd
#endif
#endif
#endif

#if !HAVE_ERR_H
void err(int, const char *, ...);
void errx(int, const char *, ...);
void warn(const char *, ...);
void warnx(const char *, ...);
void vwarnx(const char *, va_list);
#endif
#if !HAVE_DECL_WARNC
void warnc(int, const char *, ...);
#endif
#if !HAVE_DECL_VWARNC
void vwarnc(int, const char *, va_list);
#endif
#if !HAVE_DECL_ERRC
void errc(int, int, const char *, ...);
#endif
#if !HAVE_DECL_VERRC
void verrc(int, int, const char *, va_list);
#endif

#if !HAVE_ESETFUNC
void (*esetfunc(void (*)(int, const char *, ...)))(int, const char *, ...);
size_t estrlcpy(char *, const char *, size_t);
size_t estrlcat(char *, const char *, size_t);
char *estrdup(const char *);
char *estrndup(const char *, size_t);
void *ecalloc(size_t, size_t);
void *emalloc(size_t);
void *erealloc(void *, size_t);
FILE *efopen(const char *, const char *);
int easprintf(char **, const char *, ...);
int evasprintf(char **, const char *, va_list);
#endif

#if !HAVE_FGETLN || defined(__NetBSD__) || defined(__minix)
char *fgetln(FILE *, size_t *);
#endif
#if !HAVE_DPRINTF
int dprintf(int, const char *, ...);
#endif

#if !HAVE_FLOCK
# define LOCK_SH		0x01
# define LOCK_EX		0x02
# define LOCK_NB		0x04
# define LOCK_UN		0x08
int flock(int, int);
#endif

#if !HAVE_FPARSELN || BROKEN_FPARSELN || defined(__NetBSD__) || defined(__minix)
# define FPARSELN_UNESCESC	0x01
# define FPARSELN_UNESCCONT	0x02
# define FPARSELN_UNESCCOMM	0x04
# define FPARSELN_UNESCREST	0x08
# define FPARSELN_UNESCALL	0x0f
char *fparseln(FILE *, size_t *, size_t *, const char [3], int);
#endif

#if !HAVE_GETLINE
ssize_t getdelim(char **, size_t *, int, FILE *);
ssize_t getline(char **, size_t *, FILE *);
#endif

#if !HAVE_ISSETUGID
int issetugid(void);
#endif

#if !HAVE_ISBLANK && !defined(isblank)
#define isblank(x) ((x) == ' ' || (x) == '\t')
#endif

#define __nbcompat_bswap16(x)	((((x) << 8) & 0xff00) | (((x) >> 8) & 0x00ff))

#define __nbcompat_bswap32(x)	((((x) << 24) & 0xff000000) | \
				 (((x) <<  8) & 0x00ff0000) | \
				 (((x) >>  8) & 0x0000ff00) | \
				 (((x) >> 24) & 0x000000ff))

#define __nbcompat_bswap64(x)	(((u_int64_t)bswap32((x)) << 32) | \
				 ((u_int64_t)bswap32((x) >> 32)))

#if ! HAVE_DECL_BSWAP16
#ifdef bswap16
#undef bswap16
#endif
#define bswap16(x)	__nbcompat_bswap16(x)
#endif
#if ! HAVE_DECL_BSWAP32
#ifdef bswap32
#undef bswap32
#endif
#define bswap32(x)	__nbcompat_bswap32(x)
#endif
#if ! HAVE_DECL_BSWAP64
#ifdef bswap64
#undef bswap64
#endif
#define bswap64(x)	__nbcompat_bswap64(x)
#endif

#if !HAVE_MKSTEMP
int mkstemp(char *);
#endif

#if !HAVE_MKDTEMP
char *mkdtemp(char *);
#endif

#if !HAVE_MKSTEMP || !HAVE_MKDTEMP
/* This is a prototype for the internal function defined in
 * src/lib/lib/stdio/gettemp.c */
int __nbcompat_gettemp(char *, int *, int);
#endif

#if !HAVE_PREAD
ssize_t pread(int, void *, size_t, off_t);
#endif

#if !HAVE_HEAPSORT
int heapsort (void *, size_t, size_t, int (*)(const void *, const void *));
#endif
/* Make them use our version */
#  define heapsort __nbcompat_heapsort

char	       *flags_to_string(unsigned long, const char *);
int		string_to_flags(char **, unsigned long *, unsigned long *);

/*
 * HAVE_X_FROM_Y and HAVE_PWCACHE_FOODB go together, because we cannot
 * supply an implementation of one without the others -- some parts are
 * libc internal and this varies from system to system.
 *
 * XXX this is dubious anyway: we assume (see HAVE_DECLs below) that if the
 * XXX host system has all of these functions, all of their interfaces
 * XXX and interactions are exactly the same as in our libc/libutil -- ugh.
 */
#if !HAVE_USER_FROM_UID || !HAVE_UID_FROM_USER || !HAVE_GROUP_FROM_GID || \
    !HAVE_GID_FROM_GROUP || !HAVE_PWCACHE_USERDB || !HAVE_PWCACHE_GROUDB
/* Make them use our version */
#  define user_from_uid __nbcompat_user_from_uid
#  define uid_from_user __nbcompat_uid_from_user
#  define pwcache_userdb __nbcompat_pwcache_userdb
#  define group_from_gid __nbcompat_group_from_gid
#  define gid_from_group __nbcompat_gid_from_group
#  define pwcache_groupdb __nbcompat_pwcache_groupdb
#endif

#if !HAVE_DECL_UID_FROM_USER
int uid_from_user(const char *, uid_t *);
#endif

#if !HAVE_DECL_USER_FROM_UID
const char *user_from_uid(uid_t, int);
#endif

#if !HAVE_DECL_PWCACHE_USERDB
int pwcache_userdb(int (*)(int), void (*)(void),
                struct passwd * (*)(const char *), struct passwd * (*)(uid_t));
#endif

#if !HAVE_DECL_GID_FROM_GROUP
int gid_from_group(const char *, gid_t *);
#endif

#if !HAVE_DECL_GROUP_FROM_GID
const char *group_from_gid(gid_t, int);
#endif

#if !HAVE_DECL_PWCACHE_GROUPDB
int pwcache_groupdb(int (*)(int), void (*)(void),
    struct group * (*)(const char *), struct group * (*)(gid_t));
#endif

#if !HAVE_DECL_STRNDUP
char		*strndup(const char *, size_t);
#endif
#if !HAVE_DECL_STRNLEN
size_t		strnlen(const char *, size_t);
#endif
#if !HAVE_DECL_LCHFLAGS
int		lchflags(const char *, unsigned long);
#endif
#if !HAVE_DECL_LCHMOD
int		lchmod(const char *, mode_t);
#endif
#if !HAVE_DECL_LCHOWN
int		lchown(const char *, uid_t, gid_t);
#endif

#if !HAVE_PWRITE
ssize_t pwrite(int, const void *, size_t, off_t);
#endif

#if !HAVE_RAISE_DEFAULT_SIGNAL
int raise_default_signal(int);
#endif

#if !HAVE_REALLOCARR
int reallocarr(void *, size_t, size_t);
#endif

#if !HAVE_SETENV
int setenv(const char *, const char *, int);
#endif

#if !HAVE_DECL_SETGROUPENT
int setgroupent(int);
#endif

#if !HAVE_DECL_SETPASSENT
int setpassent(int);
#endif

#if !HAVE_SETPROGNAME || defined(__NetBSD__) || defined(__minix)
const char *getprogname(void);
void setprogname(const char *);
#endif

#if !HAVE_SNPRINTB_M
int snprintb(char *, size_t, const char *, uint64_t);
int snprintb_m(char *, size_t, const char *, uint64_t, size_t);
#endif

#if !HAVE_SNPRINTF
int snprintf(char *, size_t, const char *, ...);
#endif

#if !HAVE_STRLCAT
size_t strlcat(char *, const char *, size_t);
#endif

#if !HAVE_STRLCPY
size_t strlcpy(char *, const char *, size_t);
#endif

#if !HAVE_STRMODE
void strmode(mode_t, char *);
#endif

#if !HAVE_STRNDUP
char *strndup(const char *, size_t);
#endif

#if !HAVE_STRSEP || defined(__NetBSD__) || defined(__minix)
char *strsep(char **, const char *);
#endif

#if !HAVE_DECL_STRSUFTOLL
long long strsuftoll(const char *, const char *, long long, long long);
long long strsuftollx(const char *, const char *,
			long long, long long, char *, size_t);
#endif

#if !HAVE_STRTOLL
long long strtoll(const char *, char **, int);
#endif

#if !HAVE_STRTOI
intmax_t strtoi(const char * __restrict, char ** __restrict, int,
    intmax_t, intmax_t, int *);
#endif

#if !HAVE_STRTOU
uintmax_t strtou(const char * __restrict, char ** __restrict, int,
    uintmax_t, uintmax_t, int *);
#endif

#if !HAVE_USER_FROM_UID
const char *user_from_uid(uid_t, int);
#endif

#if !HAVE_GROUP_FROM_GID
const char *group_from_gid(gid_t, int);
#endif

#if !HAVE_VASPRINTF
int vasprintf(char **, const char *, va_list);
#endif

#if !HAVE_VASNPRINTF
int vasnprintf(char **, size_t, const char *, va_list);
#endif

#if !HAVE_VSNPRINTF
int vsnprintf(char *, size_t, const char *, va_list);
#endif

/*
 * getmode() and setmode() are always defined, as these function names
 * exist but with very different meanings on other OS's.  The compat
 * versions here simply accept an octal mode number; the "u+x,g-w" type
 * of syntax is not accepted.
 */

#define getmode __nbcompat_getmode
#define setmode __nbcompat_setmode

mode_t getmode(const void *, mode_t);
void *setmode(const char *);

/* Eliminate assertions embedded in binaries. */

#undef _DIAGASSERT
#define _DIAGASSERT(x)

/* Various sources use this */
#undef	__RCSID
#define	__RCSID(x) struct XXXNETBSD_RCSID
#undef	__SCCSID
#define	__SCCSID(x)
#undef	__COPYRIGHT
#define	__COPYRIGHT(x) struct XXXNETBSD_COPYRIGHT
#undef	__KERNEL_RCSID
#define	__KERNEL_RCSID(x,y)

/* Heimdal expects this one. */

#undef RCSID
#define RCSID(x)

/* Some definitions not available on all systems. */

#ifndef __inline
#define __inline inline
#endif

/* <errno.h> */

#ifndef EFTYPE
#define EFTYPE EIO
#endif

/* <fcntl.h> */

#ifndef O_EXLOCK
#define O_EXLOCK 0
#endif
#ifndef O_SHLOCK
#define O_SHLOCK 0
#endif
#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif

/* <inttypes.h> */

#if UCHAR_MAX == 0xffU			/* char is an 8-bit type */
#ifndef PRId8
#define PRId8 "hhd"
#endif
#ifndef PRIi8
#define PRIi8 "hhi"
#endif
#ifndef PRIo8
#define PRIo8 "hho"
#endif
#ifndef PRIu8
#define PRIu8 "hhu"
#endif
#ifndef PRIx8
#define PRIx8 "hhx"
#endif
#ifndef PRIX8
#define PRIX8 "hhX"
#endif
#ifndef SCNd8
#define SCNd8 "hhd"
#endif
#ifndef SCNi8
#define SCNi8 "hhi"
#endif
#ifndef SCNo8
#define SCNo8 "hho"
#endif
#ifndef SCNu8
#define SCNu8 "hhu"
#endif
#ifndef SCNx8
#define SCNx8 "hhx"
#endif
#ifndef SCNX8
#define SCNX8 "hhX"
#endif
#endif					/* char is an 8-bit type */
#if ! (defined(PRId8) && defined(PRIi8) && defined(PRIo8) && \
	defined(PRIu8) && defined(PRIx8) && defined(PRIX8))
#error "Don't know how to define PRI[diouxX]8"
#endif
#if ! (defined(SCNd8) && defined(SCNi8) && defined(SCNo8) && \
	defined(SCNu8) && defined(SCNx8) && defined(SCNX8))
#error "Don't know how to define SCN[diouxX]8"
#endif

#if USHRT_MAX == 0xffffU		/* short is a 16-bit type */
#ifndef PRId16
#define PRId16 "hd"
#endif
#ifndef PRIi16
#define PRIi16 "hi"
#endif
#ifndef PRIo16
#define PRIo16 "ho"
#endif
#ifndef PRIu16
#define PRIu16 "hu"
#endif
#ifndef PRIx16
#define PRIx16 "hx"
#endif
#ifndef PRIX16
#define PRIX16 "hX"
#endif
#ifndef SCNd16
#define SCNd16 "hd"
#endif
#ifndef SCNi16
#define SCNi16 "hi"
#endif
#ifndef SCNo16
#define SCNo16 "ho"
#endif
#ifndef SCNu16
#define SCNu16 "hu"
#endif
#ifndef SCNx16
#define SCNx16 "hx"
#endif
#ifndef SCNX16
#define SCNX16 "hX"
#endif
#endif					/* short is a 16-bit type */
#if ! (defined(PRId16) && defined(PRIi16) && defined(PRIo16) && \
	defined(PRIu16) && defined(PRIx16) && defined(PRIX16))
#error "Don't know how to define PRI[diouxX]16"
#endif
#if ! (defined(SCNd16) && defined(SCNi16) && defined(SCNo16) && \
	defined(SCNu16) && defined(SCNx16) && defined(SCNX16))
#error "Don't know how to define SCN[diouxX]16"
#endif

#if UINT_MAX == 0xffffffffU		/* int is a 32-bit type */
#ifndef PRId32
#define PRId32 "d"
#endif
#ifndef PRIi32
#define PRIi32 "i"
#endif
#ifndef PRIo32
#define PRIo32 "o"
#endif
#ifndef PRIu32
#define PRIu32 "u"
#endif
#ifndef PRIx32
#define PRIx32 "x"
#endif
#ifndef PRIX32
#define PRIX32 "X"
#endif
#ifndef SCNd32
#define SCNd32 "d"
#endif
#ifndef SCNi32
#define SCNi32 "i"
#endif
#ifndef SCNo32
#define SCNo32 "o"
#endif
#ifndef SCNu32
#define SCNu32 "u"
#endif
#ifndef SCNx32
#define SCNx32 "x"
#endif
#ifndef SCNX32
#define SCNX32 "X"
#endif
#endif					/* int is a 32-bit type */
#if ULONG_MAX == 0xffffffffU		/* long is a 32-bit type */
#ifndef PRId32
#define PRId32 "ld"
#endif
#ifndef PRIi32
#define PRIi32 "li"
#endif
#ifndef PRIo32
#define PRIo32 "lo"
#endif
#ifndef PRIu32
#define PRIu32 "lu"
#endif
#ifndef PRIx32
#define PRIx32 "lx"
#endif
#ifndef PRIX32
#define PRIX32 "lX"
#endif
#ifndef SCNd32
#define SCNd32 "ld"
#endif
#ifndef SCNi32
#define SCNi32 "li"
#endif
#ifndef SCNo32
#define SCNo32 "lo"
#endif
#ifndef SCNu32
#define SCNu32 "lu"
#endif
#ifndef SCNx32
#define SCNx32 "lx"
#endif
#ifndef SCNX32
#define SCNX32 "lX"
#endif
#endif					/* long is a 32-bit type */
#if ! (defined(PRId32) && defined(PRIi32) && defined(PRIo32) && \
	defined(PRIu32) && defined(PRIx32) && defined(PRIX32))
#error "Don't know how to define PRI[diouxX]32"
#endif
#if ! (defined(SCNd32) && defined(SCNi32) && defined(SCNo32) && \
	defined(SCNu32) && defined(SCNx32) && defined(SCNX32))
#error "Don't know how to define SCN[diouxX]32"
#endif

#if ULONG_MAX == 0xffffffffffffffffU	/* long is a 64-bit type */
#ifndef PRId64
#define PRId64 "ld"
#endif
#ifndef PRIi64
#define PRIi64 "li"
#endif
#ifndef PRIo64
#define PRIo64 "lo"
#endif
#ifndef PRIu64
#define PRIu64 "lu"
#endif
#ifndef PRIx64
#define PRIx64 "lx"
#endif
#ifndef PRIX64
#define PRIX64 "lX"
#endif
#ifndef SCNd64
#define SCNd64 "ld"
#endif
#ifndef SCNi64
#define SCNi64 "li"
#endif
#ifndef SCNo64
#define SCNo64 "lo"
#endif
#ifndef SCNu64
#define SCNu64 "lu"
#endif
#ifndef SCNx64
#define SCNx64 "lx"
#endif
#ifndef SCNX64
#define SCNX64 "lX"
#endif
#endif					/* long is a 64-bit type */
#if ULLONG_MAX == 0xffffffffffffffffU	/* long long is a 64-bit type */
#ifndef PRId64
#define PRId64 "lld"
#endif
#ifndef PRIi64
#define PRIi64 "lli"
#endif
#ifndef PRIo64
#define PRIo64 "llo"
#endif
#ifndef PRIu64
#define PRIu64 "llu"
#endif
#ifndef PRIx64
#define PRIx64 "llx"
#endif
#ifndef PRIX64
#define PRIX64 "llX"
#endif
#ifndef SCNd64
#define SCNd64 "lld"
#endif
#ifndef SCNi64
#define SCNi64 "lli"
#endif
#ifndef SCNo64
#define SCNo64 "llo"
#endif
#ifndef SCNu64
#define SCNu64 "llu"
#endif
#ifndef SCNx64
#define SCNx64 "llx"
#endif
#ifndef SCNX64
#define SCNX64 "llX"
#endif
#endif					/* long long is a 64-bit type */
#if ! (defined(PRId64) && defined(PRIi64) && defined(PRIo64) && \
	defined(PRIu64) && defined(PRIx64) && defined(PRIX64))
#error "Don't know how to define PRI[diouxX]64"
#endif
#if ! (defined(SCNd64) && defined(SCNi64) && defined(SCNo64) && \
	defined(SCNu64) && defined(SCNx64) && defined(SCNX64))
#error "Don't know how to define SCN[diouxX]64"
#endif

/* <limits.h> */

#ifndef UID_MAX
#define UID_MAX 32767
#endif
#ifndef GID_MAX
#define GID_MAX UID_MAX
#endif

#ifndef UQUAD_MAX
#define UQUAD_MAX ((u_quad_t)-1)
#endif
#ifndef QUAD_MAX
#define QUAD_MAX ((quad_t)(UQUAD_MAX >> 1))
#endif
#ifndef QUAD_MIN
#define QUAD_MIN ((quad_t)(~QUAD_MAX))
#endif
#ifndef ULLONG_MAX
#define ULLONG_MAX ((unsigned long long)-1)
#endif
#ifndef LLONG_MAX
#define LLONG_MAX ((long long)(ULLONG_MAX >> 1))
#endif
#ifndef LLONG_MIN
#define LLONG_MIN ((long long)(~LLONG_MAX))
#endif

#ifndef MAXPATHLEN
#define MAXPATHLEN	4096
#endif
#ifndef PATH_MAX
#define PATH_MAX	MAXPATHLEN
#endif

/* <paths.h> */

/* The host's _PATH_BSHELL might be broken, so override it. */
#undef _PATH_BSHELL
#define _PATH_BSHELL PATH_BSHELL
#ifndef _PATH_DEFPATH
#define _PATH_DEFPATH "/usr/bin:/bin:/usr/local/bin"
#endif
#ifndef _PATH_DEV
#define _PATH_DEV "/dev/"
#endif
#ifndef _PATH_DEVNULL
#define _PATH_DEVNULL _PATH_DEV "null"
#endif
#ifndef _PATH_TMP
#define _PATH_TMP "/tmp/"
#endif
#ifndef _PATH_DEFTAPE
#define _PATH_DEFTAPE "/dev/nrst0"
#endif
#ifndef _PATH_VI
#define _PATH_VI "/usr/bin/vi"
#endif

/* <stdint.h> */

#if !defined(SIZE_MAX) && defined(SIZE_T_MAX)
#define SIZE_MAX SIZE_T_MAX
#endif

#ifndef UINT8_MAX
#define UINT8_MAX 0xffU
#endif

#ifndef UINT16_MAX
#define UINT16_MAX 0xffffU
#endif

#ifndef UINT32_MAX
#define UINT32_MAX 0xffffffffU
#endif

/* <stdlib.h> */

#ifndef __GNUC__
# if HAVE_ALLOCA_H
#  include <alloca.h>
# else
#  ifndef alloca /* predefined by HP cc +Olibcalls */
char *alloca ();
#  endif
# endif
#endif

/* avoid prototype conflicts with host */
#define cgetcap __nbcompat_cgetcap
#define cgetclose __nbcompat_cgetclose
#define cgetent __nbcompat_cgetent
#define cgetfirst __nbcompat_cgetfirst
#define cgetmatch __nbcompat_cgetmatch
#define cgetnext __nbcompat_cgetnext
#define cgetnum __nbcompat_cgetnum
#define cgetset __nbcompat_cgetset
#define cgetstr __nbcompat_cgetstr
#define cgetustr __nbcompat_cgetustr

char	*cgetcap(char *, const char *, int);
int	 cgetclose(void);
int	 cgetent(char **, const char * const *, const char *);
int	 cgetfirst(char **, const char * const *);
int	 cgetmatch(const char *, const char *);
int	 cgetnext(char **, const char * const *);
int	 cgetnum(char *, const char *, long *);
int	 cgetset(const char *);
int	 cgetstr(char *, const char *, char **);
int	 cgetustr(char *, const char *, char **);

/* <sys/endian.h> */

#if WORDS_BIGENDIAN
#if !HAVE_DECL_HTOBE16
#define htobe16(x)	(x)
#endif
#if !HAVE_DECL_HTOBE32
#define htobe32(x)	(x)
#endif
#if !HAVE_DECL_HTOBE64
#define htobe64(x)	(x)
#endif
#if !HAVE_DECL_HTOLE16
#define htole16(x)	bswap16((u_int16_t)(x))
#endif
#if !HAVE_DECL_HTOLE32
#define htole32(x)	bswap32((u_int32_t)(x))
#endif
#if !HAVE_DECL_HTOLE64
#define htole64(x)	bswap64((u_int64_t)(x))
#endif
#else
#if !HAVE_DECL_HTOBE16
#define htobe16(x)	bswap16((u_int16_t)(x))
#endif
#if !HAVE_DECL_HTOBE32
#define htobe32(x)	bswap32((u_int32_t)(x))
#endif
#if !HAVE_DECL_HTOBE64
#define htobe64(x)	bswap64((u_int64_t)(x))
#endif
#if !HAVE_DECL_HTOLE16
#define htole16(x)	(x)
#endif
#if !HAVE_DECL_HTOLE32
#define htole32(x)	(x)
#endif
#if !HAVE_DECL_HTOLE64
#define htole64(x)	(x)
#endif
#endif
#if !HAVE_DECL_BE16TOH
#define be16toh(x)	htobe16(x)
#endif
#if !HAVE_DECL_BE32TOH
#define be32toh(x)	htobe32(x)
#endif
#if !HAVE_DECL_BE64TOH
#define be64toh(x)	htobe64(x)
#endif
#if !HAVE_DECL_LE16TOH
#define le16toh(x)	htole16(x)
#endif
#if !HAVE_DECL_LE32TOH
#define le32toh(x)	htole32(x)
#endif
#if !HAVE_DECL_LE64TOH
#define le64toh(x)	htole64(x)
#endif

#define __GEN_ENDIAN_ENC(bits, endian) \
static void \
endian ## bits ## enc(void *dst, uint ## bits ## _t u) \
{ \
	u = hto ## endian ## bits (u); \
	memcpy(dst, &u, sizeof(u)); \
}
#if !HAVE_DECL_BE16ENC
__GEN_ENDIAN_ENC(16, be)
#endif
#if !HAVE_DECL_BE32ENC
__GEN_ENDIAN_ENC(32, be)
#endif
#if !HAVE_DECL_BE64ENC
__GEN_ENDIAN_ENC(64, be)
#endif
#if !HAVE_DECL_LE16ENC
__GEN_ENDIAN_ENC(16, le)
#endif
#if !HAVE_DECL_LE32ENC
__GEN_ENDIAN_ENC(32, le)
#endif
#if !HAVE_DECL_LE64ENC
__GEN_ENDIAN_ENC(64, le)
#endif
#undef __GEN_ENDIAN_ENC

#define __GEN_ENDIAN_DEC(bits, endian) \
static uint ## bits ## _t \
endian ## bits ## dec(const void *buf) \
{ \
	uint ## bits ## _t u; \
	memcpy(&u, buf, sizeof(u)); \
	return endian ## bits ## toh (u); \
}
#if !HAVE_DECL_BE16DEC
__GEN_ENDIAN_DEC(16, be)
#endif
#if !HAVE_DECL_BE32DEC
__GEN_ENDIAN_DEC(32, be)
#endif
#if !HAVE_DECL_BE64DEC
__GEN_ENDIAN_DEC(64, be)
#endif
#if !HAVE_DECL_LE16DEC
__GEN_ENDIAN_DEC(16, le)
#endif
#if !HAVE_DECL_LE32DEC
__GEN_ENDIAN_DEC(32, le)
#endif
#if !HAVE_DECL_LE64DEC
__GEN_ENDIAN_DEC(64, le)
#endif
#undef __GEN_ENDIAN_DEC

/* <sys/mman.h> */

#ifndef MAP_FILE
#define MAP_FILE 0
#endif

/* HP-UX has MAP_ANONYMOUS but not MAP_ANON */
#ifndef MAP_ANON
#ifdef MAP_ANONYMOUS
#define MAP_ANON MAP_ANONYMOUS
#endif
#endif

/* <sys/param.h> */

#undef BIG_ENDIAN
#undef LITTLE_ENDIAN
#undef PDP_ENDIAN
#define BIG_ENDIAN 4321
#define LITTLE_ENDIAN 1234
#define PDP_ENDIAN 3412

#undef BYTE_ORDER
#if WORDS_BIGENDIAN
#define BYTE_ORDER BIG_ENDIAN
#else
#define BYTE_ORDER LITTLE_ENDIAN
#endif

/* all references of DEV_BSIZE in tools are for NetBSD's file images */
#undef DEV_BSIZE
#define DEV_BSIZE (1 << 9)

#undef MIN
#undef MAX
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

#ifndef MAXBSIZE
#define MAXBSIZE (64 * 1024)
#endif
#ifndef MAXFRAG
#define MAXFRAG 8
#endif
#ifndef MAXPHYS
#define MAXPHYS (64 * 1024)
#endif
#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN	256
#endif

/* XXX needed by makefs; this should be done in a better way */
#undef btodb
#define btodb(x) ((x) << 9)

#undef setbit
#undef clrbit
#undef isset
#undef isclr
#define	setbit(a,i)	((a)[(i)/NBBY] |= 1<<((i)%NBBY))
#define	clrbit(a,i)	((a)[(i)/NBBY] &= ~(1<<((i)%NBBY)))
#define	isset(a,i)	((a)[(i)/NBBY] & (1<<((i)%NBBY)))
#define	isclr(a,i)	(((a)[(i)/NBBY] & (1<<((i)%NBBY))) == 0)

#ifndef powerof2
#define powerof2(x) ((((x)-1)&(x))==0)
#endif

#undef roundup
#define roundup(x, y)	((((x)+((y)-1))/(y))*(y))

/* <sys/stat.h> */

#ifndef ALLPERMS
#define ALLPERMS (S_ISUID|S_ISGID|S_ISTXT|S_IRWXU|S_IRWXG|S_IRWXO)
#endif
#ifndef DEFFILEMODE
#define DEFFILEMODE (S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)
#endif
#ifndef S_ISTXT
#ifdef S_ISVTX
#define S_ISTXT S_ISVTX
#else
#define S_ISTXT 0
#endif
#endif

/* Protected by _NETBSD_SOURCE otherwise. */
#if HAVE_STRUCT_STAT_ST_FLAGS && (defined(__NetBSD__) || defined(__minix))
#define UF_SETTABLE     0x0000ffff
#define UF_NODUMP       0x00000001
#define UF_IMMUTABLE    0x00000002
#define UF_APPEND       0x00000004
#define UF_OPAQUE       0x00000008
#define SF_SETTABLE     0xffff0000
#define SF_ARCHIVED     0x00010000
#define SF_IMMUTABLE    0x00020000
#define SF_APPEND       0x00040000
#endif

/* <sys/syslimits.h> */

#ifndef LINE_MAX
#define LINE_MAX 2048
#endif

/* <sys/time.h> */

#ifndef timercmp
#define	timercmp(tvp, uvp, cmp)						\
	(((tvp)->tv_sec == (uvp)->tv_sec) ?				\
	    ((tvp)->tv_usec cmp (uvp)->tv_usec) :			\
	    ((tvp)->tv_sec cmp (uvp)->tv_sec))
#endif
#ifndef timeradd
#define	timeradd(tvp, uvp, vvp)						\
	do {								\
		(vvp)->tv_sec = (tvp)->tv_sec + (uvp)->tv_sec;		\
		(vvp)->tv_usec = (tvp)->tv_usec + (uvp)->tv_usec;	\
		if ((vvp)->tv_usec >= 1000000) {			\
			(vvp)->tv_sec++;				\
			(vvp)->tv_usec -= 1000000;			\
		}							\
	} while (/* CONSTCOND */ 0)
#endif
#ifndef timersub
#define	timersub(tvp, uvp, vvp)						\
	do {								\
		(vvp)->tv_sec = (tvp)->tv_sec - (uvp)->tv_sec;		\
		(vvp)->tv_usec = (tvp)->tv_usec - (uvp)->tv_usec;	\
		if ((vvp)->tv_usec < 0) {				\
			(vvp)->tv_sec--;				\
			(vvp)->tv_usec += 1000000;			\
		}							\
	} while (/* CONSTCOND */ 0)
#endif

/* <sys/types.h> */

#ifdef major
#undef major
#endif
#define major(x)        ((int32_t)((((x) & 0x000fff00) >>  8)))

#ifdef minor
#undef minor
#endif
#define minor(x)        ((int32_t)((((x) & 0xfff00000) >> 12) | \
                                   (((x) & 0x000000ff) >>  0)))
#ifdef makedev
#undef makedev
#endif
#define makedev(x,y)    ((dev_t)((((x) <<  8) & 0x000fff00) | \
			(((y) << 12) & 0xfff00000) | \
			(((y) <<  0) & 0x000000ff)))
#ifndef NBBY
#define NBBY 8
#endif

#if !HAVE_U_QUAD_T
/* #define, not typedef, as quad_t exists as a struct on some systems */
#define quad_t long long
#define u_quad_t unsigned long long
#define strtoq strtoll
#define strtouq strtoull
#endif

/* Has quad_t but these prototypes don't get pulled into scope. w/o we lose */
#if defined(__NetBSD__) || defined(__minix)
quad_t   strtoq(const char *, char **, int);
u_quad_t strtouq(const char *, char **, int);
#endif

#endif	/* !__NETBSD_COMPAT_DEFS_H__ */
