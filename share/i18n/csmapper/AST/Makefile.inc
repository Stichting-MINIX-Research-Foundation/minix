# $NetBSD: Makefile.inc,v 1.2 2008/10/25 22:35:36 apb Exp $

.PATH: ${.CURDIR}/AST

SRCS_mapper.dir+=	mapper.dir.AST
SRCS_charset.pivot+=	charset.pivot.AST
CLEANFILES+=		mapper.dir.AST charset.pivot.AST
PART_ARMSCII!=		${TOOL_SED} '/^\#/d;/^$$/d;' ${.CURDIR}/AST/ARMSCII.part

.for i in ${PART_ARMSCII}
.if exists(${.CURDIR}/AST/ARMSCII-$i%UCS.src)
FILES+=				ARMSCII-$i%UCS.mps
CLEANFILES+=			ARMSCII-$i%UCS.mps
FILESDIR_ARMSCII-$i%UCS.mps=	${BINDIR}/AST
.endif
.if exists(${.CURDIR}/AST/UCS%ARMSCII-$i.src)
FILES+=				UCS%ARMSCII-$i.mps
CLEANFILES+=			UCS%ARMSCII-$i.mps
FILESDIR_UCS%ARMSCII-$i.mps=	${BINDIR}/AST
.endif
.endfor

mapper.dir.AST: ${.CURDIR}/AST/ARMSCII.part
	${_MKTARGET_CREATE}
	( \
	    echo "# AST"; \
	    for i in ${PART_ARMSCII}; do \
		printf "%-32s%-32s%s\n" ARMSCII-$$i/UCS mapper_std \
		    AST/ARMSCII-$$i%UCS.mps; \
		printf "%-32s%-32s%s\n" UCS/ARMSCII-$$i mapper_std \
		    AST/UCS%ARMSCII-$$i.mps; \
	    done; \
	    echo; \
	) > ${.TARGET}

charset.pivot.AST: ${.CURDIR}/AST/ARMSCII.part
	${_MKTARGET_CREATE}
	( \
	    echo "# AST"; \
	    for i in ${PART_ARMSCII}; do \
		printf "%-32s%-32s%d\n" ARMSCII-$$i UCS 1; \
		printf "%-32s%-32s%d\n" UCS ARMSCII-$$i 1; \
	    done; \
	    echo; \
	) > ${.TARGET}
