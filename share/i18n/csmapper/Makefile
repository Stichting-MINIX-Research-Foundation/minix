# $NetBSD: Makefile,v 1.14 2011/04/27 20:05:56 mbalmer Exp $

NOMAN=

.include <bsd.own.mk>

.SUFFIXES: .src .646 .mps

BINDIR?= /usr/share/i18n/csmapper
SRCS_mapper.dir+= mapper.dir.src
SRCS_charset.pivot+= charset.pivot.src

SUBDIR= APPLE AST BIG5 CBM CNS CP EBCDIC GB GEORGIAN ISO646 ISO-8859 \
	JIS KAZAKH KOI KS MISC TCVN
.for i in ${SUBDIR}
.if exists(${.CURDIR}/$i/Makefile.inc)
.include "${.CURDIR}/$i/Makefile.inc"
.endif
.endfor

# ----------------------------------------------------------------------

mapper.dir: ${SRCS_mapper.dir}
	${_MKTARGET_CREATE}
	cat $> > $@
mapper.dir.db: mapper.dir
	${_MKTARGET_CREATE}
	${TOOL_MKCSMAPPER} -m -o $@ $>
FILES+= mapper.dir mapper.dir.db
CLEANFILES+= mapper.dir mapper.dir.db

charset.pivot: ${SRCS_charset.pivot}
	${_MKTARGET_CREATE}
	cat $> > $@
charset.pivot.pvdb: charset.pivot
	${_MKTARGET_CREATE}
	${TOOL_MKCSMAPPER} -p -o $@ $>
FILES+= charset.pivot charset.pivot.pvdb
CLEANFILES+= charset.pivot charset.pivot.pvdb

.src.mps:
	${_MKTARGET_CREATE}
	${TOOL_MKCSMAPPER} -o $@ $>

all: ${FILES}
realall: ${FILES}

.include <bsd.prog.mk>
