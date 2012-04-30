# $NetBSD: Makefile,v 1.6 2011/09/19 01:45:15 christos Exp $

PROG=	cpio
SRCS=	cmdline.c cpio.c

.include <bsd.init.mk>

.PATH:		${LIBARCHIVEDIR}/cpio

CLEANFILES+=	cpio.1

cpio.1: ${LIBARCHIVEDIR}/cpio/bsdcpio.1
	${TOOL_CAT} ${LIBARCHIVEDIR}/cpio/bsdcpio.1 > $@

SYMLINKS+=${BINDIR}/cpio /usr/bin/cpio

COPTS.cpio.c += -Wno-format-nonliteral

.include <bsd.prog.mk>
