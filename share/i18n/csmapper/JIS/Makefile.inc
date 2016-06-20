# $NetBSD: Makefile.inc,v 1.6 2007/03/05 16:58:33 tnozaki Exp $

.PATH: ${.CURDIR}/JIS

SRCS_mapper.dir+=	mapper.dir.JIS
SRCS_charset.pivot+=	charset.pivot.JIS
CLEANFILES+=		mapper.dir.JIS charset.pivot.JIS
MAPS_JIS=		JISX0201-KANA%UCS.mps \
			JISX0208@1978%UCS.mps \
			JISX0208@1990%UCS.mps \
			JISX0212%UCS.mps \
			JISX0208@MS%UCS.mps \
			JISX0208VDC@NEC%UCS.mps \
			JISX0208UDC%UCS.mps \
			JISX0212@MS%UCS.mps \
			JISX0212VDC@IBM%UCS.mps \
			JISX0212UDC%UCS.mps \
			JISX0213-1%UCS@BMP.mps JISX0213-1%UCS@SIP.mps \
			JISX0213-2%UCS@BMP.mps JISX0213-2%UCS@SIP.mps \
			UCS%JISX0201-KANA.mps \
			UCS%JISX0208@1978.mps \
			UCS%JISX0208@1990.mps \
			UCS%JISX0212.mps \
			UCS%JISX0208@MS.mps \
			UCS%JISX0208VDC@NEC.mps \
			UCS%JISX0208UDC.mps \
			UCS%JISX0212@MS.mps \
			UCS%JISX0212VDC@IBM.mps \
			UCS%JISX0212UDC.mps \
			UCS@BMP%JISX0213-1.mps UCS@SIP%JISX0213-1.mps \
			UCS@BMP%JISX0213-2.mps UCS@SIP%JISX0213-2.mps

mapper.dir.JIS: ${.CURDIR}/JIS/mapper.dir.JIS.src
	${_MKTARGET_CREATE}
	(echo "# JIS" ; cat ${.ALLSRC} ; echo ) > ${.TARGET}

charset.pivot.JIS: ${.CURDIR}/JIS/charset.pivot.JIS.src
	${_MKTARGET_CREATE}
	(echo "# JIS" ; cat ${.ALLSRC} ; echo ) > ${.TARGET}

FILES+= ${MAPS_JIS}
CLEANFILES+= ${MAPS_JIS}
.for i in ${MAPS_JIS}
FILESDIR_$i= ${BINDIR}/JIS
.endfor
