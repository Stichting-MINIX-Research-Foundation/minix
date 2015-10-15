/*	$NetBSD: private.h,v 1.44 2015/10/09 17:21:45 christos Exp $	*/

#ifndef PRIVATE_H
#define PRIVATE_H

/* NetBSD defaults */
#define TM_GMTOFF	tm_gmtoff
#define TM_ZONE		tm_zone
#define STD_INSPIRED	1
#define HAVE_LONG_DOUBLE 1

/* For when we build zic as a host tool. */
#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

/*
** This file is in the public domain, so clarified as of
** 1996-06-05 by Arthur David Olson.
*/

/*
** This header is for use ONLY with the time conversion code.
** There is no guarantee that it will remain unchanged,
** or that it will remain at all.
** Do NOT copy it to any system include directory.
** Thank you!
*/

#define GRANDPARENTED	"Local time zone must be set--see zic manual page"

/*
** Defaults for preprocessor symbols.
** You can override these in your C compiler options, e.g. '-DHAVE_GETTEXT=1'.
*/

#ifndef HAVE_GETTEXT
#define HAVE_GETTEXT		0
#endif /* !defined HAVE_GETTEXT */

#ifndef HAVE_INCOMPATIBLE_CTIME_R
#define HAVE_INCOMPATIBLE_CTIME_R	0
#endif /* !defined INCOMPATIBLE_CTIME_R */

#ifndef HAVE_LINK
#define HAVE_LINK		1
#endif /* !defined HAVE_LINK */

#ifndef HAVE_STRDUP
#define HAVE_STRDUP 1
#endif

#ifndef HAVE_SYMLINK
#define HAVE_SYMLINK		1
#endif /* !defined HAVE_SYMLINK */

#ifndef HAVE_SYS_STAT_H
#define HAVE_SYS_STAT_H		1
#endif /* !defined HAVE_SYS_STAT_H */

#ifndef HAVE_SYS_WAIT_H
#define HAVE_SYS_WAIT_H		1
#endif /* !defined HAVE_SYS_WAIT_H */

#ifndef HAVE_UNISTD_H
#define HAVE_UNISTD_H		1
#endif /* !defined HAVE_UNISTD_H */

#ifndef HAVE_UTMPX_H
#define HAVE_UTMPX_H		1
#endif /* !defined HAVE_UTMPX_H */

#ifndef NETBSD_INSPIRED
# define NETBSD_INSPIRED 1
#endif

#if HAVE_INCOMPATIBLE_CTIME_R
#define asctime_r _incompatible_asctime_r
#define ctime_r _incompatible_ctime_r
#endif /* HAVE_INCOMPATIBLE_CTIME_R */

/* Enable tm_gmtoff and tm_zone on GNUish systems.  */
#define _GNU_SOURCE 1
/* Fix asctime_r on Solaris 10.  */
#define _POSIX_PTHREAD_SEMANTICS 1
/* Enable strtoimax on Solaris 10.  */
#define __EXTENSIONS__ 1

/*
** Nested includes
*/

#if !defined(__NetBSD__) && !defined(__minix)
/* Avoid clashes with NetBSD by renaming NetBSD's declarations.  */
#define localtime_rz sys_localtime_rz
#define mktime_z sys_mktime_z
#define posix2time_z sys_posix2time_z
#define time2posix_z sys_time2posix_z
#define timezone_t sys_timezone_t
#define tzalloc sys_tzalloc
#define tzfree sys_tzfree
#include <time.h>
#undef localtime_rz
#undef mktime_z
#undef posix2time_z
#undef time2posix_z
#undef timezone_t
#undef tzalloc
#undef tzfree
#else
#include "time.h"
#endif

#include "sys/types.h"	/* for time_t */
#include "stdio.h"
#include "string.h"
#include "limits.h"	/* for CHAR_BIT et al. */
#include "stdlib.h"

#include "errno.h"

#ifndef ENAMETOOLONG
# define ENAMETOOLONG EINVAL
#endif
#ifndef EOVERFLOW
# define EOVERFLOW EINVAL
#endif

