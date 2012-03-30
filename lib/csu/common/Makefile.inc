#	$NetBSD: Makefile.inc,v 1.2 2010/12/07 19:51:02 joerg Exp $

.include <bsd.own.mk>

COMMON_DIR:=	${.PARSEDIR}
.PATH:		${COMMON_DIR}

CPPFLAGS+=	-I${NETBSDSRCDIR}/libexec/ld.elf_so -I${COMMON_DIR} -I.

OBJS+=		crt0.o gcrt0.o crti.o crtn.o
OBJS+=		crtbegin.o crtbeginS.o crtend.o

realall: ${OBJS}

crtbegin.o: crtbegin.S
	${_MKTARGET_COMPILE}
	${COMPILE.S} ${ARCHDIR}/crtbegin.S -o ${.TARGET}.o
	${LD} -x -r -o ${.TARGET} ${.TARGET}.o
	rm -f ${.TARGET}.o
.if ${MKSTRIPIDENT} != "no"
	${OBJCOPY} -R .ident ${.TARGET}
.endif

crtbeginS.o: crtbegin.S
	${_MKTARGET_COMPILE}
	${COMPILE.S} -DSHARED ${ARCHDIR}/crtbegin.S -o ${.TARGET}.o
	${LD} -x -r -o ${.TARGET} ${.TARGET}.o
	rm -f ${.TARGET}.o
.if ${MKSTRIPIDENT} != "no"
	${OBJCOPY} -R .ident ${.TARGET}
.endif

crtend.o: crtend.S
	${_MKTARGET_COMPILE}
	${COMPILE.S} ${ARCHDIR}/crtend.S -o ${.TARGET}.o
	${LD} -x -r -o ${.TARGET} ${.TARGET}.o
	rm -f ${.TARGET}.o
.if ${MKSTRIPIDENT} != "no"
	${OBJCOPY} -R .ident ${.TARGET}
.endif

.if ${MKPIC} != "no"
PICFLAGS=	-fPIC
.else
PICFLAGS=
.endif

crt0.o: crt0-common.c crt0.S
	${_MKTARGET_COMPILE}
	${COMPILE.c} ${PICFLAGS} ${COMMON_DIR}/crt0-common.c -o ${.TARGET}.c.o
	${COMPILE.S} ${ARCHDIR}/crt0.S -o ${.TARGET}.S.o
	${LD} -x -r -o ${.TARGET} ${.TARGET}.c.o ${.TARGET}.S.o
	rm -f ${.TARGET}.c.o ${.TARGET}.S.o
.if ${MKSTRIPIDENT} != "no"
	${OBJCOPY} -R .ident ${.TARGET}
.endif

gcrt0.o: crt0-common.c crt0.S
	${_MKTARGET_COMPILE}
	${COMPILE.c} ${PICFLAGS} -DMCRT0 ${COMMON_DIR}/crt0-common.c -o ${.TARGET}.c.o
	${COMPILE.S} ${ARCHDIR}/crt0.S -o ${.TARGET}.S.o
	${LD} -x -r -o ${.TARGET} ${.TARGET}.c.o ${.TARGET}.S.o
	rm -f ${.TARGET}.c.o ${.TARGET}.S.o
.if ${MKSTRIPIDENT} != "no"
	${OBJCOPY} -R .ident ${.TARGET}
.endif

sysident_assym.h: ${GENASSYM_CONF} ${GENASSYM_EXTRAS}
	${_MKTARGET_CREATE}
	cat ${COMMON_DIR}/sysident_assym.cf | \
	    ${TOOL_GENASSYM} -- ${CC} ${CFLAGS} ${CPPFLAGS} ${PROF} \
	    > sysident_assym.h.tmp && \
	mv -f sysident_assym.h.tmp sysident_assym.h

CLEANFILES+=	sysident_assym.h

crti.o: crti.S sysident_assym.h
crtn.o: crtn.S

FILES=${OBJS}
FILESDIR=${LIBDIR}
CLEANFILES+=${OBJS}
SYMLINKS+=	crtbegin.o ${LIBDIR}/crtbeginT.o
SYMLINKS+=	crtend.o ${LIBDIR}/crtendS.o
SYMLINKS+=	crt0.o ${LIBDIR}/crt1.o

.include <bsd.prog.mk>
