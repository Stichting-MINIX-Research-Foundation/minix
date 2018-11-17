dnl Id
dnl
dnl check for getpwuid_r, and if it's posix or not

AC_DEFUN([AC_CHECK_GETPWUID_R_POSIX],[
AC_FIND_FUNC_NO_LIBS(getpwuid_r,c_r)
if test "$ac_cv_func_getpwuid_r" = yes; then
	AC_CACHE_CHECK(if getpwuid_r is posix,ac_cv_func_getpwuid_r_posix,
	ac_libs="$LIBS"
	LIBS="$LIBS $LIB_getpwuid_r"
	AC_RUN_IFELSE([AC_LANG_SOURCE([[
#define _POSIX_PTHREAD_SEMANTICS
#include <pwd.h>
int main(int argc, char **argv)
{
	struct passwd pw, *pwd;
	return getpwuid_r(0, &pw, 0, 0, &pwd) < 0;
}
]])],[ac_cv_func_getpwuid_r_posix=yes],[ac_cv_func_getpwuid_r_posix=no],[:])
LIBS="$ac_libs")
	AC_CACHE_CHECK(if _POSIX_PTHREAD_SEMANTICS is needed,ac_cv_func_getpwuid_r_posix_def,
	ac_libs="$LIBS"
	LIBS="$LIBS $LIB_getpwuid_r"
	AC_RUN_IFELSE([AC_LANG_SOURCE([[
#include <pwd.h>
int main(int argc, char **argv)
{
	struct passwd pw, *pwd;
	return getpwuid_r(0, &pw, 0, 0, &pwd) < 0;
}
]])],[ac_cv_func_getpwuid_r_posix_def=no],[ac_cv_func_getpwuid_r_posix_def=yes],[:])
LIBS="$ac_libs")
if test "$ac_cv_func_getpwuid_r_posix" = yes; then
	AC_DEFINE(POSIX_GETPWUID_R, 1, [Define if getpwuid_r has POSIX flavour.])
fi
if test "$ac_cv_func_getpwuid_r_posix" = yes -a "$ac_cv_func_getpwuid_r_posix_def" = yes; then
	AC_DEFINE(_POSIX_PTHREAD_SEMANTICS, 1, [Define to get POSIX getpwuid_r in some systems.])
fi
fi
])
