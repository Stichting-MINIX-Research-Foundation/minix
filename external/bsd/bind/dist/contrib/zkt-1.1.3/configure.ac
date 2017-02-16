#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.
#
#	@(#) configure.ac 
#
#	2008-06-27	initial setup
#	2008-06-29	add of BIND path checking
#	2008-06-30	add of arg checkings
#	2008-07-02	additional arg checkings
#	2008-07-04	check for getopt_long() added
#	2008-08-30	check for unsigned integer types
#	2008-10-01	if BIND_UTIL_PATH check failed, use config_zkt.h setting as last resort
#	2009-07-30	check for timegm() added
#	2009-12-02	the tr command in bind_version= didn't work well under solaris
#	2010-10-14	new option to specify BIND_UTIL_PATH on command line (thanks to Mans Nilsson)
#			No build in default BIND_UTIL_PATH used anymore
#

dnl AC_PREREQ(2.59)

### Package name and current version
AC_INIT(ZKT, 1.1.3, Holger Zuleger hznet.de)
dnl AC_REVISION(Revision: 1.397 )

### Files to test to check if src dir contains the package
AC_CONFIG_SRCDIR([zkt-signer.c])
AC_CONFIG_HEADER([config.h])


### Checks for programs.
AC_PROG_CC

### find out the path to BIND utils and version
AC_ARG_ENABLE([bind_util_path], AS_HELP_STRING(	[--enable-bind_util_path=PATH], [Define path to BIND utilities, default is path to dnssec-signzone]), [bind_util_path=$enableval])
if test -n "$bind_util_path"
then
	if test -x "$bind_util_path/dnssec-signzone"
	then
		AC_MSG_NOTICE([BIND utilities path successfully set to $bind_util_path.])
		SIGNZONE_PROG=$bind_util_path/dnssec-signzone	
	else
        	AC_MSG_ERROR([*** 'BIND utility not found in $bind_util_path, please use --enable-bind_util_path= to set it manually' ***])
	fi
else
	AC_PATH_PROG([SIGNZONE_PROG], dnssec-signzone)
	AC_MSG_NOTICE([BIND utility $SIGNZONE_PROG found])
	if test -n "$SIGNZONE_PROG" 
	then
		bind_util_path=`dirname "$SIGNZONE_PROG"`
		AC_MSG_NOTICE([BIND utilities path automatically set to $bind_util_path.])
	else
        	AC_MSG_ERROR([*** 'could not determine BIND utility path, please use --enable-bind_util_path= ' to set it manually ***])
	fi
fi
### By now, we have a path. We'll use it.  
# define BIND_UTIL_PATH in config.h.in
AC_DEFINE_UNQUOTED(BIND_UTIL_PATH, "$bind_util_path/", Path to BIND utilities)

# define BIND_VERSION in config.h.in
bind_version=`$SIGNZONE_PROG 2>&1 |  awk -F: '/^Version:/ { split ($2, v, "."); printf ("%2d%02d%02d\n", atoi (v[[1]]), atoi (v[[2]]), atoi (v[[3]])); };'`
AC_MSG_NOTICE([BIND_VERSION string set to $bind_version.])
AC_DEFINE_UNQUOTED(BIND_VERSION, $bind_version, BIND version as integer number without dots)
if test $bind_version -lt "90800" 
then
	AC_MSG_ERROR([*** 'This version of ZKT requires a BIND version greater 9.7' ***])
fi


AC_CHECK_TYPE(uint, unsigned int)
AC_CHECK_TYPE(ulong, unsigned long)
AC_CHECK_TYPE(ushort, unsigned short)
AC_CHECK_TYPE(uchar, unsigned char)

### define configure arguments
AC_ARG_ENABLE([color_mode], AS_HELP_STRING([--disable-color-mode], [zkt without colors]))
color_mode=1
AS_IF([test "$enable_color_mode" = "no"], [color_mode=0])

AC_ARG_WITH([curses],
    AS_HELP_STRING([--without-curses], [Ignore presence of curses and disable color mode]))

AS_IF([test "x$with_curses" != "xno"],
	[AC_CHECK_LIB([ncurses],[tgetent])],
	        [HAVE_LIB_NCURSES=0; color_mode=0])

AC_DEFINE_UNQUOTED(COLOR_MODE, $color_mode, zkt-ls with colors)


