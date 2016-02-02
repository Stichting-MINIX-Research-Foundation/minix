# $NetBSD: Makefile,v 1.3 2011/04/15 21:02:47 elric Exp $

NOMAN=		# defined

.include <bsd.own.mk>
.include <${.CURDIR}/../../../Makefile.inc>

.PATH:		${HEIMDIST}/lib/sl ${HEIMDIST}/lib/roken ${HEIMDIST}/lib/vers

PROG=slc

SRCS+=	slc-gram.y slc-lex.l getarg.c rand.c get_window_size.c \
	strupr.c print_version.c

YHEADER=	# defined

HOST_CPPFLAGS+= -I.			\
	-I${HEIMBASE}/include		\
	-I${HEIMBASE}/include/krb5	\
	-I${HEIMDIST}/lib/sl		\
	-I${HEIMDIST}/lib/roken		\
	-I${HEIMDIST}/lib/vers		\
	-DHAVE_CONFIG_H

DPSRCS=		print_version.h

make-print-version.lo: ${HEIMBASE}/include/version.h

make-print-version: make-print-version.lo
	${HOST_CC} ${HOST_LDFLAGS} -o ${.TARGET} ${.ALLSRC}

print_version.h: make-print-version
	./make-print-version print_version.h

.NOPATH: print_version.h make-print-version

beforedepend:
	[ -h krb5 ] || ln -sf . krb5
	ln -sf ${HEIMBASE}/include/roken.h
	ln -sf ${HEIMBASE}/dist/lib/roken/roken-common.h
	ln -sf ${HEIMBASE}/dist/lib/roken/getarg.h

CLEANFILES+=	krb5 roken.h roken-common.h getarg.h	\
		print_version.h make-print-version	\
		make-print-version.lo

# HOSTPROG will be set already if this is a src/tools build.
.ifndef HOSTPROG
HOSTPROG=	${PROG}
.include <bsd.hostprog.mk>
.endif
