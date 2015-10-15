# $NetBSD: configure.ac,v 1.42 2014/03/09 00:33:50 agc Exp $
#
# Process this file with autoconf to produce a configure script.

AC_INIT([netpgp],[20140220],[Alistair Crooks <agc@netbsd.org> c0596823])
AC_PREREQ(2.69)
AC_REVISION([$Revision: 1.42 $])

AS_SHELL_SANITIZE

AC_CONFIG_SRCDIR([src/netpgp/netpgp.c])
AC_CONFIG_AUX_DIR([buildaux])
AC_CONFIG_HEADER([src/lib/config.h])

AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AM_MAINTAINER_MODE

AC_CANONICAL_HOST
CANONICAL_HOST=$host
AC_SUBST([CANONICAL_HOST])

# Checks for programs.
#
AC_PROG_MAKE_SET
AC_PROG_AWK
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LIBTOOL
AC_PROG_LN_S

dnl based on AC_PROG_RANLIB from autoconf
m4_ifndef([AC_PROG_AR],[dnl
AN_MAKEVAR([AR], [AC_PROG_AR])
AN_PROGRAM([ar], [AC_PROG_AR])
AC_DEFUN([AC_PROG_AR],
[AC_CHECK_TOOL(AR, ar, :)])
])

AC_PROG_AR

# Compiler characteristics
#
AC_MSG_CHECKING([if using gcc])
AS_IF([test "$GCC" = "yes"],
      [WARNCFLAGS="-Werror -Wall -Wpointer-arith"
       AC_MSG_RESULT([yes; adding to CFLAGS: "$WARNCFLAGS"])],
      [WARNCFLAGS=""
       AC_MSG_RESULT([no])])
AC_SUBST([WARNCFLAGS])

# try to see if we need to link with -ldl
AC_SEARCH_LIBS(dlopen, dl)

# Checks for header files.
#
AC_HEADER_STDC
AC_CHECK_HEADERS([CommonCrypto/CommonDigest.h])
AC_CHECK_HEADERS([dmalloc.h direct.h errno.h fcntl.h \
                 inttypes.h limits.h malloc.h zlib.h])
AC_CHECK_HEADERS([openssl/aes.h openssl/bn.h openssl/camellia.h openssl/cast.h \
		  openssl/des.h openssl/dsa.h openssl/err.h openssl/idea.h \
		  openssl/md5.h openssl/rand.h openssl/rsa.h openssl/sha.h \
		  openssl/err.h openssl/sha.h])
AC_CHECK_HEADERS([sys/cdefs.h sys/file.h sys/mman.h sys/param.h \
                  sys/resource.h sys/uio.h])

AC_CHECK_HEADERS([bzlib.h],
                 [],
                 [AC_MSG_FAILURE([missing <bzlib.h>; is bzip2 installed?])])

# Checks for typedefs, structures, and compiler characteristics.
#
AC_TYPE_LONG_LONG_INT
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

# check for openssl
m4_include([ax_check_openssl.m4])

AX_CHECK_OPENSSL([LIBS="$LIBS $OPENSSL_LIBS" INCLUDES="$INCLUDES $OPENSSL_INCLUDES"],
	[AC_MSG_ERROR([Missing OpenSSL])])

AC_CHECK_TYPES([SHA256_CTX],
               [],
               [AC_MSG_FAILURE([missing SHA256_CTX; is openssl 0.9.8 or newer installed?])],
              [[
#if defined(HAVE_SYS_TYPES_H)
#include <sys/types.h>
#endif
#if defined(HAVE_OPENSSL_SHA_H)
#include <openssl/sha.h>
#endif
#if defined(HAVE_COMMONCRYPTO_COMMONDIGEST_H)
#define COMMON_DIGEST_FOR_OPENSSL 1
#include <CommonCrypto/CommonDigest.h>
#endif
]])

# Checks for library functions.
#
AC_SEARCH_LIBS([gzopen], [z])
AC_SEARCH_LIBS([BZ2_bzDecompress], [bz2])
AC_SEARCH_LIBS([RSA_public_encrypt], [crypto])

# Initialize the testsuite
#
AC_CONFIG_TESTDIR([tests])
AM_MISSING_PROG([AUTOM4TE], [autom4te])

# Generate the Makefiles
#
AC_CONFIG_FILES([
        Makefile
        include/Makefile
        src/Makefile
        src/libmj/Makefile
        src/lib/Makefile
        src/netpgp/Makefile
        src/netpgpkeys/Makefile
        tests/Makefile
        tests/atlocal
])
AC_OUTPUT

# Display feature results
#
AC_MSG_NOTICE([ === Configuration results ===])
AC_MSG_NOTICE([Version:             $PACKAGE_VERSION])
AC_MSG_NOTICE([C compiler:          $CC])
AC_MSG_NOTICE([Warning CFLAGS:      $WARNCFLAGS])
AC_MSG_NOTICE([=============================])