dnl printtimezone is a default-disabled feature
AC_ARG_ENABLE([printtimezone], AS_HELP_STRING( [--enable-print-timezone], [print out timezone]))
printtimezone=0
AS_IF([test "$enable_printtimezone" = "yes"], [printtimezone=1])
AC_DEFINE_UNQUOTED(PRINT_TIMEZONE, $printtimezone, print out timezone)

AC_ARG_ENABLE([printyear], AS_HELP_STRING( [--enable-print-age], [print age with year]))
printyear=0
AS_IF([test "$enable_printyear" = "yes"], [printyear=1])
AC_DEFINE_UNQUOTED(PRINT_AGE_WITH_YEAR, $printyear, print age with year)

AC_ARG_ENABLE([logprogname], AS_HELP_STRING( [--enable-log-progname], [log with progname]))
logprogname=0
AS_IF([test "$enable_logprogname" = "yes"], [logprogname=1])
AC_DEFINE_UNQUOTED(LOG_WITH_PROGNAME, $logprogname, log with progname)

dnl logtimestamp is a default-enabled feature
AC_ARG_ENABLE([logtimestamp], AS_HELP_STRING([--disable-log-timestamp], [do not log with timestamp]))
logtimestamp=1
AS_IF([test "$enable_logtimestamp" = "no"], [logtimestamp=0])
AC_DEFINE_UNQUOTED(LOG_WITH_TIMESTAMP, $logtimestamp, log with timestamp)

AC_ARG_ENABLE([loglevel], AS_HELP_STRING([--disable-log-level], [do not log with level]))
loglevel=1
AS_IF([test "$enable_loglevel" = "no"], [loglevel=0])
AC_DEFINE_UNQUOTED(LOG_WITH_LEVEL, $loglevel, log with level)

AC_ARG_ENABLE([ttl_in_keyfile], AS_HELP_STRING([--disable-ttl-in-keyfiles], [do not allow TTL values in keyfiles]))
ttl_in_keyfile=1
AS_IF([test "$enable_ttl_in_keyfile" = "no"], [ttl_in_keyfile=0])
AC_DEFINE_UNQUOTED(TTL_IN_KEYFILE_ALLOWED, $ttl_in_keyfile, TTL in keyfiles allowed)

configpath="/var/named"
AC_ARG_ENABLE([configpath], 
	AS_HELP_STRING( [--enable-configpath=PATH], [set path of config file (defaults to /var/named)]),
	[configpath=$enableval])
case "$configpath" in
yes)
	configpath="/var/named"
	;;
no)
	configpath=""
	;;
*)
	;;
esac
AC_DEFINE_UNQUOTED(CONFIG_PATH, "$configpath/", [set path of config file (defaults to /var/named)])

usetree=1
t=""
AC_ARG_ENABLE([tree], 
	AS_HELP_STRING( [--disable-tree], [use single linked list instead of binary tree data structure for dnssec-zkt]),
	[usetree=$enableval])
if test "$usetree" = no
then
	usetree=0
	t="S"
fi
AC_DEFINE_UNQUOTED(USE_TREE, $usetree, Use TREE data structure for dnssec-zkt)

AC_DEFINE_UNQUOTED(ZKT_VERSION, "$t$PACKAGE_VERSION", ZKT version string)
AC_DEFINE_UNQUOTED(ZKT_COPYRIGHT, "(c) Feb 2005 - Nov 2012 Holger Zuleger hznet.de", ZKT copyright string)

### Checks for libraries.

### Checks for header files.
AC_HEADER_DIRENT
AC_HEADER_STDC
AC_CHECK_HEADERS([fcntl.h netdb.h stdlib.h getopt.h string.h strings.h sys/socket.h sys/time.h sys/types.h syslog.h unistd.h utime.h term.h curses.h])


### Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T
AC_HEADER_TIME
AC_STRUCT_TM
AC_TYPE_UID_T


### Checks for library functions.
dnl AC_FUNC_MALLOC
AC_FUNC_CLOSEDIR_VOID
AC_FUNC_ERROR_AT_LINE
AC_FUNC_MKTIME
AC_FUNC_STAT
AC_FUNC_STRFTIME
AC_FUNC_UTIME_NULL
AC_FUNC_VPRINTF
# 2008-07-04 getopt_long added
# 2009-07-30 timegm added
AC_CHECK_FUNCS([getopt_long gettimeofday memset putenv socket strcasecmp strchr strdup strerror strncasecmp strrchr tzset utime getuid timegm])


AC_CONFIG_FILES([Makefile])
AC_OUTPUT
