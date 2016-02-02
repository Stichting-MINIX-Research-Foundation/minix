# configure.ac
AC_INIT(top, 3.8beta1)

# AX_CHECK_VARIADIC_MACROS...
# -----
AC_DEFUN([AX_CHECK_VARIADIC_MACROS],
[AC_MSG_CHECKING([for variadic macros])
AC_COMPILE_IFELSE(AC_LANG_PROGRAM(
[#define a(x, ...) (x, __VA_ARGS__)], []),
	 [AS_VAR_SET(ax_cv_c99_variadic, yes)],
	 [AS_VAR_SET(ax_cv_c99_variadic, no)])
AC_COMPILE_IFELSE(AC_LANG_PROGRAM(
[#define a(x...) (x)], []),
	 [AS_VAR_SET(ax_cv_gnu_variadic, yes)],
	 [AS_VAR_SET(ax_cv_gnu_variadic, no)])
_result=""
if test "$ax_cv_c99_variadic" = "yes"; then
    _result=" c99"
    AC_DEFINE(HAVE_C99_VARIADIC_MACROS, 1, [Supports C99 style variadic macros])
fi
if test "$ax_cv_gnu_variadic" = "yes"; then
    _result="$_result gnu"
    AC_DEFINE(HAVE_GNU_VARIADIC_MACROS, 1, [Supports gnu style variadic macros])
fi
if test "x$_result" = x; then
    _result="no"
fi
AC_MSG_RESULT($_result)
])

# AC_CHECK_CFLAG...
# -----
AC_DEFUN([AC_CHECK_CFLAG],
[AC_MSG_CHECKING([whether compiler accepts $1])
AS_VAR_PUSHDEF([ac_Flag], [ac_cv_cflag_$1])dnl
_savedcflags=$CFLAGS
_savedwerror_flag=$ac_c_werror_flag
ac_c_werror_flag=yes
CFLAGS=$CFLAGS" $1"
AC_LINK_IFELSE([AC_LANG_PROGRAM([], [])], [AS_VAR_SET(ac_Flag, yes)],
				          [AS_VAR_SET(ac_Flag, no)])
AC_MSG_RESULT([AS_VAR_GET(ac_Flag)])
CFLAGS=$_savedcflags
ac_c_werror_flag=$_savedwerror_flag
AS_IF([test AS_VAR_GET(ac_Flag) = yes], [$2], [$3])[]dnl
AS_VAR_POPDEF([ac_Flag])dnl
])# AC_CHECK_CFLAG

echo "Configuring $PACKAGE_STRING"

AC_CONFIG_HEADER([config.h])
AC_CANONICAL_BUILD
AC_CANONICAL_HOST
AC_CANONICAL_TARGET

# options processing
AC_ARG_WITH(module, AC_HELP_STRING([--with-module=NAME],
		    [use the platform module NAME]),
		    [if test ! -f machine/m_$withval.c;
		     then AC_MSG_ERROR([No such module $withval]); fi])

AC_ARG_WITH(ext, AC_HELP_STRING([--with-ext=EXT],
    [use the extension EXT]),
    [if test -f ext/$withval.c; then
	AC_DEFINE(WITH_EXT, 1, [Include code that utilizes extensions])
	SRC="$SRC ext/$withval.c"
	OBJ="$OBJ $withval.o"
     else
        AC_MSG_ERROR([No such extension $withval])
     fi])

DEFAULT_TOPN=30
AC_ARG_WITH(default-topn, AC_HELP_STRING([--with-default-topn=N],
    [use N as the default for number of processes]),
    [if test x"$with_default_topn" = xall; then
	DEFAULT_TOPN="-1"
     elif test x`echo $with_default_topn | tr -d '[0-9+-]'` = x; then
	DEFAULT_TOPN=$with_default_topn
     fi])
AC_DEFINE_UNQUOTED(DEFAULT_TOPN, $DEFAULT_TOPN, [Default number of processes to display])
AC_SUBST(DEFAULT_TOPN)

NOMINAL_TOPN=40
AC_ARG_WITH(nominal-topn, AC_HELP_STRING([--with-nominal-topn=N],
    [use N as the default number of processes for non-terminals]),
    [if test x"$with_nominal_topn" = xall; then
	NOMINAL_TOPN="-1"
     elif test x`echo $with_nominal_topn | tr -d '[0-9+-]'` = x; then
	NOMINAL_TOPN=$with_nominal_topn
     fi])
AC_DEFINE_UNQUOTED(NOMINAL_TOPN, $NOMINAL_TOPN, [Default number of processes to display on non-terminals when topn is all])
AC_SUBST(NOMINAL_TOPN)

DEFAULT_DELAY=5
AC_ARG_WITH(default-delay, AC_HELP_STRING([--with-default-delay=SEC],
    [use a default delay of SEC seconds]),
    [if test x`echo $with_default_delay | tr -d '[0-9+-]'` = x; then
	DEFAULT_DELAY=$with_default_delay
     fi])
AC_DEFINE_UNQUOTED(DEFAULT_DELAY, $DEFAULT_DELAY, [Default delay])
AC_SUBST(DEFAULT_DELAY)

AC_ARG_ENABLE(debug, AC_HELP_STRING([--enable-debug],
    [enable support for debugging output]))
if test "x$enable_debug" = xyes; then
    AC_DEFINE(DEBUG, 1, [Support for debugging output])
fi

ENABLE_KILL=0
AC_ARG_ENABLE(kill, AC_HELP_STRING([--disable-kill],
    [disable kill and renice commands]))
if test x$enable_kill != xno; then
    AC_DEFINE(ENABLE_KILL, 1, [Enable kill and renice])
    ENABLE_KILL=1
fi
AC_SUBST(ENABLE_KILL)


AC_ARG_ENABLE(color, AC_HELP_STRING([--disable-color],
    [disable the use of color]))
AC_ARG_ENABLE(colour, AC_HELP_STRING([--disable-colour],
    [synonym for --disable-color]))
if test x$enable_color != xno -a x$enable_colour != xno; then
    AC_DEFINE(ENABLE_COLOR, 1, [Enable color])
fi

AC_ARG_ENABLE(dualarch, AC_HELP_STRING([--enable-dualarch],
    [enable or disable a dual architecture (32-bit and 64-bit) compile]))

# check for needed programs
AC_CHECK_PROGS(MAKE, make)
AC_PROG_CC
if test "$ac_cv_c_compiler_gnu" = "yes"; then
    ax_cv_c_compiler_vendor="gnu"
else
    AX_COMPILER_VENDOR
fi
AC_PROG_AWK
AC_PROG_INSTALL
AC_PATH_PROGS(ISAINFO, isainfo)
AC_PATH_PROGS(ISAEXEC, isaexec, , [$PATH:/usr/lib:/lib])
AC_PATH_PROGS(UNAME, uname)
AC_SUBST(ISAEXEC)

# system checks require uname
if test "$UNAME"; then
    # we make the version number available as a C preprocessor definition
    AC_MSG_CHECKING(OS revision number)
    osrev=`$UNAME -r | tr -cd ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789`

    if test "$osrev" != "unknown" ; then
        AC_DEFINE_UNQUOTED(OSREV, $osrev, [Define the OS revision.])
	osmajor=`$UNAME -r | sed 's/^\([[0-9]]*\).*$/\1/'`
	if test -n "$osmajor"; then
	    AC_DEFINE_UNQUOTED(OSMAJOR, $osmajor, [Define the major OS revision number.])
	fi
    else
        AC_DEFINE(OSREV, "")
    fi
    AC_MSG_RESULT($osrev)

    # we make the non-canonicalized hardware type available
    AC_MSG_CHECKING(hardware platform)
    UNAME_HARDWARE=`$UNAME -m`
    if test "$UNAME_HARDWARE" != "unknown"; then
        AC_DEFINE_UNQUOTED(UNAME_HARDWARE, "$UNAME_HARDWARE", [Define the system hardware platform])
    fi
    AC_MSG_RESULT($UNAME_HARDWARE)
fi

# checks for libraries
AC_CHECK_LIB(elf, elf32_getphdr)
AC_CHECK_LIB(kstat, kstat_open)
AC_CHECK_LIB(kvm, kvm_open)
# -lmld -lmach
AC_CHECK_LIB(mach, vm_statistics)
AC_SEARCH_LIBS(tgetent, termcap curses ncurses)
AC_CHECK_LIB(m, exp)

# check for libraries required by extension
extlibs=""
if test -n "$with_ext" -a -f "${srcdir}/ext/$with_ext.libs"; then
    AC_MSG_CHECKING(for libraries needed by extensions)
    for lib in `cat "${srcdir}/ext/$with_ext.libs"`
    do
	saveLIBS=$LIBS
	LIBS="$LIBS -l$lib"
	AC_TRY_LINK(, [exit(0);], [extlibs="$extlibs -l$lib"], )
	LIBS=$saveLIBS
    done
    AC_MSG_RESULT($extlibs)
    LIBS="$LIBS$extlibs"
fi

# checks for header files
AC_HEADER_STDC
AC_CHECK_HEADERS([curses.h getopt.h limits.h math.h stdarg.h sysexits.h termcap.h unistd.h sys/resource.h sys/time.h sys/utsname.h])
AC_CHECK_HEADERS([term.h],,,
[#if HAVE_CURSES_H
#include <curses.h>
#endif
])
AC_HEADER_TIME
AC_MSG_CHECKING(for a good signal.h)
SIGNAL_H="no"
for f in /usr/include/signal.h /usr/include/sys/signal.h /usr/include/sys/iso/signal_iso.h /usr/include/bits/signum.h; do
    if grep SIGKILL $f >/dev/null 2>&1; then
       SIGNAL_H=$f
       break
    fi
done
AC_MSG_RESULT($SIGNAL_H)
if test "$SIGNAL_H" = "no"; then
    SIGNAL_H="/dev/null"
fi
AC_SUBST(SIGNAL_H)

# checks for typedefs, structures, and compiler characteristics.
AX_CHECK_VARIADIC_MACROS
AC_CHECK_DECLS([sys_errlist])
AC_CHECK_DECLS([sys_signame],,,
[#include <signal.h>
/* NetBSD declares sys_siglist in unistd.h.  */
#if HAVE_UNISTD_H
# include <unistd.h>
#endif
])
AC_CHECK_DECLS([tputs, tgoto, tgetent, tgetflag, tgetnum, tgetstr],,,
[#if HAVE_CURSES_H && HAVE_TERM_H
#include <curses.h>
#include <term.h>
#else
#if HAVE_TERMCAP_H
#include <termcap.h>
#else
#if HAVE_CURSES_H
#include <curses.h>
#endif
#endif
#endif
])

# The third argument to tputs is a putc-like function that takes an 
# argument.  On most systems that argument is an int, but on some it
# is a char.  Determine which.
AC_MSG_CHECKING([argument type of tputs putc function])
_savedwerror_flag=$ac_c_werror_flag
ac_c_werror_flag=yes
AC_COMPILE_IFELSE(
[AC_LANG_PROGRAM([#ifdef HAVE_TERMCAP_H
#include <termcap.h>
#endif
#ifdef HAVE_CURSES_H
#include <curses.h>
#endif
#ifdef HAVE_TERM_H
#include <term.h>
#endif
int f(char i) { }],
[tputs("a", 1, f);])],
	    [ac_cv_type_tputs_putc="char"],
	    [ac_cv_type_tputs_putc="int"])
AC_MSG_RESULT($ac_cv_type_tputs_putc)
AC_DEFINE_UNQUOTED(TPUTS_PUTC_ARGTYPE, $ac_cv_type_tputs_putc,
			[Define as the type for the argument to the
putc function of tputs ('int' or 'char')])
ac_c_werror_flag=$_savedwerror_flag

# Determine presence of needed types
AC_TYPE_SIGNAL
AC_CHECK_TYPES([id_t, lwpid_t, pid_t, time_t, uid_t])

# Checks for library functions.
AC_CHECK_FUNCS([getopt getopt_long gettimeofday memcpy setbuffer setpriority setvbuf strcasecmp strchr strerror snprintf sighold sigrelse sigaction sigprocmask sysconf uname vsnprintf])

# this is needed in the man page
if test "x$ac_cv_func_getopt_long" = "xyes"; then
    HAVE_GETOPT_LONG=1
else
    HAVE_GETOPT_LONG=0
fi
AC_SUBST(HAVE_GETOPT_LONG)

# if we dont have snprintf/vsnprint then we need to compile the alternate
if test "x$ac_cv_func_snprintf" != "xyes" -o "x$ac_cv_func_vsnprintf" != "xyes"; then
    SRC="$SRC ap_snprintf.c"
    OBJ="$OBJ ap_snprintf.o"
fi


# determine correct user, group, and mode
# these can be overridden later if need be
AC_MSG_CHECKING(for correct ls options)
lslong="ls -l"
if test `$lslong -d . | wc -w` -lt 9; then
   lslong="ls -lg"
fi
AC_MSG_RESULT($lslong)


# determine correct module
AC_MSG_CHECKING(for a platform module)
if test "$with_module"; then
    MODULE=$with_module
else
    case $target_os in
	aix4.2*)	MODULE=aix43;;
	aix4.3*)	MODULE=aix43;;
	aix5*)		MODULE=aix5;;
	dec-osf*)	MODULE=decosf1;;
	osf1*)		MODULE=decosf1;;
	osf4*)		MODULE=decosf1;;
	osf5*)		MODULE=decosf1;;
        freebsd*)	MODULE=freebsd; USE_KMEM=1; USE_FPH=1;;
	hpux7*)		MODULE=hpux7;;
	hpux8*)		MODULE=hpux8;;
	hpux9*)		MODULE=hpux9;;
	hpux10*)	MODULE=hpux10;;
	hpux11*)	MODULE=hpux10;;
	irix5*)		MODULE=irix5;;
	irix6*)		MODULE=irixsgi;;
	linux*)		MODULE=linux; USE_FPH=1; SET_MODE=755;;
        netbsd*)	MODULE=netbsd; SET_MODE=755;;
	solaris2*)	MODULE=sunos5; USE_FPH=1; SET_MODE=755;;
	sunos4*)	MODULE=sunos4;;
	sysv4*)		MODULE=svr4;;
	sysv5*)		MODULE=svr5;;
	darwin*)
	    echo "macosx"
	    echo "The macosx module is untested.  Use at your own risk."
	    echo "If you really want to use this module, please run configure as follows:"
	    echo "    ./configure --with-module=macosx"
	    AC_MSG_ERROR([macosx module unsupported]);;
	*)  echo "none"
	    echo "Configure doesn't recognize this system and doesn't know"
	    echo "what module to assign to it.  Help the cause and run the"
	    echo "following command to let the maintainers know about this"
	    echo "deficiency!  Thanks.  Just cut and paste the following:"
