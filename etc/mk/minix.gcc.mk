.SUFFIXES:	.o .s .gs

ASMCONV=asmconv
CPP.s=${CC} ${AFLAGS} -E -x assembler-with-cpp
ASMCONVFLAGS+=-mi386 ack gnu
ASSEMBLE=gas ${AFLAGS}

# Need to convert ACK assembly files to GNU assembly before building
.s.o:
	${_MKTARGET_COMPILE}
	${CPP.s} ${COPTS.${.IMPSRC:T}} ${CPUFLAGS.${.IMPSRC:T}} ${CPPFLAGS.${.IMPSRC:T}} ${.IMPSRC} -o ${.PREFIX}.ack.s
	${ASMCONV} ${ASMCONVFLAGS} ${.PREFIX}.ack.s ${.PREFIX}.gnu.s
	${COMPILE.s} ${COPTS.${.IMPSRC:T}} ${CPUFLAGS.${.IMPSRC:T}} ${CPPFLAGS.${.IMPSRC:T}} ${.PREFIX}.gnu.s -o ${.TARGET}
	rm -rf ${.PREFIX}.ack.s ${.PREFIX}.gnu.s

# GNU Assembly file
.gs.o:
	${_MKTARGET_COMPILE}
	${ASSEMBLE} ${COPTS.${.IMPSRC:T}} ${CPUFLAGS.${.IMPSRC:T}} ${CPPFLAGS.${.IMPSRC:T}} ${.IMPSRC} -o ${.TARGET}
