#	$NetBSD: Makefile,v 1.31 2014/12/02 03:51:48 msaitoh Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/5/93

.include <bsd.init.mk>

# missing: eqnchar.7 man.7 ms.7 term.7

# MINIX:
MAN=	\
	nls.7 \
	\
	

.if !defined(__MINIX)
CLEANFILES=	tests.7
.if ${MKKYUA} != "no"
tests.7: tests.kyua.7
	rm -f ${.TARGET} && cp ${.ALLSRC} ${.TARGET}
.else
tests.7: tests.atf.7
	rm -f ${.TARGET} && cp ${.ALLSRC} ${.TARGET}
.endif

MLINKS+=c.7 c78.7 \
	c.7 c89.7 \
	c.7 c90.7 \
	c.7 c99.7
.endif # !defined(__MINIX)

.include <bsd.man.mk>
