#	$NetBSD: Makefile,v 1.35 2012/06/20 21:38:26 sjg Exp $

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
