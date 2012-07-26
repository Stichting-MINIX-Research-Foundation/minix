#	$NetBSD: Makefile,v 1.2.4.1.2.1 2011/01/09 20:43:29 riz Exp $

LIB=iscexport

.include <bsd.own.mk>

.include "${NETBSDSRCDIR}/lib/libbindexport/Makefile.inc"



DIST=	${IDIST}/lib/isc
.include "${DIST}/api"

.if ${NAMED_USE_PTHREADS} == "yes"
.PATH.c:	${DIST}/unix ${DIST}/nls ${DIST}/pthreads ${DIST}
.else
.PATH.c:	${DIST}/unix ${DIST}/nls ${DIST}/nothreads ${DIST}
.endif

UNIX_SRCS=	app.c backtrace.c dir.c entropy.c errno2result.c file.c \
		fsaccess.c interfaceiter.c keyboard.c net.c os.c resource.c \
		stdio.c stdtime.c strerror.c syslog.c time.c \
		backtrace-emptytbl.c socket.c
NLS_SRCS=	msgcat.c
PTHREAD_SRCS=	condition.c mutex.c thread.c

ISCDRIVER_SRCS= mem.c task.c lib.c timer.c heap.c

SRCS=		assertions.c base32.c base64.c bitstring.c buffer.c \
		bufferlist.c commandline.c error.c event.c \
		hash.c heap.c hex.c hmacmd5.c hmacsha.c inet_aton.c \
		iterated_hash.c lex.c lfsr.c lib.c log.c md5.c \
		mutexblock.c netaddr.c netscope.c ondestroy.c parseint.c \
		portset.c quota.c radix.c random.c ratelimiter.c refcount.c \
		region.c result.c rwlock.c serial.c sha1.c sha2.c sockaddr.c \
		stats.c string.c strtoul.c symtab.c version.c \
		mem_api.c app_api.c socket_api.c task_api.c timer_api.c\
		${UNIX_SRCS} ${NLS_SRCS} ${PTHREAD_SRCS} ${ISCDRIVER_SRCS}

.if (${USE_INET6} == "no")
CPPFLAGS.net.c=	-Wno-error
SRCS+=		ipv6.c
.endif

CPPFLAGS += -DLIBINTERFACE=${LIBINTERFACE} \
	-DLIBAGE=${LIBAGE} -DLIBREVISION=${LIBREVISION}

CPPFLAGS +=  -DUSE_SOCKETIMPREGISTER -DUSE_TIMERIMPREGISTER -DUSE_TASKIMPREGISTER \
	-DUSE_APPIMPREGISTER -DUSE_MEMIMPREGISTER

.include <bsd.lib.mk>
