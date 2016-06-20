# $NetBSD: Makefile.inc,v 1.2 2008/10/25 22:35:36 apb Exp $

.PATH: ${.CURDIR}/APPLE

SRCS_mapper.dir+=	mapper.dir.APPLE
SRCS_charset.pivot+=	charset.pivot.APPLE
CLEANFILES+=		mapper.dir.APPLE charset.pivot.APPLE
PART_MAC!=		${TOOL_SED} '/^\#/d;/^$$/d;' ${.CURDIR}/APPLE/MAC.part

.for i in ${PART_MAC}
.if exists(${.CURDIR}/APPLE/$i%UCS.src)
FILES+=			$i%UCS.mps
CLEANFILES+=		$i%UCS.mps
FILESDIR_$i%UCS.mps=	${BINDIR}/APPLE
.endif
.if exists(${.CURDIR}/APPLE/UCS%$i.src)
FILES+=			UCS%$i.mps
CLEANFILES+=		UCS%$i.mps
FILESDIR_UCS%$i.mps=	${BINDIR}/APPLE
.endif
.endfor

mapper.dir.APPLE: ${.CURDIR}/APPLE/MAC.part
	${_MKTARGET_CREATE}
	( \
	    echo "# APPLE"; \
	    for i in ${PART_MAC}; do \
		printf "%-32s%-32s%s\n" $$i/UCS mapper_std \
		    APPLE/$$i%UCS.mps; \
		printf "%-32s%-32s%s\n" UCS/$$i mapper_std \
		    APPLE/UCS%$$i.mps; \
	    done; \
	    echo; \
	) > ${.TARGET}

charset.pivot.APPLE: ${.CURDIR}/APPLE/MAC.part
	${_MKTARGET_CREATE}
	( \
	    echo "# APPLE"; \
	    for i in ${PART_MAC}; do \
		printf "%-32s%-32s%d\n" $$i UCS 1; \
		printf "%-32s%-32s%d\n" UCS $$i 1; \
	    done; \
	    echo; \
	) > ${.TARGET}
