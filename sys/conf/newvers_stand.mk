#	$NetBSD: newvers_stand.mk,v 1.2 2017/04/09 14:52:14 christos Exp $

VERSIONFILE?=version
VERSIONMACHINE?=${MACHINE}
.if exists(${VERSIONFILE})

.if !make(depend)
SRCS+=		vers.c
.endif
CLEANFILES+=	vers.c

.if ${MKREPRO:Uno} == "yes"
.	if ${MKREPRO_TIMESTAMP:U0} != 0
VERSIONFLAGS+=-D ${MKREPRO_TIMESTAMP}
.	else
VERSIONFLAGS+=-d
.	endif
.endif

vers.c:	${VERSIONFILE}
	${_MKTARGET_CREATE}
	${HOST_SH} ${S}/conf/newvers_stand.sh \
	    -m ${VERSIONMACHINE} ${VERSIONFLAGS} ${.ALLSRC} ${NEWVERSWHAT}

.endif
