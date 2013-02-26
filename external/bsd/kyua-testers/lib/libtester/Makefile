# $NetBSD: Makefile,v 1.1 2013/02/19 06:04:42 jmmv Exp $

#NOLINT=		# defined

.include <bsd.own.mk>

LIB=		tester
LIBISPRIVATE=	yes

SRCDIR=		${NETBSDSRCDIR}/external/bsd/kyua-testers/dist
.PATH:		${SRCDIR}

CPPFLAGS+=	-DGDB=\"/usr/bin/gdb\"
CPPFLAGS+=	-DKYUA_TMPDIR=\"/tmp\"
CPPFLAGS+=	-I${.CURDIR}	# For config.h and defs.h.
CPPFLAGS+=	-I${SRCDIR}	# For non-installed headers.

CPPFLAGS+=	-DHAVE_CONFIG_H

WARNS?=		3

SRCS=		cli.c
SRCS+=		env.c
SRCS+=		error.c
SRCS+=		fs.c
SRCS+=		result.c
SRCS+=		run.c
SRCS+=		stacktrace.c
SRCS+=		text.c

.if ${MKSHARE} != "no"
FILES+=		kyua-testers.pc
FILESDIR=	/usr/lib/pkgconfig

realall: kyua-testers.pc
kyua-testers.pc: Makefile kyua-testers.pc.in
	${TOOL_SED} \
	    -e 's,__LIBEXECDIR__,/usr/libexec,g' \
	    -e 's,__VERSION__,0.1,g' \
	    <${SRCDIR}/kyua-testers.pc.in >kyua-testers.pc
CLEANFILES+=	kyua-testers.pc
.endif

.include <bsd.files.mk>
.include <bsd.lib.mk>