echo "uname -a | mail -s $target_os bill@lefebvre.org"
	    echo ""
	    AC_MSG_ERROR([System type $target_os unrecognized])
    esac
fi
AC_MSG_RESULT($MODULE)
SRC="$SRC machine/m_$MODULE.c"
OBJ="$OBJ m_$MODULE.o"
CLEAN_EXTRA=""
AC_SUBST(SRC)
AC_SUBST(OBJ)
AC_SUBST(CLEAN_EXTRA)
AC_DEFINE_UNQUOTED(MODULE, "$MODULE", [Platform module])

FIRST_RULE=/dev/null
INSTALL_RULE=config.default.makeinstall

# extra things that need to be done for certain systems
# also handle setup for 64-bit detection
bits="default"
case $MODULE in
    aix5)
	AC_CHECK_LIB(perfstat, perfstat_cpu_total)
	if test -f /usr/sbin/bootinfo; then
	    bits="`/usr/sbin/bootinfo -K`"
	    extra_flag="-q64"
	fi
    ;;
    svr5)
        # -lmas
        AC_CHECK_LIB(mas, mas_open)
    ;;
    sunos5)
        if test "$ISAINFO"; then
	    bits="`$ISAINFO -b`"
	    if test "$target_cpu" = "sparc"; then
		extra_flag="-xarch=v9"
	    else
		extra_flag="-xarch=amd64"
	    fi
	fi
    ;;
