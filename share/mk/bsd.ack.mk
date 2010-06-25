CPPFLAGS+= -wo

.SUFFIXES:	.o .e .S

# Treated like a C file
.e.o:
	${_MKTARGET_COMPILE}
	${COMPILE.c} ${COPTS.${.IMPSRC:T}} ${CPUFLAGS.${.IMPSRC:T}} ${CPPFLAGS.${.IMPSRC:T}} ${.IMPSRC} -o ${.TARGET}
# .if !defined(CFLAGS) || empty(CFLAGS:M*-g*)
# 	${OBJCOPY} -x ${.TARGET}
# .endif

ASMCONV=gas2ack
AFLAGS+=-D__ASSEMBLY__ -D__minix -w -wo
CPP.s=${CC} -E ${AFLAGS}
ASMCONVFLAGS+=-mi386

# Need to convert ACK assembly files to GNU assembly before building
.S.o:
	${_MKTARGET_COMPILE}
	${CPP.s} ${COPTS.${.IMPSRC:T}} ${CPUFLAGS.${.IMPSRC:T}} ${CPPFLAGS.${.IMPSRC:T}} ${.IMPSRC} -o ${.PREFIX}.gnu.s
	${ASMCONV} ${ASMCONVFLAGS} ${.PREFIX}.gnu.s ${.PREFIX}.ack.s
	${COMPILE.s} ${COPTS.${.IMPSRC:T}} ${CPUFLAGS.${.IMPSRC:T}} ${CPPFLAGS.${.IMPSRC:T}} ${.PREFIX}.ack.s -o ${.TARGET}
	rm -rf ${.PREFIX}.ack.s ${.PREFIX}.gnu.s
