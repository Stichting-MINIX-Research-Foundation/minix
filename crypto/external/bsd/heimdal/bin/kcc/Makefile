# $NetBSD: Makefile,v 1.2 2011/05/25 19:21:16 he Exp $

.include <bsd.own.mk>
.include <${.CURDIR}/../../Makefile.inc>

USE_FORT?= yes	# cryptographic software

.PATH: ${HEIMDIST}/kuser

PROG= kcc
LINKS+= ${BINDIR}/kcc ${BINDIR}/klist

MAN= klist.1

HEIMSRCS= kcc-commands.in

SRCS=	copy_cred_cache.c	\
	kcc.c			\
	klist.c			\
	kswitch.c

CPPFLAGS+= -I${DESTDIR}/usr/include/krb5

LDADD+= -lkafs -lsl
LDADD+= -ledit -lterminfo
DPADD+= ${LIBKAFS} ${LIBSL}
DPADD+= ${LIBEDIT} ${LIBTERMINFO}

.include <${HEIMBASE}/Makefile.rules.inc>
.include <bsd.prog.mk>
