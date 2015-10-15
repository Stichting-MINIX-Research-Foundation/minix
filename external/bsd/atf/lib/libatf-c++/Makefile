# $NetBSD: Makefile,v 1.24 2014/03/02 22:50:13 jmmv Exp $

NOLINT=		# defined

.include <bsd.init.mk>

LIB=		atf-c++
LIBISCXX=	yes

LIBDPLIBS+=     atf-c	${.CURDIR}/../libatf-c
LIBDPLIBS+=     m	${.CURDIR}/../../../../../lib/libm

.PATH:		${SRCDIR}
.PATH:		${SRCDIR}/atf-c++
.PATH:		${SRCDIR}/atf-c++/detail
.PATH:		${TOPDIR}

WARNS?=		2

SRCS=		application.cpp \
		build.cpp \
		check.cpp \
		config.cpp \
		env.cpp \
		exceptions.cpp \
		fs.cpp \
		process.cpp \
		tests.cpp \
		text.cpp \
		utils.cpp

INCS=		build.hpp \
		check.hpp \
		config.hpp \
		macros.hpp \
		tests.hpp \
		utils.hpp
INCSDIR=	/usr/include/atf-c++

INCS+=		atf-c++.hpp
INCSDIR_atf-c++.hpp=/usr/include

MAN=		atf-c++-api.3

.if ${MKSHARE} != "no"
FILES+=		atf-c++.pc
FILESDIR=	/usr/lib/pkgconfig

realall: atf-c++.pc
atf-c++.pc: Makefile atf-c++.pc.in atf-version.txt
	${TOOL_SED} -e "s,__ATF_VERSION__,$$(cat atf-version.txt),g" \
	    -e 's,__CXX__,g++,g' \
	    -e 's,__INCLUDEDIR__,/usr/include,g' \
	    -e 's,__LIBDIR__,/usr/lib,g' \
	    <${SRCDIR}/atf-c++/atf-c++.pc.in >atf-c++.pc
CLEANFILES+=	atf-c++.pc
.endif

.include <bsd.lib.mk>
