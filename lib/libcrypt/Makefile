#	$NetBSD: Makefile,v 1.21 2008/08/29 00:02:22 gmcgarry Exp $

USE_SHLIBDIR=	yes

LIB=	crypt

SRCS=	crypt.c md5crypt.c bcrypt.c crypt-sha1.c util.c pw_gensalt.c
SRCS+=	hmac_sha1.c

WARNS=	4

MAN=	crypt.3
MLINKS= crypt.3 encrypt.3 crypt.3 setkey.3


.if make(*.test)
.SUFFIXES: .test
.c.test:
	${CC} -o $@ -DUNIT_TEST -DMAIN ${LDSTATIC} ${.IMPSRC} ${LDADD}
LDADD+= -L${.OBJDIR} -l${LIB} -L${.OBJDIR}/../../libc
DPADD+= lib${LIB}.a
LDSTATIC?= -static
.include <bsd.prog.mk>
.else
.include <bsd.lib.mk>
.endif

.if (defined(HAVE_GCC) && ${HAVE_GCC} == 4) || defined(HAVE_PCC)
COPTS.crypt-sha1.c+=	-Wno-pointer-sign
.endif
