# $NetBSD: Makefile.inc,v 1.7 2006/11/23 03:25:24 tnozaki Exp $

.PATH: ${.CURDIR}/GB

SRCS_mapper.dir+=	mapper.dir.GB
SRCS_charset.pivot+=	charset.pivot.GB
CLEANFILES+=		mapper.dir.GB charset.pivot.GB
MAPS_GB=		GB12345%UCS.mps \
			GB18030%UCS@BMP.mps \
			GB2312%UCS.mps \
			GB2312EXT%UCS.mps \
			GB2312UDA1%UCS.mps \
			GB2312UDA2%UCS.mps \
			GBKEXT%UCS.mps \
			GBKUDA%UCS.mps \
			ISO-IR-165EXT%UCS.mps \
			UCS@BMP%GB18030.mps \
			UCS%GB12345.mps \
			UCS%GB2312.mps \
			UCS%GB2312EXT.mps \
			UCS%GB2312UDA1.mps \
			UCS%GB2312UDA2.mps \
			UCS%GBKEXT.mps \
			UCS%GBKUDA.mps \
			UCS%ISO-IR-165EXT.mps

mapper.dir.GB: ${.CURDIR}/GB/mapper.dir.GB.src
	${_MKTARGET_CREATE}
	(echo "# GB" ; cat ${.ALLSRC} ; echo ) > ${.TARGET}

charset.pivot.GB: ${.CURDIR}/GB/charset.pivot.GB.src
	${_MKTARGET_CREATE}
	(echo "# GB" ; cat ${.ALLSRC} ; echo ) > ${.TARGET}

FILES+= ${MAPS_GB}
CLEANFILES+= ${MAPS_GB}
.for i in ${MAPS_GB}
FILESDIR_$i= ${BINDIR}/GB
.endfor
