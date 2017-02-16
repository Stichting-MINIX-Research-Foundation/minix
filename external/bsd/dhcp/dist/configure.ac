AC_INIT([DHCP],[4.3.0],[dhcp-users@isc.org])

# we specify "foreign" to avoid having to have the GNU mandated files,
# like AUTHORS, COPYING, and such
AM_INIT_AUTOMAKE([foreign])

# we specify AM_MAINTAINER_MODE to avoid problems with rebuilding
# the configure and makefiles.  Without it users doing things that
# change the timestamps on the code, like checking it into a cvs
# tree, could trigger a rebuild of the infrastructure files which
# might fail if they don't have the correct tools.
AM_MAINTAINER_MODE 

AC_CANONICAL_HOST

# We want to turn on warnings if we are using gcc and the user did 
# not specify CFLAGS. The autoconf check for the C compiler sets the
# CFLAGS if gcc is used, so we will save it before we run that check.
SAVE_CFLAGS="$CFLAGS"

# Now find our C compiler.
AC_PROG_CC

# Suppress warnings about --datarootdir
AC_DEFUN([AC_DATAROOTDIR_CHECKED])

# If we have gcc, and AC_PROG_CC changed the flags, then we know the
# user did not specify any flags. Add warnings in this case.
if test "$GCC" = "yes"; then
	if test "$CFLAGS" != "$SAVE_CFLAGS"; then
		STD_CWARNINGS="$STD_CWARNINGS -Wall -Werror -fno-strict-aliasing"
	fi
fi

# POSIX doesn't include the IPv6 Advanced Socket API and glibc hides
# parts of the IPv6 Advanced Socket API as a result.  This is stupid
# as it breaks how the two halves (Basic and Advanced) of the IPv6
# Socket API were designed to be used but we have to live with it.
# Use this to define _GNU_SOURCE to pull in the IPv6 Advanced Socket API.
AC_USE_SYSTEM_EXTENSIONS

AC_PROG_RANLIB
AC_CONFIG_HEADERS([includes/config.h])

# we sometimes need to know byte order for building packets
AC_C_BIGENDIAN(AC_SUBST(byte_order, BIG_ENDIAN), 
	       AC_SUBST(byte_order, LITTLE_ENDIAN))
AC_DEFINE_UNQUOTED([DHCP_BYTE_ORDER], [$byte_order], 
		   [Define to BIG_ENDIAN for MSB (Motorola or SPARC CPUs)
		    or LITTLE_ENDIAN for LSB (Intel CPUs).])

# Optional compile-time DEBUGging.
AC_ARG_ENABLE(debug,
	AS_HELP_STRING([--enable-debug],[create a debug-only version of the software (default is no).]),
		[enable_debug=yes],[enable_debug=no])
# This is very much off by default.
if test "$enable_debug" = "yes" ; then
	AC_DEFINE([DEBUG], [1],
		[Define to compile debug-only DHCP software.])
	# Just override CFLAGS to totally to remove optimization.
	CFLAGS="-g"
fi
# XXX: there are actually quite a lot more DEBUG_ features we could enable,
# but I don't want to pollute the --help space.
#
#/* #define DEBUG_TOKENS */
#/* #define DEBUG_PACKET */
#/* #define DEBUG_EXPRESSIONS */
#/* #define DEBUG_FIND_LEASE */
#/* #define DEBUG_EXPRESSION_PARSE */
#/* #define DEBUG_CLASS_MATCHING */
#/* #define DEBUG_MEMORY_LEAKAGE */
#/* #define DEBUG_MALLOC_POOL */
#/* #define DEBUG_LEASE_STATE_TRANSITIONS */
#/* #define DEBUG_RC_HISTORY */
#/* #define DEBUG_RC_HISTORY_EXHAUSTIVELY */
#/* #define RC_HISTORY_MAX 10240 */
#/* #define POINTER_DEBUG */
#/* #define DEBUG_FAILOVER_MESSAGES */
#/* #define DEBUG_FAILOVER_TIMING */
#/* #define DEBUG_DUMP_ALL_LEASES */

# Failover optional compile-time feature.
AC_ARG_ENABLE(failover,
	AS_HELP_STRING([--enable-failover],[enable support for failover (default is yes)]))
