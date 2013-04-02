PROG=	ntpd
SRCS=	buffer.c client.c config.c imsg.c log.c ntp.c ntp_msg.c ntpd.c \
	server.c util.c y.tab.c
MAN=	ntpd.conf.5 ntpd.8
.include <bsd.own.mk>

DIST=		${NETBSDSRCDIR}/external/bsd/openntpd/dist
CPPFLAGS+=	-I${DIST}
LIBOPENBSDCOMPAT!=	cd ${.CURDIR}/../../lib/openbsd-compat && ${PRINTOBJDIR}
LDADD+=		-L${LIBOPENBSDCOMPAT} -lopenbsd-compat
DPADD+=		${LIBOPENBSDCOMPAT}/libopenbsd-compat.a

.PATH: ${DIST}

.include <bsd.prog.mk>
