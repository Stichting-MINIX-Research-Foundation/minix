#	$NetBSD: Makefile,v 1.36 2012/12/21 18:07:36 christos Exp $
#	from: @(#)Makefile	8.2 (Berkeley) 4/3/94

.include <bsd.own.mk>

USE_FORT?= yes	# network client

PROG=	ftp
SRCS=	cmds.c cmdtab.c complete.c domacro.c fetch.c ftp.c main.c \
	progressbar.c ruserpass.c util.c

# Uncomment the following to provide defaults for gate-ftp operation
#
#CPPFLAGS+=-DGATE_SERVER=\"ftp-gw.host\" # -DGATE_PORT=21

.if defined(__MINIX)
CPPFLAGS+= -DDIRENT_MISSING_D_NAMLEN
.endif # defined (__MINIX)

.if defined(SMALLPROG)
CPPFLAGS+=-DNO_EDITCOMPLETE -DNO_ABOUT -DNO_AUTH -DNO_HELP -DNO_STATUS -DNO_DEBUG -DNO_USAGE
.else
LDADD+=	-ledit -lterminfo
DPADD+=	${LIBEDIT} ${LIBTERMINFO}
.if (${MKCRYPTO} != "no")
CPPFLAGS+= -DWITH_SSL
SRCS+=ssl.c
LDADD+= -lssl -lcrypto
DPADD+= ${LIBSSL} ${LIBCRYPTO}
.endif
.endif

.if (!defined(SMALLPROG) || defined(SMALLPROG_INET6)) && (${USE_INET6} != "no")
CPPFLAGS+= -DINET6
.endif

cmds.o fetch.o: version.h
main.o:	ftp_var.h

.include <bsd.prog.mk>
