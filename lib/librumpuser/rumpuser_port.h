/*	$NetBSD: rumpuser_port.h,v 1.46 2015/09/21 21:50:16 pooka Exp $	*/

#ifndef _LIB_LIBRUMPUSER_RUMPUSER_PORT_H_
#define _LIB_LIBRUMPUSER_RUMPUSER_PORT_H_

/*
 * Define things found by autoconf.  buildrump.sh defines RUMPUSER_CONFIG,
 * the NetBSD build does not run autoconf during build and supplies the
 * necessary values here.  To update the NetBSD values, run ./configure
 * for an up-to-date NetBSD installation and insert rumpuser_config.h
 * in the space below, e.g. with ":r !sed -ne '/^\#/p' rumpuser_config.h"
 */
#if !defined(RUMPUSER_CONFIG)

#define HAVE_ARC4RANDOM_BUF 1
#define HAVE_CHFLAGS 1
#define HAVE_CLOCKID_T 1
#define HAVE_CLOCK_GETTIME 1
#define HAVE_CLOCK_NANOSLEEP 1
#define HAVE_DLINFO 1
#define HAVE_FSYNC_RANGE 1
#define HAVE_GETENV_R 1
#define HAVE_GETPROGNAME 1
#define HAVE_GETSUBOPT 1
#define HAVE_INTTYPES_H 1
#define HAVE_KQUEUE 1
#define HAVE_MEMORY_H 1
#define HAVE_PATHS_H 1
#define HAVE_POSIX_MEMALIGN 1
#define HAVE_PTHREAD_SETNAME3 1
#define HAVE_REGISTER_T 1
#define HAVE_SETPROGNAME 1
#define HAVE_STDINT_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRINGS_H 1
#define HAVE_STRING_H 1
#define HAVE_STRSUFTOLL 1
#define HAVE_STRUCT_SOCKADDR_IN_SIN_LEN 1
#define HAVE_SYS_ATOMIC_H 1
#define HAVE_SYS_CDEFS_H 1
#define HAVE_SYS_DISKLABEL_H 1
#define HAVE_SYS_DISK_H 1
#define HAVE_SYS_DKIO_H 1
#define HAVE_SYS_PARAM_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_SYSCTL_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_UNISTD_H 1
#define HAVE___QUOTACTL 1
#define HAVE_UTIMENSAT 1
#define PACKAGE_BUGREPORT "http://rumpkernel.org/"
#define PACKAGE_NAME "rumpuser-posix"
#define PACKAGE_STRING "rumpuser-posix 999"
#define PACKAGE_TARNAME "rumpuser-posix"
#define PACKAGE_URL ""
#define PACKAGE_VERSION "999"
#define STDC_HEADERS 1
#ifndef _DARWIN_USE_64_BIT_INODE
# define _DARWIN_USE_64_BIT_INODE 1
#endif

#else /* RUMPUSER_CONFIG */
#include "rumpuser_config.h"
#endif

#if defined(__linux__) || defined(__GNU__) || defined(__GLIBC__)
#define _GNU_SOURCE
#endif

#if defined(HAVE_SYS_CDEFS_H)
#include <sys/cdefs.h>
#endif

/*
 * Some versions of FreeBSD (e.g. 9.2) contain C11 stuff without
 * any obvious way to expose the protos.  Kludge around it.
 */
#ifdef __FreeBSD__
#if __ISO_C_VISIBLE < 2011
#undef __ISO_C_VISIBLE
#define __ISO_C_VISIBLE 2011
#endif
#endif

#if defined(HAVE_SYS_PARAM_H)
#include <sys/param.h>
#endif

#ifndef MIN
#define MIN(a,b)        ((/*CONSTCOND*/(a)<(b))?(a):(b))
#endif
#ifndef MAX
#define MAX(a,b)        ((/*CONSTCOND*/(a)>(b))?(a):(b))
#endif

#if !defined(HAVE_GETSUBOPT)
static inline int
getsubopt(char **optionp, char * const *tokens, char **valuep)
{

	/* TODO make a definition */
	return -1;
}
#endif

#if !defined(HAVE_CLOCKID_T)
typedef int clockid_t;
#endif

#ifdef __ANDROID__
#include <stdint.h>
typedef uint16_t in_port_t;
#include <sys/select.h>
#define atomic_inc_uint(x)  __sync_fetch_and_add(x, 1)
#define atomic_dec_uint(x)  __sync_fetch_and_sub(x, 1)
#endif

/* sunny magic */
#if defined(__sun__)
#  if defined(RUMPUSER_NO_FILE_OFFSET_BITS)
#    undef _FILE_OFFSET_BITS
#    define _FILE_OFFSET_BITS 32
#  endif
#endif

#if !defined(HAVE_CLOCK_GETTIME)
#include <sys/time.h>
#define	CLOCK_REALTIME	0
static inline int
clock_gettime(clockid_t clk, struct timespec *ts)
{
	struct timeval tv;

	if (gettimeofday(&tv, 0) == 0) {
		ts->tv_sec = tv.tv_sec;
		ts->tv_nsec = tv.tv_usec * 1000;
		return 0;
	}
	return -1;
}
#endif

