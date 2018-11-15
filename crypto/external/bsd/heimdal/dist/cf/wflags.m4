dnl Id
dnl
dnl set WFLAGS

AC_DEFUN([rk_WFLAGS],[

AC_ARG_ENABLE(developer, 
	AS_HELP_STRING([--enable-developer], [enable developer warnings]))
if test "X$enable_developer" = Xyes; then
    dwflags="-Werror"
fi
AM_CONDITIONAL(DEVELOPER_MODE, test "X$enable_developer" = Xyes)

WFLAGS_NOUNUSED=""
if test -z "$WFLAGS" -a "$GCC" = "yes"; then
  # leave these out for now:
  #   -Wcast-align doesn't work well on alpha osf/1
  #   -Wmissing-prototypes -Wpointer-arith -Wbad-function-cast
  #   -Wmissing-declarations -Wnested-externs
  #   -Wstrict-overflow=5
  WFLAGS="ifelse($#, 0,-Wall, $1) $dwflags"

  #
  # WFLAGS_LITE can be appended to WFLAGS to turn off a host of warnings
  # that fail for various bits of older code in appl/.  Let's not use it
  # for the main libraries, though.
  WFLAGS_LITE="-Wno-extra -Wno-missing-field-initializers -Wno-strict-aliasing -Wno-shadow"
  # -Wno-unused-result (not supported on gcc-4.2)

fi
AC_SUBST(WFLAGS)dnl
AC_SUBST(WFLAGS_LITE)dnl
])
