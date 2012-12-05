#	$NetBSD: Makefile,v 1.8 2009/09/10 22:02:40 dsl Exp $
#	from: @(#)Makefile	8.1 (Berkeley) 6/6/93

PROG=	sort
SRCS=	append.c fields.c files.c fsort.c init.c msort.c sort.c tmp.c
SRCS+=	radix_sort.c

LDADD+=-lutil
DPADD+=${LIBUTIL}

.if defined(__MINIX)
CPPFLAGS+= -Dlchown=chown -Dlchmod=chmod
.endif # defined(__MINIX)

.include <bsd.prog.mk>
