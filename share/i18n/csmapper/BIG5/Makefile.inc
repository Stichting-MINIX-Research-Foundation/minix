# $NetBSD: Makefile.inc,v 1.2 2007/03/05 16:57:10 tnozaki Exp $

.PATH: ${.CURDIR}/BIG5

SRCS_mapper.dir+=	mapper.dir.BIG5
SRCS_charset.pivot+=	charset.pivot.BIG5
CLEANFILES+=		mapper.dir.BIG5 charset.pivot.BIG5
MAPS_BIG5=		Big5@1984%UCS.mps \
			Big5EXT@2003%UCS.mps \
			Big5EXT@E%UCS.mps \
			Big5EXT@IBM%UCS.mps \
			Big5EXT@Plus%UCS.mps \
			Big5UDA%UCS.mps \
			ETen%UCS@BMP.mps ETen%UCS@SIP.mps \
			HKSCS%UCS@BMP.mps HKSCS%UCS@SIP.mps \
			UCS%Big5@1984.mps \
			UCS%Big5EXT@2003.mps \
			UCS%Big5EXT@E.mps \
			UCS%Big5EXT@IBM.mps \
			UCS%Big5EXT@Plus.mps \
			UCS%Big5UDA.mps \
			UCS@BMP%ETen.mps UCS@SIP%ETen.mps \
			UCS@BMP%HKSCS.mps UCS@SIP%HKSCS.mps

mapper.dir.BIG5: ${.CURDIR}/BIG5/mapper.dir.BIG5.src
	${_MKTARGET_CREATE}
	(echo "# BIG5" ; cat ${.ALLSRC} ; echo ) > ${.TARGET}

charset.pivot.BIG5: ${.CURDIR}/BIG5/charset.pivot.BIG5.src
	${_MKTARGET_CREATE}
	(echo "# BIG5" ; cat ${.ALLSRC} ; echo ) > ${.TARGET}

FILES+= ${MAPS_BIG5}
CLEANFILES+= ${MAPS_BIG5}
.for i in ${MAPS_BIG5}
FILESDIR_$i= ${BINDIR}/BIG5
.endfor