# Failover is on by default, so define if it is not explicitly disabled.
if test "$enable_failover" != "no"; then
	AC_DEFINE([FAILOVER_PROTOCOL], [1],
		  [Define to include Failover Protocol support.])
fi

# execute() support.
AC_ARG_ENABLE(execute,
	AS_HELP_STRING([--enable-execute],[enable support for execute() in config (default is yes)]))
# execute() is on by default, so define if it is not explicitly disabled.
if test "$enable_execute" != "no" ; then
	AC_DEFINE([ENABLE_EXECUTE], [1],
		  [Define to include execute() config language support.])
fi

# Server tracing support.
AC_ARG_ENABLE(tracing,
	AS_HELP_STRING([--enable-tracing],[enable support for server activity tracing (default is yes)]))
# tracing is on by default, so define if it is not explicitly disabled.
if test "$enable_tracing" != "no" ; then
	AC_DEFINE([TRACING], [1],
		  [Define to include server activity tracing support.])
fi

# Delayed-ack feature support (experimental).
AC_ARG_ENABLE(delayed_ack,
	AS_HELP_STRING([--enable-delayed-ack],[queues multiple DHCPACK replies (default is no)]))
if test "$enable_delayed_ack" = "yes"; then
	AC_DEFINE([DELAYED_ACK], [1],
		  [Define to queue multiple DHCPACK replies per fsync.])
fi

# DHCPv6 optional compile-time feature.
AC_ARG_ENABLE(dhcpv6,
	AS_HELP_STRING([--enable-dhcpv6],[enable support for DHCPv6 (default is yes)]))
# DHCPv6 is on by default, so define if it is not explicitly disabled.
if test "$enable_dhcpv6" != "no"; then
	AC_DEFINE([DHCPv6], [1], 
		  [Define to 1 to include DHCPv6 support.])
fi

# PARANOIA is off by default (until we can test it with all features)
AC_ARG_ENABLE(paranoia,
	AS_HELP_STRING([--enable-paranoia],[enable support for chroot/setuid (default is no)]))
AC_ARG_ENABLE(early_chroot,
	AS_HELP_STRING([--enable-early-chroot],[enable chrooting prior to configuration (default is no)]))
# If someone enables early chroot, but does not enable paranoia, do so for
# them.
if test "$enable_paranoia" != "yes" && \
   test "$enable_early_chroot" = "yes" ; then
	enable_paranoia="yes"
fi

