#	$NetBSD: Makefile.inc,v 1.2 2014/09/05 18:09:37 matt Exp $

SRCS+=		__sigaction14_sigtramp.c __sigtramp2.S
SRCS+=		mulsi3.S

CPPFLAGS+=	-I.

DPSRCS+=	sysassym.h
CLEANFILES+=	sysassym.h sysassym.h.tmp

sysassym.h: ${DESTDIR}/usr/include/sys/syscall.h \
	${DESTDIR}/usr/include/sys/syscallargs.h ${ARCHDIR}/syscallargs.awk
	${_MKTARGET_CREATE}
	${TOOL_CAT} \
		${DESTDIR}/usr/include/sys/syscall.h \
		${DESTDIR}/usr/include/sys/syscallargs.h | \
	${TOOL_AWK} -f ${ARCHDIR}/syscallargs.awk | \
	${TOOL_GENASSYM} -- ${CC} -ffreestanding ${CFLAGS:N-Wa,*} \
		${CPPFLAGS:N-D*} -D_KMEMUSER \
		${GENASSYM_CPPFLAGS} > $@.tmp && \
	mv -f $@.tmp $@

.if ${MKSOFTFLOAT} != "no"
.include <softfloat/Makefile.inc>
.endif
