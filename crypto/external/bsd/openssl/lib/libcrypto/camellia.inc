#       $NetBSD: camellia.inc,v 1.3 2015/05/16 20:22:15 joerg Exp $

.PATH:  ${OPENSSLSRC}/crypto/camellia

CAMELLIA_SRCS?= camellia.c cmll_cbc.c
CAMELLIA_SRCS += cmll_misc.c cmll_ecb.c cmll_cfb.c \
    cmll_ofb.c cmll_ctr.c cmll_utl.c
SRCS += ${CAMELLIA_SRCS}

.for cryptosrc in ${CAMELLIA_SRCS}
CPPFLAGS.${cryptosrc} = -I${OPENSSLSRC}/crypto/camellia
.endfor
