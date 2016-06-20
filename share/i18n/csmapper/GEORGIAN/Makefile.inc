# $NetBSD: Makefile.inc,v 1.3 2008/10/25 22:35:36 apb Exp $

.PATH: ${.CURDIR}/GEORGIAN

SRCS_mapper.dir+=	mapper.dir.GEORGIAN
SRCS_charset.pivot+=	charset.pivot.GEORGIAN
CLEANFILES+=		mapper.dir.GEORGIAN charset.pivot.GEORGIAN
PART_GEORGIAN!=		${TOOL_SED} '/^\#/d;/^$$/d;' \
				${.CURDIR}/GEORGIAN/GEORGIAN.part

mapper.dir.GEORGIAN: ${.CURDIR}/GEORGIAN/GEORGIAN.part
	${_MKTARGET_CREATE}
	(\
	    echo "# GEORGIAN";\
	    for i in ${PART_GEORGIAN}; do \
		f=`echo $$i | ${TOOL_SED} 's/:/@/'`; \
		printf '%-32s%-16s%s\n' GEORGIAN-$$i'/UCS' \
		    mapper_std GEORGIAN/GEORGIAN-$$f'%UCS.mps';\
		printf '%-32s%-16s%s\n' UCS/GEORGIAN-$$i \
		    mapper_std GEORGIAN/UCS%GEORGIAN-$$f'.mps';\
	    done;\
	    echo;\
	) > ${.TARGET}

charset.pivot.GEORGIAN: ${.CURDIR}/GEORGIAN/GEORGIAN.part
	${_MKTARGET_CREATE}
	(\
	    echo "# GEORGIAN";\
	    for i in ${PART_GEORGIAN}; do \
		printf "%-32s%-32s%d\n" GEORGIAN-$$i UCS 1;\
		printf "%-32s%-32s%d\n" UCS GEORGIAN-$$i 1;\
	    done;\
	    echo;\
	) > ${.TARGET}

.for i in ${PART_GEORGIAN}
FILES+=		GEORGIAN-${i:S/:/@/}%UCS.mps UCS%GEORGIAN-${i:S/:/@/}.mps
CLEANFILES+=	GEORGIAN-${i:S/:/@/}%UCS.mps UCS%GEORGIAN-${i:S/:/@/}.mps
FILESDIR_GEORGIAN-${i:S/:/@/}%UCS.mps=	${BINDIR}/GEORGIAN
FILESDIR_UCS%GEORGIAN-${i:S/:/@/}.mps=	${BINDIR}/GEORGIAN
.endfor
