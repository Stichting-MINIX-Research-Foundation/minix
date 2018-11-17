.PATH.S: ${.PARSEDIR}
MODES_SRCS = ghashp8-ppc.S
# CPPFLAGS += -DGHASH_ASM_PPC	# set by dist/crypto/modes/gcm128.c
.include "../../modes.inc"
