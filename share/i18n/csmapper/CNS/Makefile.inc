# $NetBSD: Makefile.inc,v 1.3 2007/03/05 16:57:14 tnozaki Exp $

.PATH: ${.CURDIR}/CNS

SRCS_mapper.dir+=	mapper.dir.CNS
SRCS_charset.pivot+=	charset.pivot.CNS
CLEANFILES+=		mapper.dir.CNS charset.pivot.CNS
MAPS_CNS=		CNS11643-1%UCS.mps \
			CNS11643-2%UCS.mps \
			CNS11643-3%UCS@BMP.mps CNS11643-3%UCS@SIP.mps \
			CNS11643-4%UCS@BMP.mps CNS11643-4%UCS@SIP.mps \
			CNS11643-5%UCS@BMP.mps CNS11643-5%UCS@SIP.mps \
			CNS11643-6%UCS@BMP.mps CNS11643-6%UCS@SIP.mps \
			CNS11643-7%UCS@BMP.mps CNS11643-7%UCS@SIP.mps \
			UCS%CNS11643-1.mps \
			UCS%CNS11643-2.mps \
			UCS@BMP%CNS11643-3.mps UCS@SIP%CNS11643-3.mps \
			UCS@BMP%CNS11643-4.mps UCS@SIP%CNS11643-4.mps \
			UCS@BMP%CNS11643-5.mps UCS@SIP%CNS11643-5.mps \
			UCS@BMP%CNS11643-6.mps UCS@SIP%CNS11643-6.mps \
			UCS@BMP%CNS11643-7.mps UCS@SIP%CNS11643-7.mps

mapper.dir.CNS: ${.CURDIR}/CNS/mapper.dir.CNS.src
	${_MKTARGET_CREATE}
	(echo "# CNS" ; cat ${.ALLSRC} ; echo ) > ${.TARGET}

charset.pivot.CNS: ${.CURDIR}/CNS/charset.pivot.CNS.src
	${_MKTARGET_CREATE}
	(echo "# CNS" ; cat ${.ALLSRC} ; echo ) > ${.TARGET}

FILES+= ${MAPS_CNS}
CLEANFILES+= ${MAPS_CNS}
.for i in ${MAPS_CNS}
FILESDIR_$i= ${BINDIR}/CNS
.endfor
