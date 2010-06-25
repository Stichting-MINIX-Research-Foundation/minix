# Makefile for acd

.include <bsd.own.mk>

PROG=	acd
CPPFLAGS+= -DARCH=\"`arch`\" -DDESCR=\"/usr/lib/descr\"
LINKS+= ${BINDIR}/acd ${BINDIR}/cc
FILESDIR=	/usr/lib
FILES=		acd.descr
FILESNAME=	descr
MAN=

.include <bsd.prog.mk>
