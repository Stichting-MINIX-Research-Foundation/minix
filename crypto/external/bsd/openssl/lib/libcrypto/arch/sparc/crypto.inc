.if ${MACHINE} == "sparc64"
.PATH.S: ${.PARSEDIR}
CPUID_SRCS = sparcv9cap.c sparccpuid.S sparcv9-mont.S sparcv9a-mont.S 
CPUID_SRCS += sparct4-mont.S vis3-mont.S
CPUID = yes
#CPPFLAGS += -DOPENSSL_BN_ASM_MONT
AFLAGS.sparcv9-mont.S+= -Wa,-Av9
AFLAGS.sparcv9a-mont.S+= -Wa,-Av9a
AFLAGS.sparct4-mont.S+= -Wa,-Av9a
AFLAGS.vis3-mont.S+= -Wa,-Av9a
.endif
.include "../../crypto.inc"
