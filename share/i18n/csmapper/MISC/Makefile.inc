# $NetBSD: Makefile.inc,v 1.13 2007/04/01 18:52:29 tnozaki Exp $

.PATH: ${.CURDIR}/MISC

SRCS_mapper.dir+=	mapper.dir.MISC
SRCS_charset.pivot+=	charset.pivot.MISC
CLEANFILES+=		mapper.dir.MISC charset.pivot.MISC
MAPS_MISC=		ATARIST%UCS.mps \
			DECMCS%UCS.mps \
			HP-ROMAN8%UCS.mps \
			MULELAO-1%UCS.mps \
			NEXTSTEP%UCS.mps \
			RISCOS-LATIN1%UCS.mps \
			TDS565%UCS.mps \
			UCS%ATARIST.mps \
			UCS%DECMCS.mps \
			UCS%HP-ROMAN8.mps \
			UCS%MULELAO-1.mps \
			UCS%NEXTSTEP.mps \
			UCS%RISCOS-LATIN1.mps \
			UCS%TDS565.mps

mapper.dir.MISC: ${.CURDIR}/MISC/mapper.dir.MISC.src
	${_MKTARGET_CREATE}
	(echo "# MISC" ; cat ${.ALLSRC} ; echo ) > ${.TARGET}

charset.pivot.MISC: ${.CURDIR}/MISC/charset.pivot.MISC.src
	${_MKTARGET_CREATE}
	(echo "# MISC" ; cat ${.ALLSRC} ; echo ) > ${.TARGET}

FILES+= ${MAPS_MISC}
CLEANFILES+= ${MAPS_MISC}
.for i in ${MAPS_MISC}
FILESDIR_$i= ${BINDIR}/MISC
.endfor
