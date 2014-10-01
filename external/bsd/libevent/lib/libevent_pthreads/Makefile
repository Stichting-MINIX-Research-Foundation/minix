#	$NetBSD: Makefile,v 1.2 2013/09/11 23:04:10 joerg Exp $

LIB=event_pthreads

COPTS+=-pthread

SRCS=		evthread_pthread.c

LIBDPLIBS+= \
	event	${.CURDIR}/../libevent \
	pthread	${NETBSDSRCDIR}/lib/libpthread

.include <bsd.lib.mk>
