# $NetBSD: Makefile.inc,v 1.4 2008/10/25 22:35:36 apb Exp $

.PATH: ${.CURDIR}/ISO646

SRCS_mapper.dir+=	mapper.dir.ISO646
SRCS_charset.pivot+=	charset.pivot.ISO646
CLEANFILES+=		mapper.dir.ISO646 charset.pivot.ISO646
PART_ISO646!=		${TOOL_SED} '/^\#/d;/^$$/d;' \
				${.CURDIR}/ISO646/ISO646.part

mapper.dir.ISO646: ${.CURDIR}/ISO646/ISO646.part
	${_MKTARGET_CREATE}
	(\
	    echo "# ISO646";\
	    for i in ${PART_ISO646}; do \
		j=`echo $$i | tr : @`;\
		printf '%-32s%-16s%s\n' ISO646-$$i'/UCS' mapper_646 \
		    ISO646/ISO646-$$j'%UCS.646';\
		printf '%-32s%-16s%s\n' UCS/ISO646-$$i mapper_646 \
		    '!ISO646/ISO646-'$$j'%UCS.646';\
	    done;\
	    echo;\
	) > ${.TARGET}

charset.pivot.ISO646: ${.CURDIR}/ISO646/ISO646.part
	${_MKTARGET_CREATE}
	(\
	    echo "# ISO646";\
	    for i in ${PART_ISO646}; do \
		printf "%-32s%-32s%d\n" ISO646-$$i UCS 1;\
		printf "%-32s%-32s%d\n" UCS ISO646-$$i 1;\
	    done;\
	    echo;\
	) > ${.TARGET}

.for i in ${PART_ISO646}
FILES+=	ISO646-${i:S/:/@/}%UCS.646
FILESDIR_ISO646-${i:S/:/@/}%UCS.646=	${BINDIR}/ISO646
.endfor
