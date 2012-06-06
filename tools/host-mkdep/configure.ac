#	$NetBSD: configure.ac,v 1.3 2003/07/26 20:24:00 salo Exp $
#
# Autoconf definition file for mkdep.
#

AC_INIT([mkdep], [noversion], [tech-toolchain@NetBSD.org])

AC_PATH_PROG(BSHELL, sh, )
if test x"$BSHELL" = x; then
	AC_MSG_ERROR([sh must be somewhere on \$PATH])
fi

AC_CONFIG_FILES(host-mkdep)

AC_PROG_AWK
AC_PROG_CPP

AC_OUTPUT
