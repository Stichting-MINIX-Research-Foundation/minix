#	$NetBSD: Makefile.inc,v 1.32 2009/12/13 08:52:38 mrg Exp $

.if !defined(ELFSIZE)
ELFSIZE=32
.endif

.include "../../Makefile.inc"

CPPFLAGS+=	-DLIBC_SCCS -DPIC -DDYNAMIC -DELFSIZE=${ELFSIZE}
CPPFLAGS+=	-I${NETBSDSRCDIR}/libexec/ld.elf_so
CPPFLAGS+=	-I${.CURDIR}/../common_elf
CPPFLAGS+=	-I${NETBSDSRCDIR}/lib/libc/dlfcn
CPPFLAGS+=	-DDWARF2_EH
CPPFLAGS+=	-DJCR
CPPFLAGS+=	-DDSO_HANDLE

.if defined(HAVE_GCC) && ${HAVE_GCC} > 3
CFLAGS+=	-fno-unit-at-a-time
.endif

.include <bsd.own.mk>

.PATH:		${.CURDIR}/../common_elf

SRCS+=		crt0.c crti.c crtn.c
OBJS+=		crt0.o gcrt0.o crti.o crtn.o
.if ${MKPIC} != "no"
COPTS+=		-fPIC
.endif

realall: ${OBJS}

crt0.o: crt0.c
	${_MKTARGET_COMPILE}
	${COMPILE.c} -DCRT0 ${.IMPSRC} -o ${.TARGET}.o
	${LD} -x -r -o ${.TARGET} ${.TARGET}.o
	rm -f ${.TARGET}.o
.if ${MKSTRIPIDENT} != "no"
	${OBJCOPY} -R .ident ${.TARGET}
.endif

# dependant on crt0.o to pick up header dependencies
gcrt0.o: ${.CURDIR}/crt0.c crt0.o
	${_MKTARGET_COMPILE}
	${COMPILE.c} -DMCRT0 ${.CURDIR}/crt0.c -o ${.TARGET}.o
	${LD} -x -r -o ${.TARGET} ${.TARGET}.o
	rm -f ${.TARGET}.o
.if ${MKSTRIPIDENT} != "no"
	${OBJCOPY} -R .ident ${.TARGET}
.endif

crti.o: crti.c
	${_MKTARGET_COMPILE}
	${COMPILE.c} ${.IMPSRC} -o ${.TARGET}.o
	${LD} -X -r -o ${.TARGET} ${.TARGET}.o
	rm -f ${.TARGET}.o
.if ${MKSTRIPIDENT} != "no"
	${OBJCOPY} -R .ident ${.TARGET}
.endif

crtn.o: crtn.c
	${_MKTARGET_COMPILE}
	${COMPILE.c} ${.IMPSRC} -o ${.TARGET}.o
	${LD} -X -r -o ${.TARGET} ${.TARGET}.o
	rm -f ${.TARGET}.o
.if ${MKSTRIPIDENT} != "no"
	${OBJCOPY} -R .ident ${.TARGET}
.endif

FILES=${OBJS}
FILESDIR=${LIBDIR}
CLEANFILES=${OBJS}

.include <bsd.prog.mk>
