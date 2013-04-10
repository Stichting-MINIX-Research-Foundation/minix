#	$NetBSD: Makefile,v 1.13 2008/09/03 11:58:30 lukem Exp $

.include "../Makefile.inc"
LIBISPRIVATE=	yes

LIB=		common
SRCS+=		substring.c xexit.c xmalloc.c xstrdup.c key.c doc.c

CPPFLAGS+=	-DGNULOCALEDIR=\"/usr/share/locale\" \
		-DLOCALE_ALIAS_PATH=\"/usr/share/locale:.\" \
		-I${IDIST}/info

.include <bsd.lib.mk>

.PATH: ${IDIST}/lib ${IDIST}/po

all: ${FILES}
.SUFFIXES: .gmo .mo
.gmo.mo:
	${_MKTARGET_CREATE}
	@cp $< $@

GEN_SRCS =	funs.h key.c doc.c
CLEANFILES+=	${GEN_SRCS}

CMD_SRCS = session.c echo-area.c infodoc.c m-x.c indices.c nodemenu.c \
    footnotes.c variables.c

MAKEDOCDIR!=cd ${.CURDIR}/../makedoc && ${PRINTOBJDIR}
MAKEDOC=${MAKEDOCDIR}/makedoc

${GEN_SRCS}: ${MAKEDOC} ${CMD_SRCS}
	${_MKMSG_CREATE} ${GEN_SRCS}
	${.ALLSRC}

.PATH: ${IDIST}/info ${IDIST}/doc
