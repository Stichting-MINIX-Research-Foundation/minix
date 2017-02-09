#
# Rump kernel POSIX hypervisor autoconf support.
#
# NOTE!  regen by doing the following
#   1) autoreconf -iv
#   2) edit rumpuser_port.h (search for RUMPUSER_CONFIG for instructions)
#   3) rm -rf autom4te.cache
#   4) commit to NetBSD, pullup to repo.rumpkernel.org
#

AC_PREREQ([2.66])
AC_INIT([rumpuser-posix], [999], [http://rumpkernel.org/])

AC_CONFIG_HEADERS([rumpuser_config.h])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])

AC_LANG([C])

AC_SYS_LARGEFILE

AC_CHECK_HEADERS([sys/param.h sys/sysctl.h sys/disk.h \
	sys/disklabel.h sys/dkio.h sys/atomic.h paths.h])

AC_CANONICAL_TARGET

AC_CHECK_TYPES([clockid_t, register_t])

AC_CHECK_FUNCS([kqueue chflags strsuftoll setprogname getprogname	\
	getenv_r posix_memalign memalign aligned_alloc	\
	arc4random_buf getsubopt fsync_range __quotactl utimensat])

AC_TRY_LINK_FUNC([clock_nanosleep],,
        AC_CHECK_LIB([rt], [clock_nanosleep])
)
AC_CHECK_LIB([rt], [clock_gettime],
	AC_DEFINE([HAVE_CLOCK_GETTIME], 1, [clock_gettime]),
	AC_TRY_LINK_FUNC([clock_gettime],
	    AC_DEFINE([HAVE_CLOCK_GETTIME], 1, [clock_gettime])))
AC_CHECK_LIB([rt], [clock_nanosleep],
	AC_DEFINE([HAVE_CLOCK_NANOSLEEP], 1, [clock_nanosleep]),
	AC_TRY_LINK_FUNC([clock_nanosleep],
	    AC_DEFINE([HAVE_CLOCK_NANOSLEEP], 1, [clock_nanosleep])))
AC_CHECK_LIB([dl], [dlinfo],
	AC_DEFINE([HAVE_DLINFO], 1, [dlinfo]),
	AC_TRY_LINK_FUNC([dlinfo], AC_DEFINE([HAVE_DLINFO], 1, [dlinfo])))

AC_CHECK_MEMBERS([struct sockaddr_in.sin_len],,,[#include <netinet/in.h>])

dnl
dnl pthread_setname() sillyness is a bit longer; we need the signature
dnl
SAVE_CFLAGS="${CFLAGS}"
CFLAGS="${SAVE_CFLAGS} -Werror"

dnl check sys/cdefs.h creatively to process only with cc, not cpp
dnl (sys/cdefs.h in at least in musl contains a #warning)
AC_CHECK_HEADERS([sys/cdefs.h], [], [], [#include <sys/cdefs.h>])

SAVE_LIBS="${LIBS}"
LIBS="${LIBS} -lpthread"
AC_MSG_CHECKING([for two-argument pthread_setname_np()])
AC_COMPILE_IFELSE(
	[AC_LANG_PROGRAM(
		[[#define _GNU_SOURCE
		  #include <pthread.h>]],
		[[pthread_t pt;]
		[pthread_setname_np(pt, "x");return 0;]])
	],[
		AC_MSG_RESULT([yes])
		AC_DEFINE(HAVE_PTHREAD_SETNAME2, [1],
		    [Define to 1 if you have 2-arg pthread_setname_np()])
	],[
		AC_MSG_RESULT([no])
])
AC_MSG_CHECKING([for three-argument pthread_setname_np()])
AC_COMPILE_IFELSE(
	[AC_LANG_PROGRAM(
		[[#define _GNU_SOURCE
		  #include <pthread.h>]],
		[[pthread_t pt;]
		[pthread_setname_np(pt, "X", (void *)0);return 0;]])
	],[
		AC_MSG_RESULT([yes])
		AC_DEFINE(HAVE_PTHREAD_SETNAME3, [1],
		    [Define to 1 if you have 3-arg pthread_setname_np()])
	],[
		AC_MSG_RESULT([no])
])
LIBS="${SAVELIBS}"

AC_MSG_CHECKING([for ioctl cmd being int])
AC_COMPILE_IFELSE(
	[AC_LANG_PROGRAM(
		[[#include <sys/ioctl.h>
		  #include <unistd.h>
		  int ioctl(int fd, int, ...);]],
		[[]
		[return 0;]])
	],[
		AC_MSG_RESULT([yes])
		AC_DEFINE(HAVE_IOCTL_CMD_INT, [1],
		    [Define to 1 if ioctl()'s cmd arg is int])
	],[
		AC_MSG_RESULT([no])
])
CFLAGS="${SAVE_CFLAGS}"

AC_OUTPUT
