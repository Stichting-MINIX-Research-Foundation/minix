#	$NetBSD: Makefile,v 1.12 2012/12/02 12:48:33 apb Exp $

.include <bsd.own.mk>

.-include	"${TOOLDIR}/share/compat/defs.mk"

TIMESTAMP=	${TOOLDIR}/bin/${_TOOL_PREFIX}lorder

CLEANFILES+=	lorder

.PATH: ${.CURDIR}/../../usr.bin/lorder

.include <bsd.hostprog.mk>

install: ${TIMESTAMP}
${TIMESTAMP}: lorder
	${HOST_INSTALL_FILE} -m ${BINMODE} ${.ALLSRC} ${.TARGET}

lorder: lorder.sh
	${TOOL_SED} -e "s,/bin/sh,"${HOST_BSHELL:Q}",g" \
	    -e "s,{JOIN:=.*},{JOIN:="${TOOL_JOIN:Q}"}," \
	    -e "s,{MKTEMP:=.*},{MKTEMP:="${TOOL_MKTEMP:Q}"}," \
	    -e "s,{NM:=.*},{NM:="${NM:Q}"}," \
	    -e "s,{SED:=.*},{SED:="${TOOL_SED:Q}"}," \
	    < ${.ALLSRC} > ${.TARGET}

realall: lorder
