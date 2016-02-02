.if ${MACHINE} == "sparc64"
.PATH.S: ${.PARSEDIR}
# XXX bn-sparcv8plus.S doesn't work well. why?
BN_SRCS = bn-sparcv8.S
.endif
.include "../../bn.inc"
