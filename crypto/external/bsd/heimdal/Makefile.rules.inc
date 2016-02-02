# $NetBSD: Makefile.rules.inc,v 1.7 2012/09/05 19:31:04 christos Exp $

SRCS+= ${HEIMSRCS:N*.et:N*.in:N*.asn1}

.for et_src in ${HEIMSRCS:M*.et}
CLEANFILES+=		${et_src:.et=.c} ${et_src:.et=.h}
DPSRCS+=		${et_src:.et=.c} ${et_src:.et=.h}
COMPILE_ET_INCS+=	${et_src:.et=.h}
OBJS+=			${et_src:.et=.o}

${et_src:.et=.c} ${et_src:.et=.h}: ${et_src} ${TOOL_COMPILE_ET}
	${TOOL_COMPILE_ET} ${.ALLSRC:[1]}

.endfor

.for src in ${HEIMSRCS:M*.asn1}
ASN1_FILES.${src}	?= asn1_${src:.asn1=_asn1.x}

.if ${ASN1_FILES.${src}:[\#]} == 1
ASN1_OPTS.${src} ?= --one-code-file
.endif

CLEANFILES+=				\
	${src:.asn1=_asn1_files}	\
	${src:.asn1=_asn1-template.c}	\
	${ASN1_FILES.${src}}		\
	${ASN1_FILES.${src}:.x=.c}	\
	${src:.asn1=_asn1.h}		\
	${src:.asn1=_asn1.hx}		\
	${src:.asn1=_asn1-priv.h}	\
	${src:.asn1=_asn1-priv.hx}	\
	${src:.asn1=.ts}

ASN1_INCS += ${src:.asn1=_asn1.h}

DPSRCS +=	${src:.asn1=_asn1.h} ${src:.asn1=_asn1-priv.h}
OBJS +=		${ASN1_FILES.${src}:.x=.o}

${ASN1_FILES.${src}} ${src:.asn1=_asn1.h} ${src:.asn1=_asn1-priv.h}: \
	 ${src:.asn1=.ts}

.if exists(${src:.asn1=.opt})
${src:.asn1=.ts}: ${src} ${src:.asn1=.opt} ${TOOL_ASN1_COMPILE}
	@touch $@
	${TOOL_ASN1_COMPILE}			\
		${ASN1_OPTS.${src}}		\
		--option-file=${.ALLSRC:[2]}	\
		${.ALLSRC:[1]} ${src:.asn1=_asn1}
	@${TOOL_SED} -E 						\
	    -e 's,#include <(.*)_asn1\.h>,#include <krb5/\1_asn1.h>,'	\
	    2> /dev/null < ${src:.asn1=_asn1.hx} > ${src:.asn1=_asn1.h}
	@cmp -s ${src:.asn1=_asn1-priv.hx} ${src:.asn1=_asn1-priv.h}	\
	    2> /dev/null ||						\
	    cp ${src:.asn1=_asn1-priv.hx} ${src:.asn1=_asn1-priv.h}
.else
${src:.asn1=.ts}: ${src} ${TOOL_ASN1_COMPILE}
	@touch $@
	${TOOL_ASN1_COMPILE}			\
		${ASN1_OPTS.${src}}		\
		${.ALLSRC:[1]} ${src:.asn1=_asn1}
	@${TOOL_SED} -E 						\
	    -e 's,#include <(.*)_asn1\.h>,#include <krb5/\1_asn1.h>,'	\
	    2> /dev/null < ${src:.asn1=_asn1.hx} > ${src:.asn1=_asn1.h}
	@cmp -s ${src:.asn1=_asn1-priv.hx} ${src:.asn1=_asn1-priv.h}	\
	    2> /dev/null ||						\
	    cp ${src:.asn1=_asn1-priv.hx} ${src:.asn1=_asn1-priv.h}
.endif

.if ${MKREPRO:Uno} == "yes"
NORMALIZE_SRC=-e "s@${NETBSDSRCDIR}@/usr/src@g" 
.endif

.for x2c in ${ASN1_FILES.${src}}
${x2c:.x=.c}: ${x2c}
	@${TOOL_SED} ${NORMALIZE_SRC} < ${x2c} > ${x2c}.r
	@cmp -s ${x2c}.r ${x2c:.x=.c} 2> /dev/null || cp ${x2c}.r ${x2c:.x=.c}
	@rm -f ${x2c}.r
.endfor

.endfor

#
# and for slc:

.for slc_src in ${HEIMSRCS:M*.in}
CLEANFILES+=	${slc_src:.in=.c} ${slc_src:.in=.h}
DPSRCS+=	${slc_src:.in=.c} ${slc_src:.in=.h}
OBJS+=		${slc_src:.in=.o}
SLC_INCS+=	${slc_src:.in=.h}

${slc_src:.in=.c} ${slc_src:.in=.h}: ${slc_src}
	${TOOL_SLC} ${.ALLSRC}

.endfor

#
# And a tiny bit of logic for bsd.prog.mk:

.if defined(PROG) && defined(OBJS)
OBJS.${PROG} += ${OBJS}
.endif

.if defined(LIB) && exists(version-script.map) && !defined(NO_VERSION_SCRIPT)
version-script.map:

LDFLAGS.lib${LIB} += -Wl,--version-script=${version-script.map:P}
.endif