#if defined(__APPLE__)
#include <libkern/OSAtomic.h>
#define	atomic_inc_uint(x)	OSAtomicIncrement32((volatile int32_t *)(x))
#define	atomic_dec_uint(x)	OSAtomicDecrement32((volatile int32_t *)(x))
#endif

#include <sys/types.h>

#if !defined(HAVE_GETENV_R)
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
static inline int 
getenv_r(const char *name, char *buf, size_t buflen)
{
	char *tmp;

	if ((tmp = getenv(name)) != NULL) {
		if (strlen(tmp) >= buflen) {
			errno = ERANGE;
			return -1;
		}
		strcpy(buf, tmp);
		return 0;
	} else {
		errno = ENOENT;
		return -1;
	}
}
#endif

#if !defined(HAVE_POSIX_MEMALIGN)
#if !defined(HAVE_MEMALIGN)
#error method for aligned memory allocation required
#endif
#include <sys/sysmacros.h>
#include <stdlib.h>
static inline int
posix_memalign(void **ptr, size_t align, size_t size)
{

	*ptr = memalign(align, size);
	if (*ptr == NULL)
		return ENOMEM;
	return 0;
}
#endif

/*
 * For NetBSD, use COHERENCY_UNIT as the lock alignment size.
 * On other platforms, just guess it to be 64.
 */
#ifdef __NetBSD__
#define RUMPUSER_LOCKALIGN COHERENCY_UNIT
#else
#define RUMPUSER_LOCKALIGN 64
#endif

#if !defined(HAVE_ALIGNED_ALLOC)
#include <stdlib.h>
static inline void *
aligned_alloc(size_t alignment, size_t size)
{
	void *ptr;
	int rv;

	rv = posix_memalign(&ptr, alignment, size);
	return rv ? NULL : ptr;
}
#endif

#ifndef __RCSID
#define __RCSID(a)
#endif

#ifndef INFTIM
#define INFTIM (-1)
#endif

#ifndef _DIAGASSERT
#define _DIAGASSERT(_p_)
#endif

#if !defined(HAVE_STRUCT_SOCKADDR_IN_SIN_LEN)
#define SIN_SETLEN(a,b)
#else
#define SIN_SETLEN(_sin_, _len_) _sin_.sin_len = _len_
#endif

#ifndef __predict_true
#define __predict_true(a) a
#define __predict_false(a) a
#endif

#ifndef __dead
#define __dead __attribute__((__noreturn__))
#endif

#ifndef __printflike
#ifdef __GNUC__
#define __printflike(a,b) __attribute__((__format__ (__printf__,a,b)))
#else
#define __printflike(a,b)
#endif
#endif

#ifndef __noinline
#ifdef __GNUC__
#define __noinline __attribute__((__noinline__))
#else
#define __noinline
#endif
#endif

#ifndef __arraycount
#define __arraycount(_ar_) (sizeof(_ar_)/sizeof(_ar_[0]))
#endif

#ifndef __UNCONST
#define __UNCONST(_a_) ((void *)(unsigned long)(const void *)(_a_))
#endif

#ifndef __CONCAT
#define __CONCAT(x,y)	x ## y
#endif

#ifndef __STRING
#define __STRING(x)	#x
#endif

#ifndef __NetBSD_Prereq__
#define __NetBSD_Prereq__(a,b,c) 0
#endif

#include <sys/socket.h>

#if !defined(__CMSG_ALIGN)
#ifdef CMSG_ALIGN
#define __CMSG_ALIGN(a) CMSG_ALIGN(a)
#endif
#endif

#ifndef PF_LOCAL
#define PF_LOCAL PF_UNIX
#endif
#ifndef AF_LOCAL
#define AF_LOCAL AF_UNIX
#endif

/* pfft, but what are you going to do? */
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

#if !defined(HAVE_REGISTER_T) && !defined(RUMP_REGISTER_T)
#define RUMP_REGISTER_T long
typedef RUMP_REGISTER_T register_t;
#endif

#include <sys/time.h>

#ifndef TIMEVAL_TO_TIMESPEC
#define TIMEVAL_TO_TIMESPEC(tv, ts)		\
do {						\
	(ts)->tv_sec  = (tv)->tv_sec;		\
	(ts)->tv_nsec = (tv)->tv_usec * 1000;	\
} while (/*CONSTCOND*/0)
#endif

#if !defined(HAVE_SETPROGNAME)
#define setprogname(a)
#endif

/* at least GNU Hurd does not specify various common hardcoded constants */
#include <limits.h>
#ifndef MAXPATHLEN
#define MAXPATHLEN	4096
#endif
#ifndef PATH_MAX
#define PATH_MAX	MAXPATHLEN
#endif
#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN	256
#endif

#endif /* _LIB_LIBRUMPUSER_RUMPUSER_PORT_H_ */
