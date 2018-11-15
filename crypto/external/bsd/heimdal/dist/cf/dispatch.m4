
AC_DEFUN([rk_LIBDISPATCH],[

AC_CHECK_PROGS(GCD_MIG, mig, no)

if test "$GCD_MIG" != no; then
  AC_CHECK_HEADERS([dispatch/dispatch.h])
  AC_FIND_FUNC_NO_LIBS(dispatch_async_f, dispatch,
  [#ifdef HAVE_DISPATCH_DISPATCH_H
  #include <dispatch/dispatch.h>
  #endif],[0,0,0])

  if test "$ac_cv_func_dispatch_async_f" = yes -a "$GCD_MIG" != no; then
      AC_DEFINE([HAVE_GCD], 1, [Define if os support gcd.])
      libdispatch=yes
  else
      libdispatch=no
  fi

fi
AM_CONDITIONAL(have_gcd, test "$libdispatch" = yes -a "$GCD_MIG" != no)

])
