#       $NetBSD: camellia.inc,v 1.2 2012/07/26 19:58:40 christos Exp $

.PATH:  ${OPENSSLSRC}/crypto/camellia

CAMELLIA_SRCS = camellia.c cmll_misc.c \
    cmll_ecb.c cmll_cbc.c cmll_cfb.c cmll_ofb.c cmll_ctr.c cmll_utl.c
SRCS += ${CAMELLIA_SRCS}

.for cryptosrc in ${CAMELLIA_SRCS}
CPPFLAGS.${cryptosrc} = -I${OPENSSLSRC}/crypto/camellia
.endfor
