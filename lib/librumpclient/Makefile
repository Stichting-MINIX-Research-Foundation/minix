#	$NetBSD: Makefile,v 1.10 2013/09/10 17:39:10 pooka Exp $
#

.PATH:	${.CURDIR}/../../sys/rump/librump/rumpkern

LIB=		rumpclient
USE_SHLIBDIR=	yes
MAN=		rumpclient.3

.if defined(__MINIX)
RUMPUSER_THREADS=fiber
.endif

.include <bsd.own.mk>

INCS=		rumpclient.h
INCSDIR=	/usr/include/rump

.for lib in ${RUMPCLIENT_EXTERNAL_DPLIBS}
LIBDO.${lib}=	_external
LIBDPLIBS+=	${lib} lib
.endfor

CPPFLAGS+=	-DRUMP_CLIENT
CPPFLAGS+=	-I${.OBJDIR} -I${.CURDIR}
CPPFLAGS+=	-I${.CURDIR}/../librumpuser
SRCS=		rumpclient.c
SRCS+=		rump_syscalls.c

CLEANFILES+=	srcsys

COPTS.rump_syscalls.c+= -fno-strict-aliasing

.if !make(obj) && !make(clean) && !make(cleandir)
.BEGIN:
	@rm -f srcsys
	@ln -s ${NETBSDSRCDIR}/sys/sys srcsys
.endif

.include <bsd.lib.mk>