esac

# USE_FPH means the module has format_process_header
if test -n "$USE_FPH"; then
    AC_DEFINE(HAVE_FORMAT_PROCESS_HEADER, 1, [Platform module])
fi

# if we are 64-bit, try to turn on the appropriate flags
AC_MSG_CHECKING(address space size)
ARCHFLAG=""
if test "$bits" = "64"; then
    AC_MSG_RESULT(64)
    if test "$ax_cv_c_compiler_vendor" = "gnu"; then
        extra_flag="-m64"
    fi
#   Make sure our compiler accepts the flag we want to use
    AC_CHECK_CFLAG($extra_flag, [ARCHFLAG="$extra_flag"],
				[enable_dualarch="no"])
else
    AC_MSG_RESULT($bits)
fi
AC_SUBST(ARCHFLAG)

# Dual architecture handling:  for now this is only enabled on Solaris.
# Config options can explicitly enable or disable dualarch.  Otherwise,
# dualarch is only enabled when we are on a 64-bit system.
if test "$MODULE" = "sunos5"; then
    AC_MSG_CHECKING(for dual architecture compilation)
    if test "x$enable_dualarch" = x; then
#       we must make the determination implicitly
	if test "$bits" = "64"; then
	    enable_dualarch="yes"
	else
	    enable_dualarch="no"
	fi
    fi
    if test "x$enable_dualarch" = "xyes"; then
        AC_MSG_RESULT(yes)
        if test "$target_cpu" = "sparc"; then
	    FIRST_RULE="config.sparcv9.make"
	    INSTALL_RULE="config.sparcv9.makeinstall"
	    CLEAN_EXTRA="$CLEAN_EXTRA sparcv7/* sparcv9/*"
	    mkdir -p sparcv7 sparcv9
	else
	    FIRST_RULE="config.amd64.make"
	    INSTALL_RULE="config.amd64.makeinstall"
	    CLEAN_EXTRA="$CLEAN_EXTRA i386/* amd64/*"
	    mkdir -p i386 amd64
	fi
    else
        AC_MSG_RESULT(no)
    fi
