#	$NetBSD: Makefile,v 1.16 2011/06/21 13:25:45 joerg Exp $

# LSC Until it compiles cleanly...
NOGCCERROR:=yes

USE_FORT?= yes	# data-driven bugs?

PROG=		gzip
MAN=		gzip.1 gzexe.1 zdiff.1 zforce.1 zmore.1 znew.1

DPADD=		${LIBZ} ${LIBBZ2} ${LIBLZMA}
LDADD=		-lz -lbz2 -llzma

SCRIPTS=	gzexe zdiff zforce zmore znew

MLINKS+=	gzip.1 gunzip.1 \
		gzip.1 gzcat.1 \
		gzip.1 zcat.1 \
		zdiff.1 zcmp.1 

LINKS+=		${BINDIR}/gzip ${BINDIR}/gunzip \
		${BINDIR}/gzip ${BINDIR}/gzcat \
		${BINDIR}/gzip ${BINDIR}/zcat \
		${BINDIR}/zdiff ${BINDIR}/zcmp 

.include <bsd.prog.mk>
