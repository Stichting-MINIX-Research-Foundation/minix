#	$NetBSD: bsd.dep.mk,v 1.68 2008/10/25 22:27:36 apb Exp $

##### Basic targets
cleandir:	cleandepend
realdepend:	beforedepend .depend afterdepend
.ORDER:		beforedepend .depend afterdepend

beforedepend .depend afterdepend: # ensure existence

##### Default values
MKDEP?=			mkdep
MKDEP_SUFFIXES?=	.o

##### Build rules
# some of the rules involve .h sources, so remove them from mkdep line

.if defined(SRCS)							# {
#_TRADITIONAL_CPP?=-traditional-cpp
__acpp_flags=	${_TRADITIONAL_CPP}

__DPSRCS.all=	${SRCS:C/\.(c|m|s|S|C|cc|cpp|cxx)$/.d/} \
		${DPSRCS:C/\.(c|m|s|S|C|cc|cpp|cxx)$/.d/}
__DPSRCS.d=	${__DPSRCS.all:O:u:M*.d}
__DPSRCS.notd=	${__DPSRCS.all:O:u:N*.d}

.NOPATH: .depend ${__DPSRCS.d}

.if !empty(__DPSRCS.d)							# {
${__DPSRCS.d}: ${__DPSRCS.notd} ${DPSRCS}
.endif									# }

.depend: ${__DPSRCS.d}
	${_MKTARGET_CREATE}
	rm -f .depend
#	${MKDEP} -d -f ${.TARGET} -s ${MKDEP_SUFFIXES:Q} ${__DPSRCS.d}
	cat ${__DPSRCS.d} > ${.TARGET}

.SUFFIXES: .d .s .S .c .C .cc .cpp .cxx .m

.c.d:
	${_MKTARGET_CREATE}
#	${MKDEP} -f ${.TARGET} -- ${MKDEPFLAGS} \
#	    ${CFLAGS:C/-([IDU])[  ]*/-\1/Wg:M-[IDU]*} \
#	    ${CPPFLAGS} ${CPPFLAGS.${.IMPSRC:T}} ${.IMPSRC}
#	mkdep -- ${MKDEPFLAGS} \
#	    ${CFLAGS:C/-([IDU])[  ]*/-\1/Wg:M-[IDU]*} \
#	    ${CPPFLAGS} ${CPPFLAGS.${.IMPSRC:T}} ${.IMPSRC} > ${.TARGET}
	mkdep "$(CC)  ${CFLAGS:C/-([IDU])[  ]*/-\1/Wg:M-[IDU]*} \
	${CPPFLAGS} ${CPPFLAGS.${.IMPSRC:T}} -E" ${.IMPSRC} > ${.TARGET}

# .m.d:
# 	${_MKTARGET_CREATE}
# 	${MKDEP} -f ${.TARGET} -- ${MKDEPFLAGS} \
# 	    ${OBJCFLAGS:C/-([IDU])[  ]*/-\1/Wg:M-[IDU]*} \
# 	    ${CPPFLAGS} ${CPPFLAGS.${.IMPSRC:T}} ${.IMPSRC}

.s.d .S.d:
	${_MKTARGET_CREATE}
#	 ${MKDEP} -f ${.TARGET} -- ${MKDEPFLAGS} \
#	     ${AFLAGS:C/-([IDU])[  ]*/-\1/Wg:M-[IDU]*} \
#	     ${CPPFLAGS} ${CPPFLAGS.${.IMPSRC:T}} ${__acpp_flags} ${.IMPSRC}
	mkdep "$(CC) ${AFLAGS:C/-([IDU])[  ]*/-\1/Wg:M-[IDU]*} \
	${CPPFLAGS} ${CPPFLAGS.${.IMPSRC:T}} -E" ${.IMPSRC} > ${.TARGET}

# .C.d .cc.d .cpp.d .cxx.d:
# 	${_MKTARGET_CREATE}
# 	${MKDEP} -f ${.TARGET} -- ${MKDEPFLAGS} \
# 	    ${CXXFLAGS:C/-([IDU])[  ]*/-\1/Wg:M-[IDU]*} \
# 	    ${DESTDIR:D-nostdinc++ ${CPPFLAG_ISYSTEMXX} \
# 			${DESTDIR}/usr/include/g++} \
# 	    ${CPPFLAGS} ${CPPFLAGS.${.IMPSRC:T}} ${.IMPSRC}

.endif # defined(SRCS)							# }

##### Clean rules
cleandepend: .PHONY
.if defined(SRCS)
	rm -f .depend ${__DPSRCS.d} ${.CURDIR}/tags ${CLEANDEPEND}
.endif

##### Custom rules
# .if !target(tags)
# tags: ${SRCS}
# .if defined(SRCS)
# 	-cd ${.CURDIR}; ctags -f /dev/stdout ${.ALLSRC:N*.h} | \
# 	    ${TOOL_SED} "s;\${.CURDIR}/;;" > tags
# .endif
# .endif
