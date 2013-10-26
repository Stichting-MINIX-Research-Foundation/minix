#	$NetBSD: Makefile,v 1.14 2008/04/06 09:54:37 lukem Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/6/93

.include <bsd.own.mk>

USE_FORT?= yes	# setuid
PROG=	lock
DPADD+= ${LIBCRYPT}
LDADD+= -lcrypt
BINOWN= root
BINMODE=4555

.if (${USE_PAM} != "no")
CPPFLAGS+=-DUSE_PAM
DPADD+= ${LIBPAM} ${PAM_STATIC_DPADD}
LDADD+= -lpam ${PAM_STATIC_LDADD}
.else	# USE_PAM == no
.if (${USE_SKEY} != "no")
CPPFLAGS+=-DSKEY
DPADD+= ${LIBSKEY}
LDADD+= -lskey
.endif
.endif	# USE_PAM == no

.include <bsd.prog.mk>