if test "$enable_paranoia" = "yes" ; then
	AC_DEFINE([PARANOIA], [1],
		  [Define to any value to include Ari's PARANOIA patch.])
fi
if test "$enable_early_chroot" = "yes" ; then
	AC_DEFINE([EARLY_CHROOT], [1],
		  [Define to any value to chroot() prior to loading config.])
fi

AC_ARG_ENABLE(ipv4_pktinfo,
	AS_HELP_STRING([--enable-ipv4-pktinfo],[enable use of pktinfo on IPv4 sockets (default is no)]))

if test "$enable_ipv4_pktinfo" = "yes"; then
	AC_DEFINE([USE_V4_PKTINFO], [1],
		[Define to 1 to enable IPv4 packet info support.])
fi

AC_ARG_ENABLE(use_sockets,
	AS_HELP_STRING([--enable-use-sockets],[use the standard BSD socket API (default is no)]))

if test "$enable_use_sockets" = "yes"; then
	AC_DEFINE([USE_SOCKETS], [1],
		[Define to 1 to use the standard BSD socket API.])
fi

# Try to hnadle incorrect byte order for secs field
# This is off by default
AC_ARG_ENABLE(secs_byteorder,
	AS_HELP_STRING([--enable-secs-byteorder],[Correct bad byteorders in the secs field (default is no).]))

if test "$enable_secs_byteorder" = "yes" ; then
	AC_DEFINE([SECS_BYTEORDER], [1],
		[Define to correct bad byteorders in secs field.])
fi

# Testing section

atf_path="no"
AC_ARG_WITH([atf],
    AS_HELP_STRING([--with-atf=PATH],[specify location where atf was installed]),
    [atf_path="$withval"])
if test "$atf_path" != "no" ; then
    # Config path for pkg-config
    atf_pcp=""
    if test "$atf_path" != "yes" ; then
        if test -f $atf_path/lib/pkgconfig/atf-c.pc ; then
            atf_pcp=$atf_path/lib/pkgconfig
        elif test -f $atf_path/lib64/pkgconfig/atf-c.pc ; then
            atf_pcp=$atf_path/lib64/pkgconfig
        fi
    else
        # Not specified, try some common paths
        atf_dirs="/usr /usr/local /usr/pkg /opt /opt/local"
        for d in $atf_dirs
        do
            if test -f $d/lib/pkgconfig/atf-c.pc ; then
                atf_pcp=$d/lib/pkgconfig
            elif test -f $d/lib64/pkgconfig/atf-c.pc ; then
                atf_pcp=$d/lib64/pkgconfig
            fi
        done
    fi
    if test "$atf_pcp" = "" ; then
        AC_MSG_ERROR([Unable to find atf files in location specified])
    else
        ATF_CFLAGS="`PKG_CONFIG_PATH=$atf_pcp pkg-config --cflags atf-c` -DUNIT_TEST"
        ATF_LDFLAGS="`PKG_CONFIG_PATH=$atf_pcp pkg-config --libs atf-c`"
        AC_SUBST(ATF_CFLAGS)
        AC_SUBST(ATF_LDFLAGS)
    fi
fi

AM_CONDITIONAL(HAVE_ATF, test "$atf_pcp" != "")
### Uncomment this once docs.lab.isc.org upgrades to automake 1.11
### AM_COND_IF([HAVE_ATF], [AC_DEFINE([HAVE_ATF], [1], [ATF framework specified?])])

###
### Path fun.  Older versions of DHCP were installed in /usr/sbin, so we
### need to look there and potentially overwrite by default (but not if
### the user configures an alternate value).  LOCALSTATEDIR is totally
### braindead.  No one uses /usr/local/var/db/ nor /usr/local/var/run, and
### they would be insane for suggesting it.  We need to look in /var/for
### 'db' and 'state/dhcp' for db files, and /var/run for pid files by
### default.
###
AC_PREFIX_PROGRAM(dhcpd)

# XXX - isn't there SOME WAY to default autoconf to /var instead of
# /usr/local/var/no/one/has/this/please/stop/trying?
case "$localstatedir" in
	'${prefix}/var')
		localstatedir=/var
		;;
esac

# Allow specification of alternate state files
AC_ARG_WITH(srv-lease-file,
	AS_HELP_STRING([--with-srv-lease-file=PATH],[File for dhcpd leases 
		        (default is LOCALSTATEDIR/db/dhcpd.leases)]),
	AC_DEFINE_UNQUOTED([_PATH_DHCPD_DB], ["$withval"],
			   [File for dhcpd leases.]))

echo -n "checking for dhcpd.leases location..."
if [[ "x$with_srv_lease_file" = "x" ]] ; then
	if [[ -d "${localstatedir}/db" ]] ; then
		with_srv_lease_file="${localstatedir}/db/dhcpd.leases"
	elif [[ -d "${localstatedir}/state" ]] ; then
		if [[ -d "${localstatedir}/state/dhcp" ]] ; then
			with_srv_lease_file="${localstatedir}/state/dhcp/dhcpd.leases"
		else
			with_srv_lease_file="${localstatedir}/state/dhcpd.leases"
		fi
	elif [[ -d "${localstatedir}/lib" ]] ; then
		if [[ -d "${localstatedir}/lib/dhcp" ]] ; then
			with_srv_lease_file="${localstatedir}/lib/dhcp/dhcpd.leases"
		else
			with_srv_lease_file="${localstatedir}/lib/dhcpd.leases"
		fi
	elif [[ -d "${localstatedir}/etc" ]] ; then
		with_srv_lease_file="${localstatedir}/etc/dhcpd.leases"
	else
		with_srv_lease_file="/etc/dhcpd.leases"
	fi
fi
echo "$with_srv_lease_file"

AC_ARG_WITH(srv6-lease-file,
	AS_HELP_STRING([--with-srv6-lease-file=PATH],[File for dhcpd6 leases 
		        (default is LOCALSTATEDIR/db/dhcpd6.leases)]),
	AC_DEFINE_UNQUOTED([_PATH_DHCPD6_DB], ["$withval"],
			   [File for dhcpd6 leases.]))

echo -n "checking for dhcpd6.leases location..."
if [[ "x$with_srv6_lease_file" = "x" ]] ; then
	if [[ -d "${localstatedir}/db" ]] ; then
		with_srv6_lease_file="${localstatedir}/db/dhcpd6.leases"
	elif [[ -d "${localstatedir}/state" ]] ; then
		if [[ -d "${localstatedir}/state/dhcp" ]] ; then
			with_srv6_lease_file="${localstatedir}/state/dhcp/dhcpd6.leases"
		else
			with_srv6_lease_file="${localstatedir}/state/dhcpd6.leases"
		fi
	elif [[ -d "${localstatedir}/lib" ]] ; then
		if [[ -d "${localstatedir}/lib/dhcp" ]] ; then
			with_srv6_lease_file="${localstatedir}/lib/dhcp/dhcpd6.leases"
		else
			with_srv6_lease_file="${localstatedir}/lib/dhcpd6.leases"
		fi
	elif [[ -d "${localstatedir}/etc" ]] ; then
		with_srv6_lease_file="${localstatedir}/etc/dhcpd6.leases"
	else
		with_srv6_lease_file="/etc/dhcpd6.leases"
	fi
fi
echo "$with_srv6_lease_file"

AC_ARG_WITH(cli-lease-file,
	AS_HELP_STRING([--with-cli-lease-file=PATH],[File for dhclient leases 
		        (default is LOCALSTATEDIR/db/dhclient.leases)]),
	AC_DEFINE_UNQUOTED([_PATH_DHCLIENT_DB], ["$withval"],
			   [File for dhclient leases.]))

echo -n "checking for dhclient.leases location..."
if [[ "x$with_cli_lease_file" = "x" ]] ; then
	if [[ -d "${localstatedir}/db" ]] ; then
		with_cli_lease_file="${localstatedir}/db/dhclient.leases"
	elif [[ -d "${localstatedir}/state" ]] ; then
		if [[ -d "${localstatedir}/state/dhcp" ]] ; then
			with_cli_lease_file="${localstatedir}/state/dhcp/dhclient.leases"
		else
			with_cli_lease_file="${localstatedir}/state/dhclient.leases"
		fi
	elif [[ -d "${localstatedir}/lib" ]] ; then
		if [[ -d "${localstatedir}/lib/dhcp" ]] ; then
			with_cli_lease_file="${localstatedir}/lib/dhcp/dhclient.leases"
		else
			with_cli_lease_file="${localstatedir}/lib/dhclient.leases"
		fi
	elif [[ -d "${localstatedir}/etc" ]] ; then
		with_cli_lease_file="${localstatedir}/etc/dhclient.leases"
	else
		with_cli_lease_file="/etc/dhclient.leases"
	fi
fi
echo "$with_cli_lease_file"

AC_ARG_WITH(cli6-lease-file,
	AS_HELP_STRING([--with-cli6-lease-file=PATH],[File for dhclient6 leases 
		        (default is LOCALSTATEDIR/db/dhclient6.leases)]),
	AC_DEFINE_UNQUOTED([_PATH_DHCLIENT6_DB], ["$withval"],
			   [File for dhclient6 leases.]))

echo -n "checking for dhclient6.leases location..."
if [[ "x$with_cli6_lease_file" = "x" ]] ; then
	if [[ -d "${localstatedir}/db" ]] ; then
		with_cli6_lease_file="${localstatedir}/db/dhclient6.leases"
	elif [[ -d "${localstatedir}/state" ]] ; then
		if [[ -d "${localstatedir}/state/dhcp" ]] ; then
			with_cli6_lease_file="${localstatedir}/state/dhcp/dhclient6.leases"
		else
			with_cli6_lease_file="${localstatedir}/state/dhclient6.leases"
		fi
	elif [[ -d "${localstatedir}/lib" ]] ; then
		if [[ -d "${localstatedir}/lib/dhcp" ]] ; then
			with_cli6_lease_file="${localstatedir}/lib/dhcp/dhclient6.leases"
		else
			with_cli6_lease_file="${localstatedir}/lib/dhclient6.leases"
		fi
	elif [[ -d "${localstatedir}/etc" ]] ; then
		with_cli6_lease_file="${localstatedir}/etc/dhclient6.leases"
	else
		with_cli6_lease_file="/etc/dhclient6.leases"
	fi
fi
echo "$with_cli6_lease_file"

AC_ARG_WITH(srv-pid-file,
	AS_HELP_STRING([--with-srv-pid-file=PATH],[File for dhcpd process information
		        (default is LOCALSTATEDIR/run/dhcpd.pid)]),
	AC_DEFINE_UNQUOTED([_PATH_DHCPD_PID], ["$withval"],
			   [File for dhcpd process information.]))
AC_ARG_WITH(srv6-pid-file,
	AS_HELP_STRING([--with-srv6-pid-file=PATH],[File for dhcpd6 process information
		        (default is LOCALSTATEDIR/run/dhcpd6.pid)]),
	AC_DEFINE_UNQUOTED([_PATH_DHCPD6_PID], ["$withval"],
			   [File for dhcpd6 process information.]))
AC_ARG_WITH(cli-pid-file,
	AS_HELP_STRING([--with-cli-pid-file=PATH],[File for dhclient process information
		        (default is LOCALSTATEDIR/run/dhclient.pid)]),
	AC_DEFINE_UNQUOTED([_PATH_DHCLIENT_PID], ["$withval"],
			   [File for dhclient process information.]))
AC_ARG_WITH(cli6-pid-file,
	AS_HELP_STRING([--with-cli6-pid-file=PATH],[File for dhclient6 process information
		        (default is LOCALSTATEDIR/run/dhclient6.pid)]),
	AC_DEFINE_UNQUOTED([_PATH_DHCLIENT6_PID], ["$withval"],
			   [File for dhclient6 process information.]))
AC_ARG_WITH(relay-pid-file,
	AS_HELP_STRING([--with-relay-pid-file=PATH],[File for dhcrelay process information
		        (default is LOCALSTATEDIR/run/dhcrelay.pid)]),
	AC_DEFINE_UNQUOTED([_PATH_DHCRELAY_PID], ["$withval"],
			   [File for dhcrelay process information.]))
AC_ARG_WITH(relay6-pid-file,
	AS_HELP_STRING([--with-relay6-pid-file=PATH],[File for dhcrelay6 process information
		        (default is LOCALSTATEDIR/run/dhcrelay6.pid)]),
	AC_DEFINE_UNQUOTED([_PATH_DHCRELAY6_PID], ["$withval"],
			   [File for dhcrelay6 process information.]))

# Check basic types.
AC_TYPE_INT8_T
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_INT64_T

# Some systems need the u_intX_t types defined across.
AC_CHECK_TYPE([u_int8_t], [], [
  AC_TYPE_UINT8_T
  AC_DEFINE(u_int8_t, [uint8_t], [Define a type for 8-bit unsigned
				  integers.])
])
AC_CHECK_TYPE([u_int16_t], [], [
  AC_TYPE_UINT16_T
  AC_DEFINE(u_int16_t, [uint16_t], [Define a type for 16-bit unsigned
				    integers.])
])
AC_CHECK_TYPE([u_int32_t], [], [
  AC_TYPE_UINT32_T
  AC_DEFINE(u_int32_t, [uint32_t], [Define a type for 32-bit unsigned
				    integers.])
])
AC_CHECK_TYPE([u_int64_t], [], [
  AC_TYPE_UINT64_T
  AC_DEFINE(u_int64_t, [uint64_t], [Define a type for 64-bit unsigned
				    integers.])
])

# see if ifaddrs.h is available
AC_CHECK_HEADERS(ifaddrs.h)

# figure out what IPv4 interface code to use
AC_CHECK_HEADERS(linux/types.h)  # needed for linux/filter.h on old systems

AC_CHECK_HEADER(linux/filter.h, DO_LPF=1, ,
[
#ifdef HAVE_LINUX_TYPES_H
#include <linux/types.h>
#endif
])
if test -n "$DO_LPF"
then
	AC_DEFINE([HAVE_LPF], [1], 
		  [Define to 1 to use the Linux Packet Filter interface code.])
else
	AC_CHECK_HEADER(sys/dlpi.h, DO_DLPI=1)
	if test -n "$DO_DLPI"
	then
		AC_DEFINE([HAVE_DLPI], [1], 
			  [Define to 1 to use DLPI interface code.])
	else
		AC_CHECK_HEADER(net/bpf.h, DO_BPF=1)
		if test -n "$DO_BPF"
		then
			AC_DEFINE([HAVE_BPF], [1],
	  			  [Define to 1 to use the 
				   Berkeley Packet Filter interface code.])
		fi
	fi
fi

# SIOCGLIFCONF uses some transport structures.  Trick is not all platforms
# use the same structures.  We like to use 'struct lifconf' and 'struct
# lifreq', but we'll use these other structures if they're present.  HPUX
# does not define 'struct lifnum', but does use SIOCGLIFNUM - they use an
# int value.
#
AC_MSG_CHECKING([for struct lifnum])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[ #include <sys/types.h>
  #include <sys/socket.h>
  #include <net/if.h>
]], [[ struct lifnum a;
]])],[AC_MSG_RESULT(yes)
	 AC_DEFINE([ISC_PLATFORM_HAVELIFNUM], [1],
		   [Define to 1 if the system has 'struct lifnum'.])],[AC_MSG_RESULT(no)])

