#	$NetBSD: Makefile,v 1.32 2012/10/27 22:36:14 alnsn Exp $

#LSC: (MINIX) radix.h and route.h are used in libsa but not installed

INCSDIR= /usr/include/net

INCS=	ethertypes.h \
	if_ether.h \
	radix.h \
	route.h \
	\
	\
	\
	

.if !defined(__MINIX)
SUBDIR=	agr npf
.endif # !defined(__MINIX)

.include <bsd.kinc.mk>

.if !defined(__MINIX)
.PATH: ${NETBSDSRCDIR}/sys/dist/pf/net
.endif # !defined(__MINIX)
