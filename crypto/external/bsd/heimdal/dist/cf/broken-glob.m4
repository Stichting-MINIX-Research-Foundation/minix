dnl Id
dnl
dnl check for glob(3)
dnl
AC_DEFUN([AC_BROKEN_GLOB],[
AC_CACHE_CHECK(for working glob, ac_cv_func_glob_working,
ac_cv_func_glob_working=yes
AC_LINK_IFELSE([AC_LANG_PROGRAM([[
#include <stdio.h>
#include <glob.h>]],[[
glob(NULL, GLOB_BRACE|GLOB_NOCHECK|GLOB_QUOTE|GLOB_TILDE|
#ifdef GLOB_MAXPATH
GLOB_MAXPATH
#else
GLOB_LIMIT
#endif
,
NULL, NULL);
]])],[:],[ac_cv_func_glob_working=no]))

if test "$ac_cv_func_glob_working" = yes; then
	AC_DEFINE(HAVE_GLOB, 1, [define if you have a glob() that groks 
	GLOB_BRACE, GLOB_NOCHECK, GLOB_QUOTE, GLOB_TILDE, and GLOB_LIMIT])
fi
if test "$ac_cv_func_glob_working" = yes; then
AC_NEED_PROTO([#include <stdio.h>
#include <glob.h>],glob)
fi
])
