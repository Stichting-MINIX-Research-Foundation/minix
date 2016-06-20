# $NetBSD: Makefile.inc,v 1.4 2006/10/18 17:54:55 tnozaki Exp $

.PATH: ${.CURDIR}/KS

SRCS_mapper.dir+=	mapper.dir.KS
SRCS_charset.pivot+=	charset.pivot.KS
CLEANFILES+=		mapper.dir.KS charset.pivot.KS
MAPS_KS=		JOHAB%UCS.mps \
			KSC5601HANGUL%UCS.mps \
			KSC5601HANJA%UCS.mps \
			UCS%JOHAB.mps \
			UCS%KSC5601HANGUL.mps \
			UCS%KSC5601HANJA.mps

mapper.dir.KS: ${.CURDIR}/KS/mapper.dir.KS.src
	${_MKTARGET_CREATE}
	(echo "# KS" ; cat ${.ALLSRC} ; echo ) > ${.TARGET}

charset.pivot.KS: ${.CURDIR}/KS/charset.pivot.KS.src
	${_MKTARGET_CREATE}
	(echo "# KS" ; cat ${.ALLSRC} ; echo ) > ${.TARGET}

FILES+= ${MAPS_KS}
CLEANFILES+= ${MAPS_KS}
.for i in ${MAPS_KS}
FILESDIR_$i= ${BINDIR}/KS
.endfor
