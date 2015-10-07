# $NetBSD: Makefile,v 1.4 2012/11/22 21:20:44 agc Exp $

.include <bsd.own.mk>

PROG=netpgpverify
SRCS+=main.c
BINDIR=/usr/bin

WARNS=5
MAN=netpgpverify.1

CPPFLAGS+=-I${EXTDIST}/libverify

LIBNETPGPVERIFYDIR!=   cd ${.CURDIR}/../../lib/verify && ${PRINTOBJDIR}
LDADD+=         -L${LIBNETPGPVERIFYDIR} -lnetpgpverify
DPADD+=         ${LIBNETPGPVERIFYDIR}/libnetpgpverify.a

LDADD+=         -lz -lbz2
DPADD+=         ${LIBZ} ${LIBBZ2}

EXTDIST= ${.CURDIR}/../../dist/src
.PATH: ${EXTDIST}/netpgpverify

.include <bsd.prog.mk>

t: ${PROG}
	env LD_LIBRARY_PATH=${LIBNETPGPVERIFYDIR} ./${PROG} -c verify b.gpg > output16
	diff expected16 output16
	rm -f output16
	env LD_LIBRARY_PATH=${LIBNETPGPVERIFYDIR} ./${PROG} -c verify a.gpg > output17
	diff expected17 output17
	rm -f output17
	env LD_LIBRARY_PATH=${LIBNETPGPVERIFYDIR} ./${PROG} -c verify gpgsigned-a.gpg > output18
	diff expected18 output18
	rm -f output18
	env LD_LIBRARY_PATH=${LIBNETPGPVERIFYDIR} ./${PROG} -c verify NetBSD-6.0_RC2_hashes.asc > output19
	diff expected19 output19
	rm -f output19
	env LD_LIBRARY_PATH=${LIBNETPGPVERIFYDIR} ./${PROG} -c cat jj.asc > output20
	diff expected20 output20
	rm -f output20
	env LD_LIBRARY_PATH=${LIBNETPGPVERIFYDIR} ./${PROG} < a.gpg > output21
	diff expected21 output21
	rm -f output21
	env LD_LIBRARY_PATH=${LIBNETPGPVERIFYDIR} ./${PROG} < jj.asc > output22
	diff expected22 output22
	rm -f output22
	env LD_LIBRARY_PATH=${LIBNETPGPVERIFYDIR} ./${PROG} < NetBSD-6.0_RC2_hashes.asc > output23
	diff expected23 output23
	rm -f output23
	env LD_LIBRARY_PATH=${LIBNETPGPVERIFYDIR} ./${PROG} < b.gpg > output24
	diff expected24 output24
	rm -f output24
	env LD_LIBRARY_PATH=${LIBNETPGPVERIFYDIR} ./${PROG} NetBSD-6.0_RC1_hashes.gpg > output25
	diff expected25 output25
	rm -f output25
	env LD_LIBRARY_PATH=${LIBNETPGPVERIFYDIR} ./${PROG} < NetBSD-6.0_RC1_hashes.gpg > output26
	diff expected26 output26
	rm -f output26
	env LD_LIBRARY_PATH=${LIBNETPGPVERIFYDIR} ./${PROG} < NetBSD-6.0_hashes.asc > output27
	diff expected27 output27
	rm -f output27
	env LD_LIBRARY_PATH=${LIBNETPGPVERIFYDIR} ./${PROG} NetBSD-6.0_hashes.asc > output28
	diff expected28 output28
	rm -f output28
	env LD_LIBRARY_PATH=${LIBNETPGPVERIFYDIR} ./${PROG} NetBSD-6.0_RC1_hashes_ascii.gpg > output29
	diff expected29 output29
	rm -f output29
	env LD_LIBRARY_PATH=${LIBNETPGPVERIFYDIR} ./${PROG} < NetBSD-6.0_RC1_hashes_ascii.gpg > output30
	diff expected30 output30
	rm -f output30
	env LD_LIBRARY_PATH=${LIBNETPGPVERIFYDIR} ./${PROG} -c cat b.gpg b.gpg b.gpg > output31
	diff expected31 output31
	rm -f output31
	env LD_LIBRARY_PATH=${LIBNETPGPVERIFYDIR} ./${PROG} b.gpg b.gpg b.gpg > output32
	diff expected32 output32
	rm -f output32
	env LD_LIBRARY_PATH=${LIBNETPGPVERIFYDIR} ./${PROG} -c cat b.gpg jj.asc b.gpg > output33
	diff expected33 output33
	rm -f output33
	env LD_LIBRARY_PATH=${LIBNETPGPVERIFYDIR} ./${PROG} det.sig > output34
	diff expected34 output34
	rm -f output34
	env LD_LIBRARY_PATH=${LIBNETPGPVERIFYDIR} ./${PROG} -c cat det.sig > output35
	diff expected35 output35
	rm -f output35
	env LD_LIBRARY_PATH=${LIBNETPGPVERIFYDIR} ./${PROG} -k dsa-pubring.gpg in1.gpg > output36
	diff expected36 output36
	rm -f output36
	env LD_LIBRARY_PATH=${LIBNETPGPVERIFYDIR} ./${PROG} -k dsa-pubring.gpg < in1.gpg > output37
	diff expected37 output37
	rm -f output37
	env LD_LIBRARY_PATH=${LIBNETPGPVERIFYDIR} ./${PROG} -k dsa-pubring.gpg in1.asc > output38
	diff expected38 output38
	rm -f output38
	env LD_LIBRARY_PATH=${LIBNETPGPVERIFYDIR} ./${PROG} -k dsa-pubring.gpg < in1.asc > output39
	diff expected39 output39
	rm -f output39
	env LD_LIBRARY_PATH=${LIBNETPGPVERIFYDIR} ./${PROG} -k dsa-pubring.gpg -c cat in1.gpg > output40
	diff expected40 output40
	rm -f output40
	env LD_LIBRARY_PATH=${LIBNETPGPVERIFYDIR} ./${PROG} -k dsa-pubring.gpg -c cat < in1.gpg > output41
	diff expected41 output41
	rm -f output41
	env LD_LIBRARY_PATH=${LIBNETPGPVERIFYDIR} ./${PROG} -k dsa-pubring.gpg -c cat in1.asc > output42
	diff expected42 output42
	rm -f output42
	env LD_LIBRARY_PATH=${LIBNETPGPVERIFYDIR} ./${PROG} -k dsa-pubring.gpg -c cat < in1.asc > output43
	diff expected43 output43
	rm -f output43
	env LD_LIBRARY_PATH=${LIBNETPGPVERIFYDIR} ./${PROG} -k dsa-pubring.gpg in2.gpg > output44
	diff expected44 output44
	rm -f output44
	env LD_LIBRARY_PATH=${LIBNETPGPVERIFYDIR} ./${PROG} -k dsa-pubring.gpg in2.asc > output45
	diff expected45 output45
	rm -f output45
	env LD_LIBRARY_PATH=${LIBNETPGPVERIFYDIR} ./${PROG} -k problem-pubring.gpg NetBSD-6.0_hashes.asc > output46
	diff expected46 output46
	rm -f output46
