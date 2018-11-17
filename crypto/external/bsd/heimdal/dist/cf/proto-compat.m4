dnl Id
dnl
dnl
dnl Check if the prototype of a function is compatible with another one
dnl

dnl AC_PROTO_COMPAT(includes, function, prototype)

AC_DEFUN([AC_PROTO_COMPAT], [
AC_CACHE_CHECK([if $2 is compatible with system prototype],
ac_cv_func_$2_proto_compat,
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[$1]],[[$3]])],
[eval "ac_cv_func_$2_proto_compat=yes"],
[eval "ac_cv_func_$2_proto_compat=no"]))
define([foo], translit($2, [a-z], [A-Z])[_PROTO_COMPATIBLE])
if test "$ac_cv_func_$2_proto_compat" = yes; then
	AC_DEFINE(foo, 1, [define if prototype of $2 is compatible with
	$3])
fi
undefine([foo])
])
