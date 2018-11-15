dnl
dnl perl and some of its module are required to build some headers
dnl

AC_DEFUN([AC_KRB_PROG_PERL],
[AC_CHECK_PROGS(PERL, perl, perl)
if test "$PERL" = ""; then
  AC_MSG_ERROR([perl not found - Cannot build Heimdal without perl])
fi
])

AC_DEFUN([AC_KRB_PERL_MOD],
[
AC_MSG_CHECKING([for Perl5 module $1])
if ! $PERL -M$1 -e 'exit(0);' >/dev/null 2>&1; then
  AC_MSG_RESULT([no])
  AC_MSG_ERROR([perl module $1 not found - Cannot build Heimdal without perl module $1])
else
  AC_MSG_RESULT([yes])
fi
])