#if HAVE_GETTEXT
#include "libintl.h"
#endif /* HAVE_GETTEXT */

#if HAVE_SYS_WAIT_H
#include <sys/wait.h>	/* for WIFEXITED and WEXITSTATUS */
#endif /* HAVE_SYS_WAIT_H */

#ifndef WIFEXITED
#define WIFEXITED(status)	(((status) & 0xff) == 0)
#endif /* !defined WIFEXITED */
#ifndef WEXITSTATUS
#define WEXITSTATUS(status)	(((status) >> 8) & 0xff)
#endif /* !defined WEXITSTATUS */

#if HAVE_UNISTD_H
#include "unistd.h"	/* for F_OK, R_OK, and other POSIX goodness */
#endif /* HAVE_UNISTD_H */

#ifndef HAVE_STRFTIME_L
# if _POSIX_VERSION < 200809
#  define HAVE_STRFTIME_L 0
# else
#  define HAVE_STRFTIME_L 1
# endif
#endif

#ifndef F_OK
#define F_OK	0
#endif /* !defined F_OK */
#ifndef R_OK
#define R_OK	4
#endif /* !defined R_OK */

/* Unlike <ctype.h>'s isdigit, this also works if c < 0 | c > UCHAR_MAX. */
#define is_digit(c) ((unsigned)(c) - '0' <= 9)

/*
** Define HAVE_STDINT_H's default value here, rather than at the
** start, since __GLIBC__'s value depends on previously-included
** files.
** (glibc 2.1 and later have stdint.h, even with pre-C99 compilers.)
*/
#ifndef HAVE_STDINT_H
#define HAVE_STDINT_H \
   (199901 <= __STDC_VERSION__ \
    || 2 < __GLIBC__ + (1 <= __GLIBC_MINOR__)	\
    || __CYGWIN__)
#endif /* !defined HAVE_STDINT_H */

#if HAVE_STDINT_H
#include "stdint.h"
#endif /* !HAVE_STDINT_H */

#ifndef HAVE_INTTYPES_H
# define HAVE_INTTYPES_H HAVE_STDINT_H
#endif
#if HAVE_INTTYPES_H
# include <inttypes.h>
#endif

/* Pre-C99 GCC compilers define __LONG_LONG_MAX__ instead of LLONG_MAX.  */
#ifdef __LONG_LONG_MAX__
# ifndef LLONG_MAX
#  define LLONG_MAX __LONG_LONG_MAX__
# endif
# ifndef LLONG_MIN
#  define LLONG_MIN (-1 - LLONG_MAX)
# endif
#endif

#ifndef INT_FAST64_MAX
# ifdef LLONG_MAX
typedef long long	int_fast64_t;
#  define INT_FAST64_MIN LLONG_MIN
#  define INT_FAST64_MAX LLONG_MAX
# else
#  if LONG_MAX >> 31 < 0xffffffff
Please use a compiler that supports a 64-bit integer type (or wider);
you may need to compile with "-DHAVE_STDINT_H".
#  endif
typedef long		int_fast64_t;
#  define INT_FAST64_MIN LONG_MIN
#  define INT_FAST64_MAX LONG_MAX
# endif
#endif

#ifndef SCNdFAST64
# if INT_FAST64_MAX == LLONG_MAX
#  define SCNdFAST64 "lld"
# else
#  define SCNdFAST64 "ld"
# endif
#endif

#ifndef INT_FAST32_MAX
# if INT_MAX >> 31 == 0
typedef long int_fast32_t;
#  define INT_FAST32_MAX LONG_MAX
#  define INT_FAST32_MIN LONG_MIN
# else
typedef int int_fast32_t;
#  define INT_FAST32_MAX INT_MAX
#  define INT_FAST32_MIN INT_MIN
# endif
#endif

#ifndef INTMAX_MAX
# ifdef LLONG_MAX
typedef long long intmax_t;
#  define strtoimax strtoll
#  define INTMAX_MAX LLONG_MAX
#  define INTMAX_MIN LLONG_MIN
# else
typedef long intmax_t;
#  define strtoimax strtol
#  define INTMAX_MAX LONG_MAX
#  define INTMAX_MIN LONG_MIN
# endif
#endif

