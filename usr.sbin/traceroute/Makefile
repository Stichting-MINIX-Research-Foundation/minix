#	$NetBSD: Makefile,v 1.19 2011/09/11 01:06:26 christos Exp $	

USE_FORT?= yes	# network client

RUMPPRG=traceroute
MAN=	traceroute.8 

CPPFLAGS+=-DHAVE_MALLOC_H=1 -DHAVE_SYS_SELECT_H=1 -DHAVE_SYS_SOCKIO_H=1
CPPFLAGS+=-DHAVE_STRERROR=1 -DHAVE_SETLINEBUF=1 -DHAVE_SOCKADDR_SA_LEN=1 
CPPFLAGS+=-DHAVE_RAW_OPTIONS=1
CPPFLAGS+=-DHAVE_ICMP_NEXTMTU=1

.include <bsd.own.mk>
.if !defined(__MINIX)
CPPFLAGS+=-DIPSEC
LDADD+=	-lipsec
DPADD+=	${LIBIPSEC}
.endif # !defined(__MINIX)

BINOWN= root
BINMODE=4555

SRCS=	traceroute.c ifaddrlist.c 
SRCS+= version.c as.c

.PATH: ${.CURDIR}/../../lib/libc/net
RUMPSRCS= getifaddrs.c
CPPFLAGS+= -DRUMP_ACTION

AWKS=	median.awk mean.awk 

.include <bsd.prog.mk>
