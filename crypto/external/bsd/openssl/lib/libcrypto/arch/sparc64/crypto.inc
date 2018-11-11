.PATH.S: ${.PARSEDIR}
CPUID_SRCS = sparcv9cap.c sparccpuid.S sparcv9-mont.S sparcv9a-mont.S 
CPUID_SRCS += sparct4-mont.S vis3-mont.S
CPUID = yes
#CPPFLAGS += -DOPENSSL_BN_ASM_MONT
.include "../../crypto.inc"
