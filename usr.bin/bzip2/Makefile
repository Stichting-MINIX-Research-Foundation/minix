#	$NetBSD: Makefile,v 1.12 2012/05/07 08:51:47 wiz Exp $

.include <bsd.own.mk>		# for MKDYNAMICROOT definition

USE_FORT?= yes	# data-driven bugs?


PROG=	   bzip2
LDDIR!=	   cd ${NETBSDSRCDIR}/lib/libbz2 && ${PRINTOBJDIR}
LDADD+=	   -L${LDDIR} -lbz2
DPADD+=	   ${LIBBZ2}

.if (${MKDYNAMICROOT} == "no")
LDSTATIC?= -static
.endif
.ifdef	SMALLPROG
CPPFLAGS+=	-DSMALL
.endif

DIST=	   ${NETBSDSRCDIR}/external/bsd/bzip2/dist
.PATH:	   ${DIST}

MLINKS+=   bzip2.1 bunzip2.1\
	   bzip2.1 bzcat.1\
	   bzip2.1 bzip2recover.1

LINKS+=    ${BINDIR}/bzip2 ${BINDIR}/bunzip2
LINKS+=    ${BINDIR}/bzip2 ${BINDIR}/bzcat

.include <bsd.prog.mk>
