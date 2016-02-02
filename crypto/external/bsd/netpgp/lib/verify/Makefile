# $NetBSD: Makefile,v 1.8 2015/04/15 19:13:46 mrg Exp $

LIB=netpgpverify
SRCS+=b64.c
SRCS+=bignum.c
SRCS+=bufgap.c
SRCS+=digest.c
SRCS+=libverify.c
SRCS+=misc.c
SRCS+=pgpsum.c
SRCS+=rsa.c
SRCS+=bzlib.c
SRCS+=zlib.c
SRCS+=sha1.c
SRCS+=sha2.c
SRCS+=md5c.c
SRCS+=rmd160.c
SRCS+=tiger.c
CPPFLAGS+=-I${.CURDIR} -I${EXTDIST}/src/netpgpverify
MAN=libnetpgpverify.3
WARNS=5
#.if defined(__MINIX)
#LSC: While Compiling with -O3:
#<..>/lib/verify/../../dist/src/netpgpverify/bignum.c:802:5: error: assuming signed overflow does not occur when assuming that (X + c) < X is always false [-Werror=strict-overflow]
#  if (olduse > ix) {
#       ^
#       cc1: all warnings being treated as errors
CWARNFLAGS+= -Wno-strict-overflow
#.endif # defined(__MINIX)

EXTDIST=${.CURDIR}/../../dist
.PATH: ${EXTDIST}/src/netpgpverify

INCS=verify.h
INCSDIR=/usr/include/netpgp

.include <bsd.lib.mk>
