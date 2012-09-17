#	$NetBSD: Makefile.inc,v 1.14 2011/04/12 03:35:47 mrg Exp $

SRCS+=	__sigaction14_sigtramp.c __sigtramp2.S

# `source' files built from m4 source
# the name `div.o' is taken for the ANSI C `div' function, hence sdiv here
SRCS+=	rem.S sdiv.S udiv.S urem.S
CLEANFILES+=rem.S sdiv.S udiv.S urem.S

sdiv.S: ${LIBCDIR}/arch/sparc/gen/divrem.m4
	@echo 'building ${.TARGET} from ${.ALLSRC}'
	@rm -f ${.TARGET}
	@(echo "define(NAME,\`.div')define(OP,\`div')define(S,\`true')"; \
	 cat ${.ALLSRC}) | ${TOOL_M4} > ${.TARGET}
	@chmod 444 ${.TARGET}

udiv.S: ${LIBCDIR}/arch/sparc/gen/divrem.m4
	@echo 'building ${.TARGET} from ${.ALLSRC}'
	@rm -f ${.TARGET}
	@(echo "define(NAME,\`.udiv')define(OP,\`div')define(S,\`false')"; \
	 cat ${.ALLSRC}) | ${TOOL_M4} > ${.TARGET}
	@chmod 444 ${.TARGET}

rem.S: ${LIBCDIR}/arch/sparc/gen/divrem.m4
	@echo 'building ${.TARGET} from ${.ALLSRC}'
	@rm -f ${.TARGET}
	@(echo "define(NAME,\`.rem')define(OP,\`rem')define(S,\`true')"; \
	 cat ${.ALLSRC}) | ${TOOL_M4} > ${.TARGET}
	@chmod 444 ${.TARGET}

urem.S: ${LIBCDIR}/arch/sparc/gen/divrem.m4
	@echo 'building ${.TARGET} from ${.ALLSRC}'
	@rm -f ${.TARGET}
	@(echo "define(NAME,\`.urem')define(OP,\`rem')define(S,\`false')"; \
	 cat ${.ALLSRC}) | ${TOOL_M4} > ${.TARGET}
	@chmod 444 ${.TARGET}
