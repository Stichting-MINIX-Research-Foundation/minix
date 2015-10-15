#	$NetBSD: Makefile.inc,v 1.25 2014/03/04 17:57:56 joerg Exp $

.include <bsd.own.mk>

# XXX: FIXME: This is defined in bsd.lib.mk
PICFLAGS ?= -fPIC

COMMON_DIR:=	${.CURDIR}/common
.PATH:		${COMMON_DIR}

CPPFLAGS+=	-I${NETBSDSRCDIR}/libexec/ld.elf_so -I${COMMON_DIR} -I.

OBJS+=		crt0.o gcrt0.o crti.o crtn.o
OBJS+=		crtbegin.o crtend.o

.if ${MKPIC} == "yes"
OBJS+=		crtbeginS.o
CFLAGS.crtbegin.c+= -fPIE
.endif
.if ${MACHINE_ARCH} == "alpha"
OBJS+=		crtfm.o
.endif

.if ${CSU_MACHINE_ARCH} == "sparc64"
# create helper objects for the compiler to mark compiler memory models
.for m in medlow medmid medany
sparc_mc${m}.o:	compident.S sysident_assym.h
	${CC} ${ASFLAGS} -I. -DCONTENT=\"${m}\\0\\0\" -DCONTENTLENGTH=8 -c \
		-o $@ ${COMMON_DIR}/compident.S
.endfor
OBJS += sparc_mcmedlow.o sparc_mcmedmid.o sparc_mcmedany.o
.endif

realall: ${OBJS}

.if exists(${ARCHDIR}/crtbegin.S)
crtbegin.o: crtbegin.S
	${_MKTARGET_COMPILE}
	${COMPILE.S} ${ARCHDIR}/crtbegin.S -o ${.TARGET}.o
.else
crtbegin.o: crtbegin.c crtbegin.h
	${_MKTARGET_COMPILE}
	${COMPILE.c} ${CFLAGS.crtbegin.c} ${COMMON_DIR}/crtbegin.c -o ${.TARGET}.o
.endif
	${OBJCOPY} ${OBJCOPYLIBFLAGS} ${.TARGET}.o ${.TARGET} 
	rm -f ${.TARGET}.o
.if ${MKSTRIPIDENT} != "no"
	${OBJCOPY} -R .ident ${.TARGET}
.endif

.if exists(${ARCHDIR}/crtbegin.S)
crtbeginS.o: crtbegin.S
	${_MKTARGET_COMPILE}
	${COMPILE.S} ${PICFLAGS} -DSHARED ${ARCHDIR}/crtbegin.S -o ${.TARGET}.o
.else
crtbeginS.o: crtbegin.c crtbegin.h
	${_MKTARGET_COMPILE}
	${COMPILE.c} ${PICFLAGS} -DSHARED ${COMMON_DIR}/crtbegin.c -o ${.TARGET}.o
.endif
	${OBJCOPY} ${OBJCOPYLIBFLAGS} ${.TARGET}.o ${.TARGET} 
	rm -f ${.TARGET}.o
.if ${MKSTRIPIDENT} != "no"
	${OBJCOPY} -R .ident ${.TARGET}
.endif

crtend.o: crtend.S
	${_MKTARGET_COMPILE}
	${COMPILE.S} ${ARCHDIR}/crtend.S -o ${.TARGET}.o
	${OBJCOPY} ${OBJCOPYLIBFLAGS} ${.TARGET}.o ${.TARGET} 
	rm -f ${.TARGET}.o
.if ${MKSTRIPIDENT} != "no"
	${OBJCOPY} -R .ident ${.TARGET}
.endif

.if ${MKPIC} != "no"
MY_PICFLAGS=	${PICFLAGS}
.else
MY_PICFLAGS=
.endif

crt0.o: crt0.S crt0-common.c
	${_MKTARGET_COMPILE}
	${COMPILE.S} ${ARCHDIR}/crt0.S -o ${.TARGET}.S.o
	${COMPILE.c} ${MY_PICFLAGS} ${COMMON_DIR}/crt0-common.c -o ${.TARGET}.c.o
	${LD} -r -o ${.TARGET}.o ${.TARGET}.S.o ${.TARGET}.c.o
	${OBJCOPY} ${OBJCOPYLIBFLAGS} ${.TARGET}.o ${.TARGET} 
	rm -f ${.TARGET}.S.o ${.TARGET}.c.o ${.TARGET}.o
.if ${MKSTRIPIDENT} != "no"
	${OBJCOPY} -R .ident ${.TARGET}
.endif

gcrt0.o: crt0.S crt0-common.c
	${_MKTARGET_COMPILE}
	${COMPILE.S} ${ARCHDIR}/crt0.S -o ${.TARGET}.S.o
	${COMPILE.c} ${MY_PICFLAGS} -DMCRT0 ${COMMON_DIR}/crt0-common.c -o ${.TARGET}.c.o
	${LD} -r -o ${.TARGET}.o ${.TARGET}.S.o ${.TARGET}.c.o
	${OBJCOPY} ${OBJCOPYLIBFLAGS} ${.TARGET}.o ${.TARGET} 
	rm -f ${.TARGET}.S.o ${.TARGET}.c.o ${.TARGET}.o
.if ${MKSTRIPIDENT} != "no"
	${OBJCOPY} -R .ident ${.TARGET}
.endif

.if ${MACHINE_ARCH} == "alpha"
# can't do this in Makefile.inc otherwise it will before realall:
crtfm.o: crtfm.c
	${_MKTARGET_COMPILE}
	${COMPILE.c} ${.ALLSRC} -o ${.TARGET}.o
	${OBJCOPY} ${OBJCOPYLIBFLAGS} ${.TARGET}.o ${.TARGET} 
	rm -f ${.TARGET}.o
.endif

GENASSYM_CONF=	${COMMON_DIR}/sysident_assym.cf 
sysident_assym.h: ${GENASSYM_CONF} ${GENASSYM_EXTRAS} ${NETBSDSRCDIR}/sys/sys/param.h
	${_MKTARGET_CREATE}
	cat ${COMMON_DIR}/sysident_assym.cf | \
	    ${TOOL_GENASSYM} -- ${CC} ${CFLAGS:N-Wa,*} ${CPPFLAGS} ${PROF} \
	    ${GENASSYM_CPPFLAGS} > sysident_assym.h.tmp && \
	mv -f sysident_assym.h.tmp sysident_assym.h

CLEANFILES+=	sysident_assym.h

crti.o: crti.S sysident_assym.h sysident.S
crtn.o: crtn.S

FILES=${OBJS}
FILESDIR=${LIBDIR}
CLEANFILES+=${OBJS}

.if ${MKPIC} == "yes"
SYMLINKS+=	crtbegin.o ${LIBDIR}/crtbeginT.o
SYMLINKS+=	crtend.o ${LIBDIR}/crtendS.o
.endif

.include <bsd.prog.mk>
