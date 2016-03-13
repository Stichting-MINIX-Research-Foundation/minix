#	$NetBSD: Makefile,v 1.32 2012/10/27 22:36:14 alnsn Exp $

INCSDIR= /usr/include/net

INCS=	dlt.h ethertypes.h if.h if_arp.h \
	if_dl.h if_ether.h \
	\
	\
	\
	pfil.h radix.h \
	route.h 

.if !defined(__MINIX)
SUBDIR=	agr npf
.endif # !defined(__MINIX)

.include <bsd.kinc.mk>

.PATH: ${NETBSDSRCDIR}/sys/dist/pf/net
