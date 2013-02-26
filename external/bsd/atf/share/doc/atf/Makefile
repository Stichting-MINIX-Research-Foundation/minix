# $NetBSD: Makefile,v 1.4 2010/11/02 14:41:32 njoly Exp $

.include <bsd.own.mk>

SRCDIR=	${NETBSDSRCDIR}/external/bsd/atf/dist
.PATH:	${SRCDIR}
.PATH:	${SRCDIR}/atf-c
.PATH:	${SRCDIR}/atf-c++
.PATH:	${SRCDIR}/atf-sh
.PATH:	${SRCDIR}/doc

.if ${MKDOC} != "no"
FILESDIR=	/usr/share/doc/atf
FILES=		AUTHORS COPYING NEWS README
.endif

MAN=	atf.7 atf-formats.5 atf-test-case.4 atf-test-program.1

CLEANFILES+=	atf.7 atf.7.tmp
atf.7: atf.7.in
	${TOOL_SED} \
	    -e 's,__DOCDIR__,/usr/share/doc/atf,g' \
	    -e 's,__TESTSDIR__,/usr/tests,g' \
	    <${SRCDIR}/doc/atf.7.in >atf.7.tmp
	mv atf.7.tmp atf.7

.include <bsd.files.mk>
.include <bsd.man.mk>
