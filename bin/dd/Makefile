#	$NetBSD: Makefile,v 1.18 2015/03/18 13:23:49 manu Exp $
#	@(#)Makefile	8.1 (Berkeley) 5/31/93

.include <bsd.own.mk>

RUMPPRG=dd
SRCS=	args.c conv.c dd.c misc.c position.c

DPADD+=	${LIBUTIL}
LDADD+=	-lutil

.ifdef SMALLPROG
CPPFLAGS+=	-DNO_CONV -DNO_MSGFMT -DNO_IOFLAG -DSMALL
.else
CPPFLAGS+=	-D_NETBSD_SOURCE -D_INCOMPLETE_XOPEN_C063
SRCS+=		conv_tab.c
.ifdef CRUNCHEDPROG
CPPFLAGS+=	-DSMALL
.endif
.endif

.include <bsd.prog.mk>
