dnl Id
dnl
dnl set WFLAGS

AC_DEFUN([rk_WFLAGS],[

AC_ARG_ENABLE(developer, 
	AS_HELP_STRING([--enable-developer], [enable developer warnings]))
if test "X$enable_developer" = Xyes; then
    dwflags="-Werror"
fi

WFLAGS_NOUNUSED=""
WFLAGS_NOIMPLICITINT=""
if test -z "$WFLAGS" -a "$GCC" = "yes"; then
  # -Wno-implicit-int for broken X11 headers
  # leave these out for now:
  #   -Wcast-align doesn't work well on alpha osf/1
  #   -Wmissing-prototypes -Wpointer-arith -Wbad-function-cast
  #   -Wmissing-declarations -Wnested-externs
  #   -Wstrict-overflow=5
  WFLAGS="ifelse($#, 0,-Wall, $1) $dwflags"
  WFLAGS_NOUNUSED="-Wno-unused"
  WFLAGS_NOIMPLICITINT="-Wno-implicit-int"
fi
AC_SUBST(WFLAGS)dnl
AC_SUBST(WFLAGS_NOUNUSED)dnl
AC_SUBST(WFLAGS_NOIMPLICITINT)dnl
])
