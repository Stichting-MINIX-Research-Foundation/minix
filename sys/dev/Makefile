#	$NetBSD: Makefile,v 1.37 2015/08/02 11:09:05 jmcneill Exp $

#__MINIX: a lot not imported
SUBDIR=	\
	i2c \
	pci \
	

.include <bsd.own.mk>

.if ${MKISCSI} != "no"
SUBDIR+= iscsi
.endif

INCSDIR= /usr/include/dev

# Only install includes which are used by userland
INCS=	\
	vndvar.h

.include <bsd.kinc.mk>
