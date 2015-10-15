# $NetBSD: Makefile,v 1.13 2014/03/02 22:50:13 jmmv Exp $

USE_ATF_LIBTOOLS=	yes

.include <bsd.init.mk>

.PATH:		${SRCDIR}/tools

BINDIR=		/usr/bin
PROG_CXX=	atf-version
SRCS=		atf-version.cpp
MAN=		atf-version.1

CPPFLAGS+=	-I${SRCDIR}/tools
CPPFLAGS+=	-I.

WARNS?=		2

DPSRCS+=	version.hpp
CLEANFILES+=	version.hpp
version.hpp: Makefile atf-version.txt
	echo "#define ATF_VERSION \"$$(cat atf-version.txt)\"" >version.hpp

.include <bsd.prog.mk>
