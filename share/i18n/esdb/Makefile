# $NetBSD: Makefile,v 1.15 2011/04/27 20:05:56 mbalmer Exp $

NOMAN=

.include <bsd.own.mk>

BINDIR?=	/usr/share/i18n/esdb

.SUFFIXES: .src .esdb

.src.esdb:
	${_MKTARGET_CREATE}
	${TOOL_MKESDB} -o $@ $>

SUBDIR=	APPLE AST BIG5 CBM CP DEC EUC EBCDIC GB GEORGIAN ISO-2022 \
	ISO-8859 ISO646 KAZAKH KOI MISC TCVN UTF

.for i in ${SUBDIR}
.include "${.CURDIR}/$i/Makefile.inc"
.endfor

# ----------------------------------------------------------------------
# esdb.{dir,alias}
#

esdb.dir: ${SRC_esdb.dir}
	${_MKTARGET_CREATE}
	cat $> > $@
esdb.dir.db: esdb.dir
	${_MKTARGET_CREATE}
	${TOOL_MKESDB} -m -o $@ $>

esdb.alias: ${SRC_esdb.alias}
	${_MKTARGET_CREATE}
	cat $> > $@
esdb.alias.db: esdb.alias
	${_MKTARGET_CREATE}
	${TOOL_MKESDB} -m -o $@ $>

FILES+=		esdb.dir esdb.dir.db esdb.alias esdb.alias.db
CLEANFILES+=	esdb.dir esdb.dir.db esdb.alias esdb.alias.db
FILESDIR:=	${BINDIR}

all: ${FILES}
realall: ${FILES}

.include <bsd.prog.mk>