#ifndef PRIdMAX
# if INTMAX_MAX == LLONG_MAX
#  define PRIdMAX "lld"
# else
#  define PRIdMAX "ld"
# endif
#endif

#ifndef UINT_FAST64_MAX
# if defined ULLONG_MAX || defined __LONG_LONG_MAX__
typedef unsigned long long uint_fast64_t;
# else
#  if ULONG_MAX >> 31 >> 1 < 0xffffffff
Please use a compiler that supports a 64-bit integer type (or wider);
you may need to compile with "-DHAVE_STDINT_H".
#  endif
typedef unsigned long	uint_fast64_t;
# endif
#endif

#ifndef UINTMAX_MAX
# if defined ULLONG_MAX || defined __LONG_LONG_MAX__
typedef unsigned long long uintmax_t;
# else
typedef unsigned long uintmax_t;
# endif
#endif

#ifndef PRIuMAX
# if defined ULLONG_MAX || defined __LONG_LONG_MAX__
#  define PRIuMAX "llu"
# else
#  define PRIuMAX "lu"
# endif
#endif

#ifndef INT32_MAX
#define INT32_MAX 0x7fffffff
#endif /* !defined INT32_MAX */
#ifndef INT32_MIN
#define INT32_MIN (-1 - INT32_MAX)
#endif /* !defined INT32_MIN */

#ifndef SIZE_MAX
#define SIZE_MAX ((size_t) -1)
#endif

#if 2 < __GNUC__ + (96 <= __GNUC_MINOR__)
# define ATTRIBUTE_CONST __attribute__ ((__const__))
# define ATTRIBUTE_PURE __attribute__ ((__pure__))
# define ATTRIBUTE_FORMAT(spec) __attribute__ ((__format__ spec))
#else
# define ATTRIBUTE_CONST /* empty */
# define ATTRIBUTE_PURE /* empty */
# define ATTRIBUTE_FORMAT(spec) /* empty */
#endif

#if !defined _Noreturn && __STDC_VERSION__ < 201112
# if 2 < __GNUC__ + (8 <= __GNUC_MINOR__)
#  define _Noreturn __attribute__ ((__noreturn__))
# else
#  define _Noreturn
# endif
#endif

#if __STDC_VERSION__ < 199901 && !defined restrict
# define restrict /* empty */
#endif

/*
** Workarounds for compilers/systems.
*/

/*
** Compile with -Dtime_tz=T to build the tz package with a private
** time_t type equivalent to T rather than the system-supplied time_t.
** This debugging feature can test unusual design decisions
** (e.g., time_t wider than 'long', or unsigned time_t) even on
** typical platforms.
*/
#ifdef time_tz
# ifdef LOCALTIME_IMPLEMENTATION
static time_t sys_time(time_t *x) { return time(x); }
# endif

typedef time_tz tz_time_t;

# undef  ctime
# define ctime tz_ctime
# undef  ctime_r
# define ctime_r tz_ctime_r
# undef  difftime
# define difftime tz_difftime
# undef  gmtime
# define gmtime tz_gmtime
# undef  gmtime_r
# define gmtime_r tz_gmtime_r
# undef  localtime
# define localtime tz_localtime
# undef  localtime_r
# define localtime_r tz_localtime_r
# undef  localtime_rz
# define localtime_rz tz_localtime_rz
# undef  mktime
# define mktime tz_mktime
# undef  mktime_z
# define mktime_z tz_mktime_z
# undef  offtime
# define offtime tz_offtime
# undef  posix2time
# define posix2time tz_posix2time
# undef  posix2time_z
# define posix2time_z tz_posix2time_z
# undef  time
# define time tz_time
# undef  time2posix
# define time2posix tz_time2posix
# undef  time2posix_z
# define time2posix_z tz_time2posix_z
# undef  time_t
# define time_t tz_time_t
# undef  timegm
# define timegm tz_timegm
# undef  timelocal
# define timelocal tz_timelocal
# undef  timeoff
# define timeoff tz_timeoff
# undef  tzalloc
# define tzalloc tz_tzalloc
# undef  tzfree
# define tzfree tz_tzfree
# undef  tzset
# define tzset tz_tzset
# undef  tzsetwall
# define tzsetwall tz_tzsetwall

