.SUFFIXES:	.o .e

# Treated like a C file
.e.o:
	${_MKTARGET_COMPILE}
	${COMPILE.c} ${COPTS.${.IMPSRC:T}} ${CPUFLAGS.${.IMPSRC:T}} ${CPPFLAGS.${.IMPSRC:T}} ${.IMPSRC} -o ${.TARGET}
# .if !defined(CFLAGS) || empty(CFLAGS:M*-g*)
# 	${OBJCOPY} -x ${.TARGET}
# .endif

