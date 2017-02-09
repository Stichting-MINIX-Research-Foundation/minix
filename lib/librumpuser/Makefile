#	$NetBSD: Makefile,v 1.23 2014/11/09 17:39:37 pooka Exp $
#

.include <bsd.own.mk>

WARNS?=		5

# rumpuser.h is in sys/rump for inclusion by kernel components
.PATH:		${.CURDIR}/../../sys/rump/include/rump

.if defined(__MINIX)
RUMPUSER_THREADS=fiber
.else
RUMPUSER_THREADS?=pthread
.endif

LIB=		rumpuser

.if   ${RUMPUSER_THREADS} == "pthread"
LIBDPLIBS+=	pthread ${.CURDIR}/../libpthread
.endif
.for lib in ${RUMPUSER_EXTERNAL_DPLIBS}
LIBDO.${lib}=	_external
LIBDPLIBS+=	${lib} lib
.endfor
CPPFLAGS+=	-DLIBRUMPUSER
#CPPFLAGS+=	-D_DIAGNOSTIC


.if   ${RUMPUSER_THREADS} == "pthread"
SRCS=		rumpuser.c rumpuser_pth.c rumpuser_bio.c
SRCS+=		rumpuser_sp.c
.elif ${RUMPUSER_THREADS} == "none"
SRCS=		rumpuser.c rumpuser_pth_dummy.c rumpuser_bio.c
SRCS+=		rumpuser_sp.c
.elif ${RUMPUSER_THREADS} == "fiber"
.if defined(RUMP_CURLWP)
.if ${RUMP_CURLWP} != "hypercall"
.error Unsupported curlwp scheme for thread model: ${RUMP_CURLWP}
.endif
.endif
SRCS=		rumpfiber.c rumpfiber_bio.c
SRCS+=		rumpfiber_sp.c
.else
.error Unsupported rumpuser threading type: ${RUMPUSER_THREADS}
.endif

SRCS+=		rumpuser_component.c rumpuser_random.c
SRCS+=		rumpuser_file.c rumpuser_mem.c

SRCS+=		rumpuser_errtrans.c rumpuser_sigtrans.c

# optional
SRCS+=		rumpuser_dl.c rumpuser_daemonize.c

INCSDIR=	/usr/include/rump
INCS=		rumpuser_component.h rumpuser_port.h

CPPFLAGS+=	-D_REENTRANT

.include <bsd.lib.mk>
