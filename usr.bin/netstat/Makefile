#	$NetBSD: Makefile,v 1.43 2015/06/06 13:48:37 joerg Exp $
#	from: @(#)Makefile	8.1 (Berkeley) 6/12/93

.include <bsd.own.mk>

USE_FORT?= yes	# setgid

RUMPPRG=netstat
#SRCS=	atalk.c bpf.c fast_ipsec.c if.c inet.c inet6.c
SRCS=	atalk.c bpf.c if.c inet.c inet6.c \
	main.c mbuf.c mroute.c mroute6.c pfkey.c pfsync.c route.c \
	unix.c vtw.c rtutil.c
.if !defined(__MINIX)
BINGRP=	kmem
BINMODE=2555
.endif # !defined(__MINIX)
LDADD=	-lutil -lkvm
DPADD=	${LIBUTIL} ${LIBKVM}
.if !defined(__MINIX)
CPPFLAGS+= -DIPSEC -I${.CURDIR}
.else # defined(__MINIX)
CPPFLAGS+= -I${.CURDIR}
.endif # defined(__MINIX)
CPPFLAGS+= -I${NETBSDSRCDIR}/sys/dist/pf
CPPFLAGS+= -I${NETBSDSRCDIR}/sbin/route

.PATH:  ${.CURDIR}/../../lib/libc/gen
.PATH:  ${.CURDIR}/../../lib/libc/net
.PATH:  ${.CURDIR}/../../sbin/route
CPPFLAGS+= -DRUMP_ACTION
RUMPSRCS+= sysctlbyname.c sysctlgetmibinfo.c sysctlnametomib.c
RUMPSRCS+= if_indextoname.c getifaddrs.c getnameinfo.c

.if (${USE_INET6} != "no")
CPPFLAGS+= -DINET6
.endif

.if defined(__MINIX)
# FIXME: compilation on ARM fails on a format string warning because of a
# mismatch between the PRIxPTR format and the uintptr_t type, which should not
# be possible.  This is a bug in the ARM port, but I have no idea how to solve
# that.  For now, simply do not fail the build on this error.  --dcvmoole
CPPFLAGS+= -Wno-error=format
.endif # defined(__MINIX)

.include <bsd.prog.mk>
