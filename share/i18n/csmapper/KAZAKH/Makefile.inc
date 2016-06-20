# $NetBSD: Makefile.inc,v 1.1 2007/03/13 16:34:37 tnozaki Exp $

.PATH: ${.CURDIR}/KAZAKH

SRCS_mapper.dir+=	mapper.dir.KAZAKH
SRCS_charset.pivot+=	charset.pivot.KAZAKH
CLEANFILES+=		mapper.dir.KAZAKH charset.pivot.KAZAKH
MAPS_KAZAKH=		KZ1048%UCS.mps \
			PTCP154%UCS.mps \
			UCS%KZ1048.mps \
			UCS%PTCP154.mps

mapper.dir.KAZAKH: ${.CURDIR}/KAZAKH/mapper.dir.KAZAKH.src
	${_MKTARGET_CREATE}
	(echo "# KAZAKH" ; cat ${.ALLSRC} ; echo ) > ${.TARGET}

charset.pivot.KAZAKH: ${.CURDIR}/KAZAKH/charset.pivot.KAZAKH.src
	${_MKTARGET_CREATE}
	(echo "# KAZAKH" ; cat ${.ALLSRC} ; echo ) > ${.TARGET}

FILES+= ${MAPS_KAZAKH}
CLEANFILES+= ${MAPS_KAZAKH}
.for i in ${MAPS_KAZAKH}
FILESDIR_$i= ${BINDIR}/KAZAKH
.endfor
