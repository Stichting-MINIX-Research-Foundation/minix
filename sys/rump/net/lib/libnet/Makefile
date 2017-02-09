#	$NetBSD: Makefile,v 1.26 2015/08/31 08:02:45 ozaki-r Exp $
#


.PATH:	${.CURDIR}/../../../../net ${.CURDIR}/../../../../compat/common

LIB=	rumpnet_net
IOCONF=	NET.ioconf
# iffy stuff
SRCS=	if.c if_loop.c route.c rtsock.c raw_usrreq.c			\
	raw_cb.c if_media.c link_proto.c net_stats.c if_ethersubr.c
SRCS+=	if_43.c pfil.c
SRCS+=	if_llatbl.c
SRCS+=	net_component.c

.include <bsd.init.mk>

.if !empty(RUMP_NBCOMPAT:M50)
SRCS+=	rtsock_50.c uipc_syscalls_50.c
.endif

.include "${.CURDIR}/../libnetinet/Makefile.inc"
.include "${.CURDIR}/../libnetinet6/Makefile.inc"
.include "${.CURDIR}/../libnetmpls/Makefile.inc"

.include <bsd.lib.mk>
.include <bsd.klinks.mk>