AC_MSG_CHECKING([for struct if_laddrconf])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[ #include <sys/types.h>
  #include <net/if6.h>
]], [[ struct if_laddrconf a;
]])],[AC_MSG_RESULT(yes)
	 AC_DEFINE([ISC_PLATFORM_HAVEIF_LADDRCONF], [1],
		   [Define to 1 if the system has 'struct if_laddrconf'.])],[AC_MSG_RESULT(no)])

AC_MSG_CHECKING([for struct if_laddrreq])
AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <sys/types.h>
 #include <net/if6.h>
]], [[ struct if_laddrreq a;
]])],[AC_MSG_RESULT(yes)
	 AC_DEFINE([ISC_PLATFORM_HAVEIF_LADDRREQ], [1],
		   [Define to 1 if the system has 'struct if_laddrreq'.])],[AC_MSG_RESULT(no)])

#
# check for GCC noreturn attribute
#
AC_MSG_CHECKING(for GCC noreturn attribute)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[void foo() __attribute__((noreturn));]])],[AC_MSG_RESULT(yes)
	 AC_DEFINE([ISC_DHCP_NORETURN], [__attribute__((noreturn))],
		   [Define to the string for a noreturn attribute.])],[AC_MSG_RESULT(no)
	 AC_DEFINE([ISC_DHCP_NORETURN], [],
		   [Define to the string for a noreturn attribute.])])

