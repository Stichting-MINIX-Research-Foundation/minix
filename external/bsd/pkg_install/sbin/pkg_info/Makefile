# $NetBSD: Makefile,v 1.3 2011/05/26 12:56:26 joerg Exp $
# Original from FreeBSD, no rcs id

PROG= pkg_info
SRCS= main.c perform.c show.c

.include <bsd.init.mk>

.PATH:	${DIST}/info

CWARNFLAGS.clang+=	-Wno-format-security

.include <bsd.prog.mk>