fi

if test x$enable_dualarch = xyes; then
    AC_DEFINE(ENABLE_DUALARCH, 1, [Enable dual architecture])
fi

AC_SUBST_FILE(FIRST_RULE)
AC_SUBST_FILE(INSTALL_RULE)

AC_MSG_CHECKING(for installation settings)
# calculate appropriate settings
OWNER=""
GROUP=""
MODE=""
if test ! -n "$USE_KMEM" -a -d /proc; then
#   make sure we are installed so that we can read /proc
    rm -f conftest.txt
    if test -r /proc/0/psinfo; then
#       system uses solaris-style /proc
	$lslong /proc/0/psinfo >conftest.txt
    elif test -r /proc/1/stat; then
#       linux-style /proc?
	$lslong /proc/1/stat >conftest.txt
    else
	echo "-r--r--r-- 1 bin bin 32 Jan 1 12:00 /foo" >conftest.txt
    fi

#   set permissions so that we can read stuff in /proc
    if grep '^.......r..' conftest.txt >/dev/null; then
#	world readable
	MODE=755
    elif grep '^....r.....' conftest.txt >/dev/null; then
#	group readable
	MODE=2711
	GROUP=`awk ' { print $4 }'`
    else
#	probably only readable by root
	MODE=4711
	OWNER=`awk ' { print $3 }'`
    fi

