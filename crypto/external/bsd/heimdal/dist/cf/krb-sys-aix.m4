dnl Id
dnl
dnl
dnl AIX have a very different syscall convention
dnl
AC_DEFUN([AC_KRB_SYS_AIX], [
AC_MSG_CHECKING(for AIX)
AC_CACHE_VAL(krb_cv_sys_aix,
AC_EGREP_CPP(yes, 
[#ifdef _AIX
	yes
#endif 
], krb_cv_sys_aix=yes, krb_cv_sys_aix=no) )
AC_MSG_RESULT($krb_cv_sys_aix)
])