char *ctime(time_t const *);
char *ctime_r(time_t const *, char *);
double difftime(time_t, time_t);
struct tm *gmtime(time_t const *);
struct tm *gmtime_r(time_t const *restrict, struct tm *restrict);
struct tm *localtime(time_t const *);
struct tm *localtime_r(time_t const *restrict, struct tm *restrict);
time_t mktime(struct tm *);
time_t time(time_t *);
void tzset(void);
#endif

/*
** Some time.h implementations don't declare asctime_r.
** Others might define it as a macro.
** Fix the former without affecting the latter.
** Similarly for timezone, daylight, and altzone.
*/

#ifndef asctime_r
extern char *	asctime_r(struct tm const *restrict, char *restrict);
#endif

#if defined(USG_COMPAT) && !defined(__NetBSD__) && !defined(__minix)
# ifndef timezone
extern long timezone;
# endif
# ifndef daylight
extern int daylight;
# endif
#endif
#if defined ALTZONE && !defined altzone
extern long altzone;
#endif

/*
** The STD_INSPIRED functions are similar, but most also need
** declarations if time_tz is defined.
*/

#ifdef STD_INSPIRED
# if !defined tzsetwall || defined time_tz
void tzsetwall(void);
# endif
# if !defined offtime || defined time_tz
struct tm *offtime(time_t const *, long);
# endif
# if !defined timegm || defined time_tz
time_t timegm(struct tm *);
# endif
# if !defined timelocal || defined time_tz
time_t timelocal(struct tm *);
# endif
# if !defined timeoff || defined time_tz
time_t timeoff(struct tm *, long);
# endif
# if !defined time2posix || defined time_tz
time_t time2posix(time_t);
# endif
# if !defined posix2time || defined time_tz
time_t posix2time(time_t);
# endif
#endif

/* Infer TM_ZONE on systems where this information is known, but suppress
   guessing if NO_TM_ZONE is defined.  Similarly for TM_GMTOFF.  */
#if (defined __GLIBC__ \
     || defined __FreeBSD__ || defined __minix || defined __NetBSD__ || defined __OpenBSD__ \
     || (defined __APPLE__ && defined __MACH__))
# if !defined TM_GMTOFF && !defined NO_TM_GMTOFF
#  define TM_GMTOFF tm_gmtoff
# endif
# if !defined TM_ZONE && !defined NO_TM_ZONE
#  define TM_ZONE tm_zone
# endif
#endif

/*
** Define functions that are ABI compatible with NetBSD but have
** better prototypes.  NetBSD 6.1.4 defines a pointer type timezone_t
** and labors under the misconception that 'const timezone_t' is a
** pointer to a constant.  This use of 'const' is ineffective, so it
** is not done here.  What we call 'struct state' NetBSD calls
** 'struct __state', but this is a private name so it doesn't matter.
*/
#if !defined(__NetBSD__) && !defined(__minix)
#if NETBSD_INSPIRED
typedef struct state *timezone_t;
struct tm *localtime_rz(timezone_t restrict, time_t const *restrict,
			struct tm *restrict);
time_t mktime_z(timezone_t restrict, struct tm *restrict);
timezone_t tzalloc(char const *);
void tzfree(timezone_t);
# ifdef STD_INSPIRED
#  if !defined posix2time_z || defined time_tz
time_t posix2time_z(timezone_t __restrict, time_t) ATTRIBUTE_PURE;
#  endif
#  if !defined time2posix_z || defined time_tz
time_t time2posix_z(timezone_t __restrict, time_t) ATTRIBUTE_PURE;
#  endif
# endif
#endif
#endif

/*
** Finally, some convenience items.
*/

