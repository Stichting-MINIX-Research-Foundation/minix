#	$NetBSD: Makefile.inc,v 1.13 2009/12/06 07:05:50 uebayasi Exp $

SRCS+=	__sigaction14_sigtramp.c __sigtramp2.S

# Some assembler files use v9a extensions.
AFLAGS+= -Wa,-Av9a

# `source' files built from m4 source
# the name `div.o' is taken for the ANSI C `div' function, hence sdiv here
SRCS+=	rem.S sdiv.S udiv.S urem.S
CLEANFILES+=rem.S sdiv.S udiv.S urem.S

sdiv.S: ${LIBCDIR}/arch/sparc64/gen/divrem.m4
	@echo 'building ${.TARGET} from ${.ALLSRC}'
	@(echo "define(NAME,\`.div')define(OP,\`div')define(S,\`true')"; \
	 cat ${.ALLSRC}) | ${TOOL_M4} > ${.TARGET}
	@chmod 444 ${.TARGET}

udiv.S: ${LIBCDIR}/arch/sparc64/gen/divrem.m4
	@echo 'building ${.TARGET} from ${.ALLSRC}'
	@(echo "define(NAME,\`.udiv')define(OP,\`div')define(S,\`false')"; \
	 cat ${.ALLSRC}) | ${TOOL_M4} > ${.TARGET}
	@chmod 444 ${.TARGET}

rem.S: ${LIBCDIR}/arch/sparc64/gen/divrem.m4
	@echo 'building ${.TARGET} from ${.ALLSRC}'
	@(echo "define(NAME,\`.rem')define(OP,\`rem')define(S,\`true')"; \
	 cat ${.ALLSRC}) | ${TOOL_M4} > ${.TARGET}
	@chmod 444 ${.TARGET}

urem.S: ${LIBCDIR}/arch/sparc64/gen/divrem.m4
	@echo 'building ${.TARGET} from ${.ALLSRC}'
	@(echo "define(NAME,\`.urem')define(OP,\`rem')define(S,\`false')"; \
	 cat ${.ALLSRC}) | ${TOOL_M4} > ${.TARGET}
	@chmod 444 ${.TARGET}

.PATH: ${ARCHDIR}/softfloat
SRCS+= qp.c
CPPFLAGS+=	-DSOFTFLOATSPARC64_FOR_GCC
.if ${MKSOFTFLOAT} != "no"
.include <softfloat/Makefile.inc>
.else
#
# Can't directly use softfloat.c due to .PATH issues with the other source
# in softfloat. Just provide a wrapper which pulls it in via a #include
SRCS.softfloat= softfloat-wrapper.c
SRCS+= ${SRCS.softfloat}
CPPFLAGS+=      -I${ARCHDIR}/softfloat -I${.CURDIR}/softfloat \
		-I${.CURDIR}/softfloat/bits64
.endif
