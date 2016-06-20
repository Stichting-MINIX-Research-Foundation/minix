# $NetBSD: Makefile.inc,v 1.3 2008/10/25 22:35:36 apb Exp $

.PATH: ${.CURDIR}/KOI

SRCS_mapper.dir+=	mapper.dir.KOI
SRCS_charset.pivot+=	charset.pivot.KOI
CLEANFILES+=		mapper.dir.KOI charset.pivot.KOI
MAPS_KOI=		GOST19768-74%UCS.mps	\
			UCS%GOST19768-74.mps	\
			ISO-5427%UCS.mps	\
			UCS%ISO-5427.mps	\
			KOI7%UCS.mps		\
			UCS%KOI7.mps
PART_KOI8!=		${TOOL_SED} '/^\#/d;/^$$/d;' ${.CURDIR}/KOI/KOI8.part

mapper.dir.KOI: ${.CURDIR}/KOI/KOI8.part mapper.dir.KOI.src
	${_MKTARGET_CREATE}
	( \
	    cat ${.CURDIR}/KOI/mapper.dir.KOI.src; \
	    for i in ${PART_KOI8}; do \
		printf "%-31s %-31s %s\n" KOI$$i"EXT/UCS" \
			mapper_std KOI/KOI$$i"%UCS.mps"; \
		printf "%-31s %-31s %s\n" UCS/KOI$$i"EXT" \
			mapper_std KOI/UCS%KOI$$i".mps"; \
	    done; \
	    for i in ${PART_KOI8}; do \
		printf "%-31s %-31s %s\n" KOI$$i"/UCS" \
			mapper_parallel GOST19768-74/UCS,KOI$$i"EXT/UCS"; \
		printf "%-31s %-31s %s\n" UCS/KOI$$i \
			mapper_parallel UCS/GOST19768-74,UCS/KOI$$i"EXT"; \
	    done; \
	    echo; \
	) > ${.TARGET}

charset.pivot.KOI: ${.CURDIR}/KOI/KOI8.part charset.pivot.KOI.src
	${_MKTARGET_CREATE}
	( \
	    cat ${.CURDIR}/KOI/charset.pivot.KOI.src; \
	    for i in ${PART_KOI8}; do \
		printf "%-31s %-31s%d\n" KOI$$i UCS 1; \
		printf "%-31s %-31s%d\n" UCS KOI$$i 1; \
	    done; \
	    echo; \
	) > ${.TARGET}

FILES+=		${MAPS_KOI}
CLEANFILES+=	${MAPS_KOI}
.for i in ${MAPS_KOI}
FILESDIR_$i=	${BINDIR}/KOI
.endfor

.for i in ${PART_KOI8}
FILES+=		KOI${i:S/:/@/}%UCS.mps UCS%KOI${i:S/:/@/}.mps
CLEANFILES+=	KOI${i:S/:/@/}%UCS.mps UCS%KOI${i:S/:/@/}.mps
FILESDIR_KOI${i:S/:/@/}%UCS.mps=	${BINDIR}/KOI
FILESDIR_UCS%KOI${i:S/:/@/}.mps=	${BINDIR}/KOI
.endfor
