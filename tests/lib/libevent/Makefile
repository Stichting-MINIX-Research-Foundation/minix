# $NetBSD: Makefile,v 1.2 2013/04/11 17:00:12 christos Exp $

TESTSDIR=	${TESTSBASE}/lib/libevent

TESTS_SH=	t_event

.include <bsd.own.mk>
.include <../../../external/bsd/libevent/Makefile.inc>

.PATH:		${LIBEVENT_DIST}/test

BINDIR=		${TESTSDIR}

PROG=		h_event
MAN=		# defined
#__MINIX: Not Supported: regress_thread.c
SRCS=		regress.c regress.gen.c regress_dns.c regress_http.c \
		regress_rpc.c regress_main.c tinytest.c regress_testutils.c \
		regress_minheap.c regress_util.c \
		regress_buffer.c regress_bufferevent.c regress_listener.c \
		regress_zlib.c regress_et.c

.if ${MKCRYPTO} == "yes"
SRCS+=		regress_ssl.c
DPADD+=		${LIBEVENT_OPENSSL} ${LIBSSL} ${LIBCRYPTO}
LDADD+=		-levent_openssl -lssl -lcrypto
CPPFLAGS+=	-DLIBEVENT_CRYPTO
.endif
.if !defined(__MINIX)
DPADD+=		${LIBEVENT} ${LIBEVENT_PTHREADS} ${LIBZ} ${LIBPTHREAD}
LDADD+=		-levent_pthreads -levent -lz -lpthread
.else
DPADD+=		${LIBEVENT} ${LIBZ}
LDADD+=		-levent -lz
.endif # !defined(__MINIX)

.include <bsd.test.mk>
