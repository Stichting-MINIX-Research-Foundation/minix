# $NetBSD: Makefile,v 1.2 2012/11/20 05:26:25 agc Exp $

PROG=netpgpv
SRCS=libverify.c b64.c pgpsum.c
SRCS+=digest.c tiger.c
SRCS+=bignum.c misc.c
SRCS+=rsaglue.c rsa.c
SRCS+=main.c
WARNS=5
MKMAN=no
CPPFLAGS+=-I${.CURDIR}/../libbn
CPPFLAGS+=-I${.CURDIR}/../librsa
LDADD+=-lz
LDADD+=-lbz2

# XXX - debugging
#CPPFLAGS+=-g -O0
#LDFLAGS+=-g -O0
#CPPFLAGS+=-O3
#LDFLAGS+=-O3

.PATH: ${.CURDIR} ${.CURDIR}/../libdigest ${.CURDIR}/../libverify ${.CURDIR}/../libbn ${.CURDIR}/../librsa

.include <bsd.prog.mk>

t: ${PROG}
	./${PROG} -c verify b.gpg > output16
	diff expected16 output16
	rm -f output16
	./${PROG} -c verify a.gpg > output17
	diff expected17 output17
	rm -f output17
	./${PROG} -c verify gpgsigned-a.gpg > output18
	diff expected18 output18
	rm -f output18
	./${PROG} -c verify NetBSD-6.0_RC2_hashes.asc > output19
	diff expected19 output19
	rm -f output19
	./${PROG} -c cat jj.asc > output20
	diff expected20 output20
	rm -f output20
	./${PROG} < a.gpg > output21
	diff expected21 output21
	rm -f output21
	./${PROG} < jj.asc > output22
	diff expected22 output22
	rm -f output22
	./${PROG} < NetBSD-6.0_RC2_hashes.asc > output23
	diff expected23 output23
	rm -f output23
	./${PROG} < b.gpg > output24
	diff expected24 output24
	rm -f output24
	./${PROG} NetBSD-6.0_RC1_hashes.gpg > output25
	diff expected25 output25
	rm -f output25
	./${PROG} < NetBSD-6.0_RC1_hashes.gpg > output26
	diff expected26 output26
	rm -f output26
	./${PROG} < NetBSD-6.0_hashes.asc > output27
	diff expected27 output27
	rm -f output27
	./${PROG} NetBSD-6.0_hashes.asc > output28
	diff expected28 output28
	rm -f output28
	./${PROG} NetBSD-6.0_RC1_hashes_ascii.gpg > output29
	diff expected29 output29
	rm -f output29
	./${PROG} < NetBSD-6.0_RC1_hashes_ascii.gpg > output30
	diff expected30 output30
	rm -f output30