# Look for optional headers.
AC_CHECK_HEADERS(sys/socket.h net/if_dl.h net/if6.h regex.h)

# Solaris needs some libraries for functions
AC_SEARCH_LIBS(socket, [socket])
AC_SEARCH_LIBS(inet_ntoa, [nsl])

AC_SEARCH_LIBS(inet_aton, [socket nsl], , 
	AC_DEFINE([NEED_INET_ATON], [1], 
		  [Define to 1 if the inet_aton() function is missing.]))

# Check for a standalone regex library.
AC_SEARCH_LIBS(regcomp, [regex])

AC_CHECK_FUNCS(strlcat)

# For HP/UX we need -lipv6 for if_nametoindex, perhaps others.
AC_SEARCH_LIBS(if_nametoindex, [ipv6])

# check for /dev/random (declares HAVE_DEV_RANDOM)
AC_CHECK_FILE(/dev/random,
	AC_DEFINE([HAVE_DEV_RANDOM], [1], 
		  [Define to 1 if you have the /dev/random file.]))

# see if there is a "sa_len" field in our interface information structure
AC_CHECK_MEMBER(struct sockaddr.sa_len,
	AC_DEFINE([HAVE_SA_LEN], [], 
		  [Define to 1 if the sockaddr structure has a length field.]),
	,
	[#include <sys/socket.h>])

# figure out pointer size
AC_CHECK_SIZEOF(struct iaddr *, , [
#include "includes/inet.h"
#include <stdio.h>
])

# Solaris does not have the msg_control or msg_controlen members 
# in the msghdr structure unless you define:
#
#   _XOPEN_SOURCE, _XOPEN_SOURCE_EXTENDED, and __EXTENSIONS__
# 
# See the "standards" man page for details.
# 
# We check for the msg_control member, and if it is not found, we check
# again with the appropriate defines added to the CFLAGS. (In order to
# do this we have to remove the check from the cache, which is what the
# "unset" is for.)
AC_CHECK_MEMBER(struct msghdr.msg_control,,
	[CFLAGS="$CFLAGS -D_XOPEN_SOURCE -D_XOPEN_SOURCE_EXTENDED=1"
	 CFLAGS="$CFLAGS -D__EXTENSIONS__"
	 unset ac_cv_member_struct_msghdr_msg_control
	 AC_CHECK_MEMBER(struct msghdr.msg_control,,
	 	[AC_MSG_ERROR([Missing msg_control member in 
			       msg_control structure.])],
		[
#include <sys/types.h>
#include <sys/socket.h>
		])
	],
	[
#include <sys/types.h>
#include <sys/socket.h>
	])

libbind=
AC_ARG_WITH(libbind,
	AS_HELP_STRING([--with-libbind=PATH],[bind includes and libraries are in PATH 
		        (default is ./bind)]),
	use_libbind="$withval", use_libbind="no")
case "$use_libbind" in 
yes)
	libbind="\${top_srcdir}/bind"
	;;
no)
	libbind="\${top_srcdir}/bind"
	;;