#if __STDC_VERSION__ < 199901
# define true 1
# define false 0
# define bool int
#else
# include <stdbool.h>
#endif

#ifndef TYPE_BIT
#define TYPE_BIT(type)	(sizeof (type) * CHAR_BIT)
#endif /* !defined TYPE_BIT */

#ifndef TYPE_SIGNED
#define TYPE_SIGNED(type) (/*CONSTCOND*/((type) -1) < 0)
#endif /* !defined TYPE_SIGNED */

#define TWOS_COMPLEMENT(t) (/*CONSTCOND*/(t) ~ (t) 0 < 0)

/* Max and min values of the integer type T, of which only the bottom
   B bits are used, and where the highest-order used bit is considered
   to be a sign bit if T is signed.  */
#define MAXVAL(t, b) /*LINTED*/					\
  ((t) (((t) 1 << ((b) - 1 - TYPE_SIGNED(t)))			\
	- 1 + ((t) 1 << ((b) - 1 - TYPE_SIGNED(t)))))
#define MINVAL(t, b)						\
  ((t) (TYPE_SIGNED(t) ? - TWOS_COMPLEMENT(t) - MAXVAL(t, b) : 0))

#ifdef LOCALTIME_IMPLEMENTATION
/* The minimum and maximum finite time values.  This assumes no padding.  */
static time_t const time_t_min = MINVAL(time_t, TYPE_BIT(time_t));
static time_t const time_t_max = MAXVAL(time_t, TYPE_BIT(time_t));
#endif

#ifndef INT_STRLEN_MAXIMUM
/*
** 302 / 1000 is log10(2.0) rounded up.
** Subtract one for the sign bit if the type is signed;
** add one for integer division truncation;
** add one more for a minus sign if the type is signed.
*/
#define INT_STRLEN_MAXIMUM(type) \
	((TYPE_BIT(type) - TYPE_SIGNED(type)) * 302 / 1000 + \
	1 + TYPE_SIGNED(type))
#endif /* !defined INT_STRLEN_MAXIMUM */

/*
** INITIALIZE(x)
*/

#if defined(__GNUC__) || defined(__lint__)
# define INITIALIZE(x)	((x) = 0)
#else
# define INITIALIZE(x)
#endif

#ifndef UNINIT_TRAP
# define UNINIT_TRAP 0
#endif

/*
** For the benefit of GNU folk...
** '_(MSGID)' uses the current locale's message library string for MSGID.
** The default is to use gettext if available, and use MSGID otherwise.
*/

#ifndef _
#if HAVE_GETTEXT
#define _(msgid) gettext(msgid)
#else /* !HAVE_GETTEXT */
#define _(msgid) msgid
#endif /* !HAVE_GETTEXT */
#endif /* !defined _ */

#if !defined TZ_DOMAIN && defined HAVE_GETTEXT
# define TZ_DOMAIN "tz"
#endif

#if HAVE_INCOMPATIBLE_CTIME_R
#undef asctime_r
#undef ctime_r
char *asctime_r(struct tm const *, char *);
char *ctime_r(time_t const *, char *);
#endif /* HAVE_INCOMPATIBLE_CTIME_R */

#ifndef YEARSPERREPEAT
#define YEARSPERREPEAT		400	/* years before a Gregorian repeat */
#endif /* !defined YEARSPERREPEAT */

/*
** The Gregorian year averages 365.2425 days, which is 31556952 seconds.
*/

#ifndef AVGSECSPERYEAR
#define AVGSECSPERYEAR		31556952L
#endif /* !defined AVGSECSPERYEAR */

#ifndef SECSPERREPEAT
#define SECSPERREPEAT		((int_fast64_t) YEARSPERREPEAT * (int_fast64_t) AVGSECSPERYEAR)
#endif /* !defined SECSPERREPEAT */

#ifndef SECSPERREPEAT_BITS
#define SECSPERREPEAT_BITS	34	/* ceil(log2(SECSPERREPEAT)) */
#endif /* !defined SECSPERREPEAT_BITS */

#endif /* !defined PRIVATE_H */
