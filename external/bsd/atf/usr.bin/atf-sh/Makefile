# $NetBSD: Makefile,v 1.12 2014/03/02 22:50:13 jmmv Exp $

.include <bsd.init.mk>

.PATH:		${SRCDIR}/atf-sh
.PATH:		${TOPDIR}

BINDIR=		/usr/bin
PROG_CXX=	atf-sh
SRCS=		atf-sh.cpp
MAN=		atf-sh.1 atf-sh-api.3

LDADD+=		-latf-c++ -latf-c
DPADD+=		${LIBATF_CXX} ${LIBATF_C}

WARNS?=		2

.if ${MKSHARE} != "no"
FILESMODE=	444

FILES+=				libatf-sh.subr
FILESDIR_libatf-sh.subr=	/usr/share/atf

FILES+=			atf-sh.pc
FILESDIR_atf-sh.pc=	/usr/lib/pkgconfig

realall: atf-sh.pc
atf-sh.pc: Makefile atf-sh.pc.in atf-version.txt
	${TOOL_SED} -e "s,__ATF_VERSION__,$$(cat atf-version.txt),g" \
	    -e 's,__EXEC_PREFIX__,/usr,g' \
	    <${SRCDIR}/atf-sh/atf-sh.pc.in >atf-sh.pc
CLEANFILES+=	atf-sh.pc
.endif

.include <bsd.prog.mk>
