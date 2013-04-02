# $Id: configure.ac,v 1.77 2006/05/13 11:09:33 dtucker Exp $
#
# Copyright (c) 2004, 2005 Darren Tucker.
# Parts based on configure.ac from Portable OpenSSH:
# Copyright (c) 1999-2004 Damien Miller
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

AC_INIT(OpenNTPD, Portable)
AC_CONFIG_SRCDIR([ntpd.c])
AC_CONFIG_HEADER(config.h)

AC_PROG_CC
AC_PROG_RANLIB
AC_PROG_INSTALL
AC_PATH_PROG(AR, ar)
AC_PROG_AWK
AC_PROG_YACC

if test -z "$LD" ; then
	LD=$CC
fi
AC_SUBST(LD)

for i in CC LD AR RANLIB INSTALL AWK
do
	prog=`eval echo '$'$i`
	if test -z "$prog"; then
		AC_MSG_ERROR([Required program $i not found])
	fi
done

AC_C_INLINE
if test "$GCC" = "yes" || test "$GCC" = "egcs"; then
	CFLAGS="$CFLAGS -Wall"
	CFLAGS="$CFLAGS -Wstrict-prototypes -Wmissing-prototypes"
	CFLAGS="$CFLAGS -Wmissing-declarations"
	CFLAGS="$CFLAGS -Wshadow -Wpointer-arith -Wcast-qual"
	CFLAGS="$CFLAGS -Wsign-compare"
fi

