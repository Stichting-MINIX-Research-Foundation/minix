# $NetBSD: Makefile,v 1.4 2013/10/18 23:36:10 jmmv Exp $

#NOLINT=		# defined

.include <bsd.own.mk>

LIB=		lutok
LIBISCXX=	yes

LIBDPLIBS+=     lua	${.CURDIR}/../../../../mit/lua/lib/liblua
LIBDPLIBS+=     m	${.CURDIR}/../../../../../lib/libm

SRCDIR=		${NETBSDSRCDIR}/external/bsd/lutok/dist
.PATH:		${SRCDIR}

CPPFLAGS+=	-I${.CURDIR}
CPPFLAGS+=	-I.

CPPFLAGS+=	-DHAVE_CONFIG_H

WARNS?=		4

.if defined(__MINIX)
NOCLANGERROR:= yes
.endif # defined(__MINIX)

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
	    -e 's,__VERSION__,0.3,g' \
	    <${SRCDIR}/lutok.pc.in >lutok.pc
CLEANFILES+=	lutok.pc
.endif

.include <bsd.lib.mk>
