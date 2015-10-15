# $NetBSD: Makefile,v 1.21 2014/03/02 22:50:13 jmmv Exp $

NOLINT=		# defined

.include <bsd.init.mk>

LIB=		atf-c

CWARNFLAGS+=	-Wno-format
CWARNFLAGS+=	-Wno-missing-noreturn

.PATH:		${SRCDIR}
.PATH:		${SRCDIR}/atf-c
.PATH:		${SRCDIR}/atf-c/detail
.PATH:		${TOPDIR}

config.o:	Makefile

WARNS?=		4

SRCS=		build.c \
		check.c \
		config.c \
		dynstr.c \
		env.c \
		error.c \
		fs.c \
		list.c \
		map.c \
		process.c \
		sanity.c \
		text.c \
		user.c \
		utils.c \
		tc.c \
		tp.c \
		tp_main.c

INCS=		build.h \
		check.h \
		config.h \
		defs.h \
		error.h \
		error_fwd.h \
		macros.h \
		tc.h \
		tp.h \
		utils.h
INCSDIR=	/usr/include/atf-c

INCS+=		atf-c.h
INCSDIR_atf-c.h=/usr/include

MAN=		atf-c-api.3

.if ${MKSHARE} != "no"
FILES+=		atf-c.pc
FILESDIR=	/usr/lib/pkgconfig

realall: atf-c.pc
atf-c.pc: Makefile atf-c.pc.in atf-version.txt
	${TOOL_SED} -e "s,__ATF_VERSION__,$$(cat atf-version.txt),g" \
	    -e 's,__CC__,gcc,g' \
	    -e 's,__INCLUDEDIR__,/usr/include,g' \
	    -e 's,__LIBDIR__,/usr/lib,g' \
	    <${SRCDIR}/atf-c/atf-c.pc.in >atf-c.pc
CLEANFILES+=	atf-c.pc
.endif

.include <bsd.lib.mk>
