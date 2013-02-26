# $NetBSD: Makefile,v 1.2 2013/02/22 15:14:31 jmmv Exp $

#NOLINT=		# defined

.include <bsd.own.mk>

LIB=		lutok
LIBISCXX=	yes

LIBDPLIBS+=     lua	${.CURDIR}/../../../../mit/lua/lib/liblua
.if ${HAVE_GCC} == 4
LIBDPLIBS+=     stdc++	${.CURDIR}/../../../../../gnu/lib/libstdc++-v3_4
.else
LIBDPLIBS+=     stdc++	${.CURDIR}/../../../../../external/gpl3/gcc/lib/libstdc++-v3
.endif
LIBDPLIBS+=     m	${.CURDIR}/../../../../../lib/libm

SRCDIR=		${NETBSDSRCDIR}/external/bsd/lutok/dist
.PATH:		${SRCDIR}

CPPFLAGS+=	-I${.CURDIR}
CPPFLAGS+=	-I.

CPPFLAGS+=	-DHAVE_CONFIG_H

WARNS?=		4

SRCS=		c_gate.cpp \
		debug.cpp \
		exceptions.cpp \
		operations.cpp \
		stack_cleaner.cpp \
		state.cpp

INCS=		c_gate.hpp \
		debug.hpp \
		exceptions.hpp \
		operations.hpp \
		stack_cleaner.hpp \
		state.hpp \
		state.ipp \
		test_utils.hpp
INCSDIR=	/usr/include/lutok

.if ${MKSHARE} != "no"
FILES+=		lutok.pc
FILESDIR=	/usr/lib/pkgconfig

realall: lutok.pc
lutok.pc: Makefile lutok.pc.in
	${TOOL_SED} \
	    -e 's,__INCLUDEDIR__,/usr/include,g' \
	    -e 's,__LIBDIR__,/usr/lib,g' \
	    -e 's,__LUA_CFLAGS__,-I/usr/include,g' \
	    -e 's,__LUA_LIBS,-llua,g' \
	    -e 's,__VERSION__,0.2,g' \
	    <${SRCDIR}/lutok.pc.in >lutok.pc
CLEANFILES+=	lutok.pc
.endif

.include <bsd.lib.mk>
