# $NetBSD: Makefile.inc,v 1.2 2008/10/25 22:27:36 apb Exp $

.PATH: ${.CURDIR}/GEORGIAN

CODE:=		GEORGIAN
SEP_GEORGIAN:=	-
.include "${.CURDIR}/Makefile.part"

.for i in ${GEORGIAN_PART}
GEORGIAN-${i}.src: GEORGIAN.src
	${_MKTARGET_CREATE}
	${TOOL_SED} -e 's/GEORGIANx/GEORGIAN-${i}/' \
	    -e 's/GEORGIANy/GEORGIAN-${i:S/-/:/}/' $> > $@
CLEANFILES+= GEORGIAN-${i}.src
.endfor