*)
	libbind="$use_libbind"
	;;
esac

# OpenLDAP support.
AC_ARG_WITH(ldap,
    AS_HELP_STRING([--with-ldap],[enable OpenLDAP support in dhcpd (default is no)]),
    [ldap=$withval],
    [ldap=no])

# OpenLDAP with SSL support.
AC_ARG_WITH(ldapcrypto,
    AS_HELP_STRING([--with-ldapcrypto],[enable OpenLDAP crypto support in dhcpd (default is no)]),
    [ldapcrypto=$withval],
    [ldapcrypto=no])

# OpenLDAP support is disabled by default, if enabled then SSL support is an
# extra optional that is also disabled by default.  Enabling LDAP SSL support
# implies enabling LDAP support.
if test x$ldap = xyes || test x$ldapcrypto = xyes ; then
    AC_SEARCH_LIBS(ldap_initialize, [ldap], ,
		   AC_MSG_FAILURE([*** Cannot find ldap_initialize with -lldap - do you need to install an OpenLDAP2 Devel package?]))
    AC_SEARCH_LIBS(ber_pvt_opt_on, [lber], ,
		   AC_MSG_FAILURE([*** Cannot find ber_pvt_opt_on with -llber - do you need to install an OpenLDAP2 Devel package?]))

    if test x$ldapcrypto = xyes ; then
	AC_SUBST(LDAP_CFLAGS, ["-DLDAP_CONFIGURATION -DLDAP_USE_SSL"])
    else
	AC_SUBST(LDAP_CFLAGS, ["-DLDAP_CONFIGURATION"])
    fi