elif test -c /dev/kmem; then
    $lslong -L /dev/kmem >conftest.txt
    if grep '^....r..r..' conftest.txt >/dev/null; then
        MODE=755
    elif grep '^....r..-..' conftest.txt >/dev/null; then
	MODE=2755
	GROUP=`$AWK ' { print $4 }' conftest.txt`
    fi
else
    MODE=755
fi
rm -f conftest.txt
# let module settings override what we calculated
OWNER=${SET_OWNER:-$OWNER}
GROUP=${SET_GROUP:-$GROUP}
MODE=${SET_MODE:-$MODE}

# set only those things that require it
result=""
INSTALL_OPTS_PROG=""
if test x$OWNER != x; then
    result="${result}owner=$OWNER, "
    INSTALL_OPTS_PROG="$INSTALL_OPTS_PROG -o $OWNER"
fi
if test x$GROUP != x; then
    result="${result}group=$GROUP, "
    INSTALL_OPTS_PROG="$INSTALL_OPTS_PROG -g $GROUP"
fi
result="${result}mode=$MODE"
INSTALL_OPTS_PROG="$INSTALL_OPTS_PROG -m $MODE"

AC_MSG_RESULT($result)

# add extra cflags if the compiler accepts them
AX_CFLAGS_WARN_ALL
MODULE_CFLAGS=""
if test "$ax_cv_c_compiler_vendor" = "gnu"; then
    AX_CFLAGS_GCC_OPTION([-fno-strict-aliasing], [MODULE_CFLAGS])
    if test "$target_cpu" = "alpha"; then
	AX_CFLAGS_GCC_OPTION([-mfp-trap-mode=sui -mtrap-precision=i])
    fi
fi

# Define man page supplement
MAN_SUPPLEMENT=machine/m_$MODULE.man
AC_SUBST_FILE(MAN_SUPPLEMENT)

# Extra things we want substituted
AC_SUBST(MODULE)
AC_SUBST(MODULE_CFLAGS)
AC_SUBST(INSTALL_OPTS_PROG)

# wrapup

AC_CONFIG_FILES(Makefile top.1)
AC_OUTPUT
