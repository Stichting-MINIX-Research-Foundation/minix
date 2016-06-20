# $NetBSD: Makefile.inc,v 1.1 2006/11/13 15:16:30 tnozaki Exp $

.PATH: ${.CURDIR}/TCVN

SRCS_mapper.dir+=	mapper.dir.TCVN
SRCS_charset.pivot+=	charset.pivot.TCVN
CLEANFILES+=		mapper.dir.TCVN charset.pivot.TCVN
MAPS_TCVN=		TCVN5712-1%UCS.mps \
			VISCII%UCS.mps \
			UCS%TCVN5712-1.mps \
			UCS%VISCII.mps

mapper.dir.TCVN: ${.CURDIR}/TCVN/mapper.dir.TCVN.src
	${_MKTARGET_CREATE}
	(echo "# TCVN" ; cat ${.ALLSRC} ; echo ) > ${.TARGET}

charset.pivot.TCVN: ${.CURDIR}/TCVN/charset.pivot.TCVN.src
	${_MKTARGET_CREATE}
	(echo "# TCVN" ; cat ${.ALLSRC} ; echo ) > ${.TARGET}

FILES+= ${MAPS_TCVN}
CLEANFILES+= ${MAPS_TCVN}
.for i in ${MAPS_TCVN}
FILESDIR_$i= ${BINDIR}/TCVN
.endfor
