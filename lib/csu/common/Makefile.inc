#	$NetBSD: Makefile.inc,v 1.7 2012/03/25 06:55:19 joerg Exp $

.include <bsd.own.mk>

COMMON_DIR:=	${.CURDIR}/common
.PATH:		${COMMON_DIR}

CPPFLAGS+=	-I${NETBSDSRCDIR}/libexec/ld.elf_so -I${COMMON_DIR} -I.

OBJS+=		crt0.o gcrt0.o crti.o crtn.o
OBJS+=		crtbegin.o crtbeginS.o crtend.o

# BJG - for backwards compatability with minix gcc
# which links with crt1.o instead of crt0.o
SYMLINKS+=     crt0.o ${LIBDIR}/crt1.o

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
	${COMPILE.S} -DPIC -DSHARED ${ARCHDIR}/crtbegin.S -o ${.TARGET}.o
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

crt0.o: crt0.S crt0-common.c
	${_MKTARGET_COMPILE}
	${COMPILE.S} ${ARCHDIR}/crt0.S -o ${.TARGET}.S.o
	${COMPILE.c} ${PICFLAGS} ${COMMON_DIR}/crt0-common.c -o ${.TARGET}.c.o
	${LD} -x -r -o ${.TARGET} ${.TARGET}.S.o ${.TARGET}.c.o
	rm -f ${.TARGET}.S.o ${.TARGET}.c.o
.if ${MKSTRIPIDENT} != "no"
	${OBJCOPY} -R .ident ${.TARGET}
.endif

gcrt0.o: crt0.S crt0-common.c
	${_MKTARGET_COMPILE}
	${COMPILE.S} ${ARCHDIR}/crt0.S -o ${.TARGET}.S.o
	${COMPILE.c} ${PICFLAGS} -DMCRT0 ${COMMON_DIR}/crt0-common.c -o ${.TARGET}.c.o
	${LD} -x -r -o ${.TARGET} ${.TARGET}.S.o ${.TARGET}.c.o
	rm -f ${.TARGET}.S.o ${.TARGET}.c.o
.if ${MKSTRIPIDENT} != "no"
	${OBJCOPY} -R .ident ${.TARGET}
.endif

sysident_assym.h: ${GENASSYM_CONF} ${GENASSYM_EXTRAS}
	${_MKTARGET_CREATE}
	cat ${COMMON_DIR}/sysident_assym.cf | \
	    ${TOOL_GENASSYM} -- ${CC} ${CFLAGS:N-Wa,*} ${CPPFLAGS} ${PROF} \
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

.include <bsd.prog.mk>
