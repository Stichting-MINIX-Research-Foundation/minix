#	$NetBSD: Makefile,v 1.10 2007/05/28 12:06:24 tls Exp $

.include <bsd.own.mk>		# for MKDYNAMICROOT definition

.if defined(__MINIX)
MKDYNAMICROOT= no
SMALLPROG= yes
USE_FORT=no		# __SSP_FORTIFY_LEVEL=0
.else
USE_FORT?= yes	# data-driven bugs?
.endif

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

DIST=	   ${NETBSDSRCDIR}/dist/bzip2
.PATH:	   ${DIST}

MLINKS+=   bzip2.1 bunzip2.1\
	   bzip2.1 bzcat.1\
	   bzip2.1 bzip2recover.1

LINKS+=    ${BINDIR}/bzip2 ${BINDIR}/bunzip2
LINKS+=    ${BINDIR}/bzip2 ${BINDIR}/bzcat

test:
	make -C dist/bzip2 test
install-extra:
	make -C dist/bzip2 install
.include <bsd.prog.mk>
