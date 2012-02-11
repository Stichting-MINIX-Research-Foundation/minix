#	$NetBSD: Makefile,v 1.5 2007/07/02 18:41:03 christos Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/6/93

PROG=	cut

.if ${COMPILER_TYPE} != "gnu"
LDADD+=	-lminixutil
DPADD+=	${LIBMINIXUTIL}
.else
LDADD+=	-lutil
DPADD+=	${LIBUTIL}
.endif

.include <bsd.prog.mk>
