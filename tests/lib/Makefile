# $NetBSD: Makefile,v 1.24 2015/06/22 00:05:23 matt Exp $

.include <bsd.own.mk>

#__MINIX:
#	libbluetooth
#	libobjc libposix libppath libpthread
#	librt libtre semaphore
TESTS_SUBDIRS=	csu libc libcrypt libcurses libevent libexecinfo \
		libm libprop  \
		libutil

.if (${MKRUMP} != "no") && !defined(BSD_MK_COMPAT_FILE)
TESTS_SUBDIRS+= librumpclient librumphijack
.endif

.if ${MKCRYPTO} != "no"
TESTS_SUBDIRS+= libdes
.endif

.if ${MKKYUA} != "no"
# These tests come from external/bsd/lutok/tests/lib/liblutok.  Ensure they
# get registered in the Atffile only when installed.
ATFFILE_EXTRA_TPS=	liblutok
.endif

.if (${MKSKEY} != "no")
TESTS_SUBDIRS+= libskey
.endif

.if ${MKSLJIT} != "no"
TESTS_SUBDIRS+=	libsljit
TESTS_SUBDIRS+=	libbpfjit
.endif

TESTSDIR=	${TESTSBASE}/lib

.include <bsd.test.mk>
