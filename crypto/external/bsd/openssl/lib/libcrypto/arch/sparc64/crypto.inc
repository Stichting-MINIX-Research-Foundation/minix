.PATH.S: ${.PARSEDIR}
CPUID_SRCS = sparcv9cap.c sparccpuid.S sparcv9-mont.S sparcv9a-mont.S
CPUID = yes
.include "../../crypto.inc"
