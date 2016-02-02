#	$NetBSD: Makefile,v 1.56 2013/08/11 06:32:13 dholland Exp $
#	@(#)Makefile	8.1 (Berkeley) 7/19/93

.include <bsd.own.mk>

USE_FORT?= yes	# setuid
PROG=	login
SRCS=	copyrightstr.c
DPADD+=	${LIBUTIL} ${LIBCRYPT}
LDADD+=	-lutil -lcrypt
BINOWN=	root
BINMODE=4555

SRCS+=	common.c
.if (${USE_PAM} != "no")
SRCS+=	login_pam.c
LDADD+=	-lpam ${PAM_STATIC_LDADD}
DPADD+= ${LIBPAM} ${PAM_STATIC_DPADD}
.else	# USE_PAM == no
SRCS+=	login.c
# MINIX: -DLOGIN_CAP
CPPFLAGS+= -DSUPPORT_UTMP -DSUPPORT_UTMPX

.if (${USE_KERBEROS} != "no")
SRCS+= k5login.c
CPPFLAGS+=-DKERBEROS5
DPADD+=	${LIBKRB5} ${LIBASN1}
LDADD+= -lkrb5 -lasn1

DPADD+=	${LIBCRYPTO} ${LIBROKEN} ${LIBCOM_ERR}
LDADD+=	-lcrypto -lroken -lcom_err
.endif

.if (${USE_SKEY} != "no")
CPPFLAGS+=-DSKEY
DPADD+= ${LIBSKEY}
LDADD+=	-lskey
.endif
.endif	# USE_PAM == no

CLEANFILES+=	copyrightstr.c

copyrightstr.c: ${NETBSDSRCDIR}/sys/conf/copyright
	${_MKTARGET_CREATE}
	rm -f ${.TARGET}
	${TOOL_AWK} '\
		BEGIN { print "const char copyrightstr[] =" }\
		{ print "\""$$0"\\n\""}\
		END { print "\"\\n\";" }\
	' ${.ALLSRC} > ${.TARGET}


.include <bsd.prog.mk>