# Check for some target-specific stuff
AC_CANONICAL_HOST
case "$host" in
*-*-aix*)
	case "$host" in
	*-*-aix4*)
		AC_DEFINE(BROKEN_GETADDRINFO, [], [Broken getaddrinfo])
		;;
	esac
	AC_DEFINE(SETEUID_BREAKS_SETUID,[],[setuid after seteuid doesn't work])
	AC_DEFINE(BROKEN_SETREUID,[], [Broken setreuid])
	AC_DEFINE(BROKEN_SETREGID,[], [Broken setregid])
	# define these to pick up most of the prototypes
	CFLAGS="$CFLAGS -D_ALL_SOURCE -D_GNU_SOURCE -D_POSIX_C_SOURCE=199506L"
	;;
*-*-darwin*)
	AC_DEFINE(SETEUID_BREAKS_SETUID,[],[setuid after seteuid doesn't work])
	AC_DEFINE(BROKEN_SETREUID,[], [Broken setreuid])
	AC_DEFINE(BROKEN_SETREGID,[], [Broken setregid])
	;;
*-*-irix*)
	AC_DEFINE(SETEUID_BREAKS_SETUID,[],[setuid after seteuid doesn't work])
	AC_DEFINE(BROKEN_SETREUID,[], [Broken setreuid])
	AC_DEFINE(BROKEN_SETREGID,[], [Broken setregid])
	AC_CHECK_PROGS(have_sysconf, sysconf, no)
	if test "$have_sysconf" != "no"; then
		AC_MSG_CHECKING([for value of IOV_MAX])
		iov_max=`sysconf IOV_MAX`
		if test $? -eq 0; then
			AC_MSG_RESULT([$iov_max])
			AC_DEFINE_UNQUOTED(DEF_IOV_MAX, $iov_max,
			    [retrived via sysconf])
		else
			AC_MSG_RESULT(no)
		fi
	fi
	;;
*-*-linux*)
	AC_DEFINE(__need_IOV_MAX, [],
	    [Needed to get IOV_MAX from stdio.h on Linux])
	AC_DEFINE(USE_SIOCGIFCONF, [],
	    [Use SIOCGIFCONF ioctl if we do not have getifaddrs()])

	;;
*-*-qnx*)
	AC_DEFINE(SETEUID_BREAKS_SETUID,[],[setuid after seteuid doesn't work])
	AC_DEFINE(BROKEN_SETREUID,[], [Broken setreuid])
	AC_DEFINE(BROKEN_SETREGID,[], [Broken setregid])
	AC_DEFINE(SOCKLEN_T_IS_SIZE_T, [], [arg 3 of socket is size_t])
	AC_DEFINE_UNQUOTED(DEF_IOV_MAX, 1024, [statically set by configure])
	;;
+*-*-*UnixWare*)
	AC_DEFINE(SETEUID_BREAKS_SETUID,[],[setuid after seteuid doesn't work])
	AC_DEFINE(BROKEN_SETREUID,[], [Broken setreuid])
	AC_DEFINE(BROKEN_SETREGID,[], [Broken setregid])
	AC_DEFINE(BROKEN_SETRESUID,[], [Broken setresuid])
	AC_DEFINE(BROKEN_SETRESGID,[], [Broken setresgid])
	AC_DEFINE_UNQUOTED(DEF_IOV_MAX, 16, [statically set by configure])
	;;
esac

AC_CHECK_HEADERS( \
	arpa/inet.h \
	arpa/nameser.h \
	ctype.h \
	err.h \
	ifaddrs.h \
	netdb.h \
	netinet/in_systm.h \
	netinet/ip.h \
	paths.h \
	poll.h \
	stdarg.h \
	sys/bitypes.h \
	sys/fcntl.h \
	sys/queue.h \
	sys/select.h \
	sys/socket.h \
	sys/sockio.h \
	sys/time.h \
	sys/timers.h \
	sys/timex.h \
	sys/types.h \
	syslog.h \
)

AC_CHECK_DECLS(asprintf, ,
	[saved_CFLAGS="$CFLAGS"
	 CFLAGS="$CFLAGS -D_GNU_SOURCE"
	 unset ac_cv_have_decl_asprintf
	 AC_CHECK_DECLS(asprintf, , CFLAGS="$saved_CFLAGS")
	]
)

AC_CHECK_FUNCS(socketpair, ,
	[saved_LDFLAGS="$LDFLAGS"
	 LDFLAGS="$LDFLAGS -lsocket -lnsl"
	 unset ac_cv_func_socketpair
	 AC_CHECK_FUNCS(socketpair, , LDFLAGS="$saved_LDFLAGS")
	]
)

AC_CHECK_FUNCS( \
	adjtime \
	adjtimex \
	arc4random \
	asprintf \
	bzero \
	clock_getres \
	daemon \
	errx \
	getifaddrs \
	inet_pton \
	poll select \
	setgroups \
	setproctitle \
	snprintf \
	strlcpy \
	strsignal \
	verrx \
	vsnprintf \
)

dnl UnixWare vsyslog test is not accurate so we skip it
case "$host" in
*-*-*UnixWare*)
	;;
*)
	AC_CHECK_FUNCS(vsyslog)
	;;
esac

dnl Check for uidswap functions
AC_CHECK_FUNCS(setuid setgid seteuid setegid setreuid setregid)

AC_CHECK_FUNCS(setresuid, [
	dnl Some platorms have setresuid that isn't implemented, test for this
	AC_MSG_CHECKING(if setresuid seems to work)
	AC_RUN_IFELSE(
		[AC_LANG_SOURCE([[
#include <stdlib.h>
#include <errno.h>
int main(){errno=0; setresuid(0,0,0); if (errno==ENOSYS) exit(1); else exit(0);}
		]])],
		[AC_MSG_RESULT(yes)],
		[AC_DEFINE(BROKEN_SETRESUID, 1, [ENOSYS: Not implemented])
		 AC_MSG_RESULT(not implemented)],
		[AC_MSG_WARN([cross compiling: not checking setresuid])]
	)
])

AC_CHECK_FUNCS(setresgid, [
	dnl Some platorms have setresgid that isn't implemented, test for this
	AC_MSG_CHECKING(if setresgid seems to work)
	AC_RUN_IFELSE(
		[AC_LANG_SOURCE([[
#include <stdlib.h>
#include <errno.h>
int main(){errno=0; setresgid(0,0,0); if (errno==ENOSYS) exit(1); else exit(0);}
		]])],
		[AC_MSG_RESULT(yes)],
		[AC_DEFINE(BROKEN_SETRESGID, 1, [ENOSYS: Not implemented])
		 AC_MSG_RESULT(not implemented)],
		[AC_MSG_WARN([cross compiling: not checking setresuid])]
	)
])

dnl Check for getaddrinfo and friends.
AC_CHECK_FUNCS(getaddrinfo getnameinfo freeaddrinfo)

dnl IRIX has a const char return value for gai_strerror()
AC_CHECK_FUNCS(gai_strerror,[
	AC_DEFINE(HAVE_GAI_STRERROR)
	AC_TRY_COMPILE([
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

const char *gai_strerror(int);],[
char *str;

str = gai_strerror(0);],[
		AC_DEFINE(HAVE_CONST_GAI_STRERROR_PROTO, 1,
		[Define if gai_strerror() returns const char *])])])

AC_CACHE_CHECK([for struct sockaddr_storage], ac_cv_have_struct_sockaddr_storage, [
	AC_TRY_COMPILE(
		[
#include <sys/types.h>
#include <sys/socket.h>
		],
		[ struct sockaddr_storage s; ],
		[ ac_cv_have_struct_sockaddr_storage="yes" ],
		[ ac_cv_have_struct_sockaddr_storage="no" ]
	)
])
if test "x$ac_cv_have_struct_sockaddr_storage" = "xyes" ; then
	AC_DEFINE(HAVE_STRUCT_SOCKADDR_STORAGE, [], [Have struct sockaddr_storage])
fi

AC_CACHE_CHECK([for struct sockaddr_in6], ac_cv_have_struct_sockaddr_in6, [
	AC_TRY_COMPILE(
		[
#include <sys/types.h>
#include <netinet/in.h>
		],
		[ struct sockaddr_in6 s; s.sin6_family = 0; ],
		[ ac_cv_have_struct_sockaddr_in6="yes" ],
		[ ac_cv_have_struct_sockaddr_in6="no" ]
	)
])
if test "x$ac_cv_have_struct_sockaddr_in6" = "xyes" ; then
	AC_DEFINE(HAVE_STRUCT_SOCKADDR_IN6, [], [Have struct sockaddr_in6])
fi

AC_CACHE_CHECK([for struct in6_addr], ac_cv_have_struct_in6_addr, [
	AC_TRY_COMPILE(
		[
#include <sys/types.h>
#include <netinet/in.h>
		],
		[ struct in6_addr s; s.s6_addr[0] = 0; ],
		[ ac_cv_have_struct_in6_addr="yes" ],
		[ ac_cv_have_struct_in6_addr="no" ]
	)
])
if test "x$ac_cv_have_struct_in6_addr" = "xyes" ; then
	AC_DEFINE(HAVE_STRUCT_IN6_ADDR, [], [Have struct in6_addr])
fi

AC_CACHE_CHECK([for struct addrinfo], ac_cv_have_struct_addrinfo, [
	AC_TRY_COMPILE(
		[
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
		],
		[ struct addrinfo s; s.ai_flags = AI_PASSIVE; ],
		[ ac_cv_have_struct_addrinfo="yes" ],
		[ ac_cv_have_struct_addrinfo="no" ]
	)
])
if test "x$ac_cv_have_struct_addrinfo" = "xyes" ; then
	AC_DEFINE(HAVE_STRUCT_ADDRINFO, [], [Have struct addrinfo])
fi

AC_CHECK_MEMBER(struct sockaddr.sa_len,
	AC_DEFINE(HAVE_SOCKADDR_SA_LEN, [], [Have sockaddr.sa_len]), ,
	[ #include <netdb.h>
	  #include <netinet/in.h>
	  #include <sys/socket.h> ]
)

AC_SEARCH_LIBS(res_init, resolv)
AC_SEARCH_LIBS(res_9_init, resolv)

AC_CHECK_MEMBERS([struct sockaddr.sa_len,  struct sockaddr_in.sin_len,
	struct sockaddr_in6.sin6_len, struct sockaddr_in6.sin6_scope_id,
	struct sockaddr_storage.ss_family,
	struct sockaddr_storage.__ss_family], , ,
	[#include <sys/types.h>
	 #include <sys/socket.h>
	 #include <netinet/in.h>
	 #include <netdb.h>]
)

AC_CACHE_CHECK([if libc defines __progname], ac_cv_libc_defines___progname, [
	AC_TRY_LINK([],
		[ extern char *__progname; printf("%s", __progname); ],
		[ ac_cv_libc_defines___progname="yes" ],
		[ ac_cv_libc_defines___progname="no" ]
	)
])
if test "x$ac_cv_libc_defines___progname" = "xyes" ; then
	AC_DEFINE(HAVE___PROGNAME, [], [libc defines __progname])
fi

AC_CACHE_CHECK([if libc defines in6addr_any], ac_cv_libc_defines_in6addr_any, [
	AC_TRY_LINK(
		[ #include <sys/types.h>
		  #include <sys/socket.h>
		  #include <netinet/in.h> ]
		[  struct sockaddr_in6 = in6addr_any; ],
		[ ac_cv_libc_defines_in6addr_any="yes" ],
		[ ac_cv_libc_defines_in6addr_any="no" ]
	)
])
if test "x$ac_cv_libc_defines_in6addr_any" = "xyes" ; then
	AC_DEFINE(HAVE_IN6ADDR_ANY, [], [libc defines in6addr_any])
fi

dnl Does not support catman yet
mansubdir=man
AC_SUBST(mansubdir)

# Checks for data types
AC_CHECK_SIZEOF(char, 1)
AC_CHECK_SIZEOF(short int, 2)
AC_CHECK_SIZEOF(int, 4)
AC_CHECK_SIZEOF(long int, 4)
AC_CHECK_SIZEOF(long long int, 8)

# Sanity check long long for some platforms (AIX)
if test "x$ac_cv_sizeof_long_long_int" = "x4" ; then
	ac_cv_sizeof_long_long_int=0
fi

if test "x$ac_cv_sizeof_long_long_int" = "x0" -o -z "$ac_cv_sizeof_long_long_int"; then
	AC_DEFINE(NO_LONG_LONG, 1, [no "long long" support])
fi

# More checks for data types
AC_CACHE_CHECK([for u_int type], ac_cv_have_u_int, [
	AC_TRY_COMPILE(
		[ #include <sys/types.h> ],
		[ u_int a; a = 1;],
		[ ac_cv_have_u_int="yes" ],
		[ ac_cv_have_u_int="no" ]
	)
])
if test "x$ac_cv_have_u_int" = "xyes" ; then
	AC_DEFINE(HAVE_U_INT, [], [Have u_int])
	have_u_int=1
fi

AC_CACHE_CHECK([for intXX_t types], ac_cv_have_intxx_t, [
	AC_TRY_COMPILE(
		[ #include <sys/types.h> ],
		[ int8_t a; int16_t b; int32_t c; a = b = c = 1;],
		[ ac_cv_have_intxx_t="yes" ],
		[ ac_cv_have_intxx_t="no" ]
	)
])
if test "x$ac_cv_have_intxx_t" = "xyes" ; then
	AC_DEFINE(HAVE_INTXX_T, [], [Have intXX_t types])
	have_intxx_t=1
fi

if (test -z "$have_intxx_t" && \
	   test "x$ac_cv_header_stdint_h" = "xyes")
then
    AC_MSG_CHECKING([for intXX_t types in stdint.h])
	AC_TRY_COMPILE(
		[ #include <stdint.h> ],
		[ int8_t a; int16_t b; int32_t c; a = b = c = 1;],
		[
			AC_DEFINE(HAVE_INTXX_T, [], [Have intXX_t types])
			AC_MSG_RESULT(yes)
		],
		[ AC_MSG_RESULT(no) ]
	)
fi

AC_CACHE_CHECK([for int64_t type], ac_cv_have_int64_t, [
	AC_TRY_COMPILE(
		[
#include <sys/types.h>
#ifdef HAVE_STDINT_H
# include <stdint.h>
#endif
#include <sys/socket.h>
#ifdef HAVE_SYS_BITYPES_H
# include <sys/bitypes.h>
#endif
		],
		[ int64_t a; a = 1;],
		[ ac_cv_have_int64_t="yes" ],
		[ ac_cv_have_int64_t="no" ]
	)
])
if test "x$ac_cv_have_int64_t" = "xyes" ; then
	AC_DEFINE(HAVE_INT64_T, [], [Have int64_t type])
fi

AC_CACHE_CHECK([for u_intXX_t types], ac_cv_have_u_intxx_t, [
	AC_TRY_COMPILE(
		[ #include <sys/types.h> ],
		[ u_int8_t a; u_int16_t b; u_int32_t c; a = b = c = 1;],
		[ ac_cv_have_u_intxx_t="yes" ],
		[ ac_cv_have_u_intxx_t="no" ]
	)
])
if test "x$ac_cv_have_u_intxx_t" = "xyes" ; then
	AC_DEFINE(HAVE_U_INTXX_T, [], [Have u_intXX_t types])
	have_u_intxx_t=1
fi

if test -z "$have_u_intxx_t" ; then
    AC_MSG_CHECKING([for u_intXX_t types in sys/socket.h])
	AC_TRY_COMPILE(
		[ #include <sys/socket.h> ],
		[ u_int8_t a; u_int16_t b; u_int32_t c; a = b = c = 1;],
		[
			AC_DEFINE(HAVE_U_INTXX_T, [], [Have u_intXX_t])
			AC_MSG_RESULT(yes)
		],
		[ AC_MSG_RESULT(no) ]
	)
fi

AC_CACHE_CHECK([for u_int64_t types], ac_cv_have_u_int64_t, [
	AC_TRY_COMPILE(
		[ #include <sys/types.h> ],
		[ u_int64_t a; a = 1;],
		[ ac_cv_have_u_int64_t="yes" ],
		[ ac_cv_have_u_int64_t="no" ]
	)
])
if test "x$ac_cv_have_u_int64_t" = "xyes" ; then
	AC_DEFINE(HAVE_U_INT64_T, [], [Have u_int64_t])
	have_u_int64_t=1
fi

if test -z "$have_u_int64_t" ; then
    AC_MSG_CHECKING([for u_int64_t type in sys/bitypes.h])
	AC_TRY_COMPILE(
		[ #include <sys/bitypes.h> ],
		[ u_int64_t a; a = 1],
		[
			AC_DEFINE(HAVE_U_INT64_T, [], [Have u_int64_t])
			AC_MSG_RESULT(yes)
		],
		[ AC_MSG_RESULT(no) ]
	)
fi

if test -z "$have_u_intxx_t" ; then
	AC_CACHE_CHECK([for uintXX_t types], ac_cv_have_uintxx_t, [
		AC_TRY_COMPILE(
			[
#include <sys/types.h>
			],
			[ uint8_t a; uint16_t b; uint32_t c; a = b = c = 1; ],
			[ ac_cv_have_uintxx_t="yes" ],
			[ ac_cv_have_uintxx_t="no" ]
		)
	])
	if test "x$ac_cv_have_uintxx_t" = "xyes" ; then
		AC_DEFINE(HAVE_UINTXX_T, [], [Have uintXX_t types])
	fi
fi

if test -z "$have_uintxx_t" ; then
    AC_MSG_CHECKING([for uintXX_t types in stdint.h])
	AC_TRY_COMPILE(
		[ #include <stdint.h> ],
		[ uint8_t a; uint16_t b; uint32_t c; a = b = c = 1;],
		[
			AC_DEFINE(HAVE_UINTXX_T, [], [Have unitXX_t])
			AC_MSG_RESULT(yes)
		],
		[ AC_MSG_RESULT(no) ]
	)
fi

if (test -z "$have_u_intxx_t" || test -z "$have_intxx_t" && \
	   test "x$ac_cv_header_sys_bitypes_h" = "xyes")
then
	AC_MSG_CHECKING([for intXX_t and u_intXX_t types in sys/bitypes.h])
	AC_TRY_COMPILE(
		[
#include <sys/bitypes.h>
		],
		[
			int8_t a; int16_t b; int32_t c;
			u_int8_t e; u_int16_t f; u_int32_t g;
			a = b = c = e = f = g = 1;
		],
		[
			AC_DEFINE(HAVE_U_INTXX_T, [], [Have u_intXX_t])
			AC_DEFINE(HAVE_INTXX_T, [], [Have intXX_t])
			AC_MSG_RESULT(yes)
		],
		[AC_MSG_RESULT(no)]
	)
fi


AC_CACHE_CHECK([for u_char], ac_cv_have_u_char, [
	AC_TRY_COMPILE(
		[
#include <sys/types.h>
		],
		[ u_char foo; foo = 125; ],
		[ ac_cv_have_u_char="yes" ],
		[ ac_cv_have_u_char="no" ]
	)
])
if test "x$ac_cv_have_u_char" = "xyes" ; then
	AC_DEFINE(HAVE_U_CHAR, [], [Have u_char type])
fi

AC_CHECK_TYPES(sig_atomic_t,,,[#include <signal.h>])

AC_CHECK_TYPES(socklen_t,,,[
#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif
])

AC_ARG_WITH(privsep-user,
	[  --with-privsep-user=user Specify privilege separation user],
	[ AC_DEFINE_UNQUOTED(NTPD_USER, "$withval", [Unprivileged userid])
	  privsep_user=$withval ],
	[ privsep_user=_ntp ]
)
AC_SUBST(privsep_user)

AC_ARG_WITH(privsep-path,
	[  --with-privsep-path=path Specify privilege separation chroot path],
	[ AC_DEFINE_UNQUOTED(NTPD_CHROOT_DIR, "$withval",
	    [Privilege separation chroot path])
	  privsep_path=$withval ],
	[ privsep_path=/var/empty ]
)
AC_SUBST(privsep_path)

STRIP_OPT=-s
AC_ARG_ENABLE(strip,
	[  --disable-strip	 Disable calling strip(1) on install],
	[
		if test "x$enableval" = "xno" ; then
			STRIP_OPT=
		fi
	]
)
AC_SUBST(STRIP_OPT)

AC_ARG_WITH(builtin-arc4random,
	[  --with-builtin-arc4random Use builtin arc4random rather than OpenSSL's],
	[ builtin_arc4random=$withval ]
)

AC_ARG_WITH(mantype,
	[  --with-mantype=man|cat|doc  Set man page type],
	[
		case "$withval" in
		man|cat|doc)
			MANTYPE=$withval
			;;
		*)
			AC_MSG_ERROR(invalid man type: $withval)
			;;
		esac
	]
)
if test -z "$MANTYPE"; then
	TestPath="/usr/bin${PATH_SEPARATOR}/usr/ucb"
	AC_PATH_PROGS(NROFF, nroff awf, /bin/false, $TestPath)
	AC_MSG_CHECKING(man page format)
	if ${NROFF} -mdoc ${srcdir}/ntpd.8 >/dev/null 2>&1; then
		MANTYPE=doc
	elif ${NROFF} -man ${srcdir}/ntpd.8 >/dev/null 2>&1; then
		MANTYPE=man
	else
		MANTYPE=cat
	fi
	AC_MSG_RESULT($MANTYPE)
fi
AC_SUBST(MANTYPE)
if test "$MANTYPE" = "doc"; then
	mansubdir=man;
else
	mansubdir=$MANTYPE;
fi
AC_SUBST(mansubdir)

# Search for OpenSSL if required.
if test "$ac_cv_func_arc4random" != "yes" && test "x$builtin_arc4random" != "xyes"; then
saved_CPPFLAGS="$CPPFLAGS"
saved_LDFLAGS="$LDFLAGS"
AC_ARG_WITH(ssl-dir,
	[  --with-ssl-dir=PATH     Specify path to OpenSSL installation ],
	[
		if test "x$withval" != "xno" ; then
			if test -d "$withval/lib"; then
				if test -n "${need_dash_r}"; then
					LDFLAGS="-L${withval}/lib -R${withval}/lib ${LDFLAGS}"
				else
					LDFLAGS="-L${withval}/lib ${LDFLAGS}"
				fi
			else
				if test -n "${need_dash_r}"; then
					LDFLAGS="-L${withval} -R${withval} ${LDFLAGS}"
				else
					LDFLAGS="-L${withval} ${LDFLAGS}"
				fi
			fi
			if test -d "$withval/include"; then
				CPPFLAGS="-I${withval}/include ${CPPFLAGS}"
			else
				CPPFLAGS="-I${withval} ${CPPFLAGS}"
			fi
		fi
	]
)
saved_LIBS="$LIBS"
LIBS="-lcrypto $LIBS"
AC_TRY_LINK_FUNC(RAND_add, AC_DEFINE(HAVE_OPENSSL, [], [Have OpenSSL]),
	[
		dnl Check default openssl install dir
		if test -n "${need_dash_r}"; then
			LDFLAGS="-L/usr/local/ssl/lib -R/usr/local/ssl/lib ${saved_LDFLAGS}"
		else
			LDFLAGS="-L/usr/local/ssl/lib ${saved_LDFLAGS}"
		fi
		CPPFLAGS="-I/usr/local/ssl/include ${saved_CPPFLAGS}"
		AC_TRY_LINK_FUNC(RAND_add, AC_DEFINE(HAVE_OPENSSL),
			[
				AC_MSG_WARN([*** Can't find recent OpenSSL libcrypto, enabling internal arc4random ***])
				builtin_arc4random=yes
				LIBS="$saved_LIBS"
			]
		)
	]
)
fi

if test "$ac_cv_func_arc4random" != "yes" && test "x$builtin_arc4random" != "xyes"; then
# Determine OpenSSL header version
AC_MSG_CHECKING([OpenSSL header version])
AC_RUN_IFELSE(
	[
#include <stdio.h>
#include <string.h>
#include <openssl/opensslv.h>
#define DATA "conftest.sslincver"
int main(void) {
	FILE *fd;
	int rc;

	fd = fopen(DATA,"w");
	if(fd == NULL)
		exit(1);

	if ((rc = fprintf(fd ,"%x (%s)\n", OPENSSL_VERSION_NUMBER, OPENSSL_VERSION_TEXT)) <0)
		exit(1);

	exit(0);
}
	],
	[
		ssl_header_ver=`cat conftest.sslincver`
		AC_MSG_RESULT($ssl_header_ver)
	],
	[
		AC_MSG_RESULT(not found)
		AC_MSG_ERROR(OpenSSL version header not found.)
	],
	[
		AC_MSG_WARN([cross compiling: not checked])
	]
)

# Determine OpenSSL library version
AC_MSG_CHECKING([OpenSSL library version])
AC_RUN_IFELSE(
	[
#include <stdio.h>
#include <string.h>
#include <openssl/opensslv.h>
#include <openssl/crypto.h>
#define DATA "conftest.ssllibver"
int main(void) {
	FILE *fd;
	int rc;

	fd = fopen(DATA,"w");
	if(fd == NULL)
		exit(1);

	if ((rc = fprintf(fd ,"%x (%s)\n", SSLeay(), SSLeay_version(SSLEAY_VERSION))) <0)
		exit(1);

	exit(0);
}
	],
	[
		ssl_library_ver=`cat conftest.ssllibver`
		AC_MSG_RESULT($ssl_library_ver)
	],
	[
		AC_MSG_RESULT(not found)
		AC_MSG_ERROR(OpenSSL library not found.)
	],
	[
		AC_MSG_WARN([cross compiling: not checking])
	]
)

# Sanity check OpenSSL headers
AC_MSG_CHECKING([whether OpenSSL's headers match the library])
AC_RUN_IFELSE(
	[
#include <string.h>
#include <openssl/opensslv.h>
int main(void) { exit(SSLeay() == OPENSSL_VERSION_NUMBER ? 0 : 1); }
	],
	[
		AC_MSG_RESULT(yes)
	],
	[
		AC_MSG_RESULT(no)
		AC_MSG_ERROR([Your OpenSSL headers do not match your library.
Check config.log for details.
Also see contrib/findssl.sh for help identifying header/library mismatches.])
	],
	[
		AC_MSG_WARN([cross compiling: not checking])
	]
)

### Configure cryptographic random number support

# Check wheter OpenSSL seeds itself
AC_MSG_CHECKING([whether OpenSSL's PRNG is internally seeded])
AC_RUN_IFELSE(
	[
#include <string.h>
#include <openssl/rand.h>
int main(void) { exit(RAND_status() == 1 ? 0 : 1); }
	],
	[
		OPENSSL_SEEDS_ITSELF=yes
		AC_MSG_RESULT(yes)
	],
	[
		AC_MSG_RESULT(no)
		AC_MSG_ERROR(OpenNTPd requires a self-seeding OpenSSL)
	],
	[
		AC_MSG_WARN([cross compiling: assuming yes])
	]
)
fi

AC_CACHE_CHECK([whether $CC implements __FUNCTION__], ac_cv_cc_implements___FUNCTION__, [
	AC_TRY_LINK([
#include <stdio.h>
],
		[ printf("%s", __FUNCTION__); ],
		[ ac_cv_cc_implements___FUNCTION__="yes" ],
		[ ac_cv_cc_implements___FUNCTION__="no" ]
	)
])
if test "x$ac_cv_cc_implements___FUNCTION__" = "xyes" ; then
	AC_DEFINE(HAVE___FUNCTION__, 1, [have __FUNCTION__ macro])
fi

AC_CACHE_CHECK([whether $CC implements __func__], ac_cv_cc_implements___func__, [
	AC_TRY_LINK([
#include <stdio.h>
],
		[ printf("%s", __func__); ],
		[ ac_cv_cc_implements___func__="yes" ],
		[ ac_cv_cc_implements___func__="no" ]
	)
])
if test "x$ac_cv_cc_implements___func__" = "xyes" ; then
	AC_DEFINE(HAVE___func__, 1, [have __func__ macro])
fi

AC_CONFIG_FILES([Makefile openbsd-compat/Makefile])
AC_OUTPUT
