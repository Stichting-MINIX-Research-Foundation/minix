#	$NetBSD: Makefile,v 1.33 2012/10/05 01:26:56 christos Exp $
#	from: @(#)Makefile	8.2 (Berkeley) 4/27/95

.include <bsd.own.mk>

PROG=	mtree
#CPPFLAGS+=-DDEBUG
CPPFLAGS+= -DMTREE
MAN=	mtree.8
SRCS=	compare.c crc.c create.c excludes.c misc.c mtree.c spec.c specspec.c \
	verify.c getid.c pack_dev.c
.if (${HOSTPROG:U} == "")
.if defined(__MINIX)
CPPFLAGS+= -Dlchown=chown -Dlchmod=chmod 
.endif
DPADD+= ${LIBUTIL}
LDADD+= -lutil
.endif

CPPFLAGS+=	-I${NETBSDSRCDIR}/sbin/mknod
.PATH:		${NETBSDSRCDIR}/sbin/mknod

.include <bsd.prog.mk>
