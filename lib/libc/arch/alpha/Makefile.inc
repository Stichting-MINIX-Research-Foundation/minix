#	$NetBSD: Makefile.inc,v 1.20 2008/10/26 07:43:07 mrg Exp $

SRCS+=	__longjmp14.c
SRCS+=	__sigaction14_sigtramp.c __sigtramp2.S

# `source' files built from m4 source
SRCS+=	__divqu.S __divq.S __divlu.S __divl.S
SRCS+=	__remqu.S __remq.S __remlu.S __reml.S
CLEANFILES+=	__divqu.S __divq.S __divlu.S __divl.S
CLEANFILES+=	__remqu.S __remq.S __remlu.S __reml.S

__divqu.S: ${LIBCDIR}/arch/alpha/gen/divrem.m4
	${TOOL_M4} -DNAME=__divqu -DOP=div -DS=false -DWORDSIZE=64 \
		${.ALLSRC} > ${.TARGET}

__divq.S: ${LIBCDIR}/arch/alpha/gen/divrem.m4
	${TOOL_M4} -DNAME=__divq -DOP=div -DS=true -DWORDSIZE=64 \
		${.ALLSRC} > ${.TARGET}

__divlu.S: ${LIBCDIR}/arch/alpha/gen/divrem.m4
	${TOOL_M4} -DNAME=__divlu -DOP=div -DS=false -DWORDSIZE=32 \
		${.ALLSRC} > ${.TARGET}

__divl.S: ${LIBCDIR}/arch/alpha/gen/divrem.m4
	${TOOL_M4} -DNAME=__divl -DOP=div -DS=true -DWORDSIZE=32 \
		${.ALLSRC} > ${.TARGET}

__remqu.S: ${LIBCDIR}/arch/alpha/gen/divrem.m4
	${TOOL_M4} -DNAME=__remqu -DOP=rem -DS=false -DWORDSIZE=64 \
		${.ALLSRC} > ${.TARGET}

__remq.S: ${LIBCDIR}/arch/alpha/gen/divrem.m4
	${TOOL_M4} -DNAME=__remq -DOP=rem -DS=true -DWORDSIZE=64 \
		${.ALLSRC} > ${.TARGET}

__remlu.S: ${LIBCDIR}/arch/alpha/gen/divrem.m4
	${TOOL_M4} -DNAME=__remlu -DOP=rem -DS=false -DWORDSIZE=32 \
		${.ALLSRC} > ${.TARGET}

__reml.S: ${LIBCDIR}/arch/alpha/gen/divrem.m4
	${TOOL_M4} -DNAME=__reml -DOP=rem -DS=true -DWORDSIZE=32 \
		${.ALLSRC} > ${.TARGET}

CPPFLAGS+= -I.
