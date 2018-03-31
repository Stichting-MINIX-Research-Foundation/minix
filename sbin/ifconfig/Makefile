#	$NetBSD: Makefile,v 1.56 2015/05/19 08:14:38 ozaki-r Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/5/93

# when making a change to this file, please check if the change is
# also needed for src/distrib/utils/x_ifconfig.
# such stuff should be into Makefile.inc.

.include <bsd.own.mk>

RUMPPRG=ifconfig
MAN=	ifconfig.8

.if defined(__MINIX)
CWARNFLAGS.gcc+=	-Wno-unused-but-set-variable
.endif # defined(__MINIX)
#DBG+=-g
SRCS= af_atalk.c af_link.c carp.c
.if (${USE_INET6} != "no")
CPPFLAGS+=	-DINET6
SRCS+= af_inet6.c
.endif

.include "Makefile.inc"

.PATH:		${.CURDIR}/../../lib/libc/net
RUMPSRCS= getifaddrs.c getnameinfo.c if_indextoname.c
.if (${MKRUMP} != "no")
CPPFLAGS+= -DRUMP_ACTION
.endif

.ifdef SMALLPROG
CPPFLAGS+=-DSMALL
.endif

CPPFLAGS+=-I${NETBSDSRCDIR}/sys/dist/pf/
SRCS+= pfsync.c

.if ${MACHINE_ARCH} == "m68000"
# XXX workaround for gcc -O1 bug (PR bin/40036 and toolchain/40066)
COPTS.ifconfig.c+= -fno-loop-optimize
.endif

.include <bsd.prog.mk>
