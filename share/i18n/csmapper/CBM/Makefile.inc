# $NetBSD: Makefile.inc,v 1.1 2011/04/27 20:05:56 mbalmer Exp $

.PATH: ${.CURDIR}/CBM

SRCS_mapper.dir+=	mapper.dir.CBM
SRCS_charset.pivot+=	charset.pivot.CBM
CLEANFILES+=		mapper.dir.CBM charset.pivot.CBM
MAPS_CBM=		CBMINT%UCS.mps \
			UCS%CBMINT.mps

mapper.dir.CBM: ${.CURDIR}/CBM/mapper.dir.CBM.src
	${_MKTARGET_CREATE}
	(echo "# CBM" ; cat ${.ALLSRC} ; echo ) > ${.TARGET}

charset.pivot.CBM: ${.CURDIR}/CBM/charset.pivot.CBM.src
	${_MKTARGET_CREATE}
	( echo "# CBM" ; cat ${.ALLSRC} ; echo ) > ${.TARGET}

FILES+=		${MAPS_CBM}
CLEANFILES+=	${MAPS_CBM}
.for i in ${MAPS_CBM}
FILESDIR_$i=${BINDIR}/CBM
.endfor