fi

# Append selected warning levels to CFLAGS before substitution (but after
# AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[]])],[],[]) & etc).
CFLAGS="$CFLAGS $STD_CWARNINGS"

# Try to add the bind include directory
CFLAGS="$CFLAGS -I$libbind/include"

case "$host" in
*-darwin*)
    CFLAGS="$CFLAGS -D__APPLE_USE_RFC_3542" ;;
esac

AC_C_FLEXIBLE_ARRAY_MEMBER

AC_CONFIG_FILES([
  Makefile
  client/Makefile
  common/Makefile
  common/tests/Makefile
  dhcpctl/Makefile
  dst/Makefile
  includes/Makefile
  omapip/Makefile
  relay/Makefile
  server/Makefile
  tests/Makefile
  server/tests/Makefile
  doc/devel/doxyfile
])
AC_OUTPUT

sh util/bindvar.sh

cat > config.report << END

     ISC DHCP source configure results:
    -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

Package:
  Name:          $PACKAGE_NAME
  Version:       $PACKAGE_VERSION

C Compiler:      $CC

Flags:
  DEFS:          $DEFS
  CFLAGS:        $CFLAGS

Features:
  debug:         $enable_debug
  failover:      $enable_failover
  execute:       $enable_execute

Developer:
  ATF unittests : $atf_path

END
# TODO: Add Perl system tests

if test "$atf_path" != "no"
then
echo "ATF_CFLAGS  : $ATF_CFLAGS" >> config.report
echo "ATF_LDFLAGS : $ATF_LDFLAGS" >> config.report
echo
fi

cat config.report

echo
echo Now you can type "make" to build ISC DHCP
echo
