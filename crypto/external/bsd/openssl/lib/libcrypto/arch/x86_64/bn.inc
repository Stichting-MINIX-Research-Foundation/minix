.PATH.S: ${.PARSEDIR}
.PATH.c: ${OPENSSLSRC}/crypto/bn/asm
BN_SRCS = x86_64-gf2m.S x86_64-mont.S x86_64-mont5.S x86_64-gcc.c
CPPFLAGS += -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_BN_ASM_GF2m
.include "../../bn.inc"
