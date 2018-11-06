# $NetBSD: Makefile,v 1.3 2017/04/20 13:18:47 joerg Exp $
# Original from FreeBSD, no rcs id.

PROG= pkg_add
SRCS= main.c perform.c

.include <bsd.init.mk>

.PATH:	${DIST}/add

CPPFLAGS+=	-DPKGSRC_MACHINE_ARCH=\"${MACHINE_ARCH}\"

.include <bsd.prog.mk>
