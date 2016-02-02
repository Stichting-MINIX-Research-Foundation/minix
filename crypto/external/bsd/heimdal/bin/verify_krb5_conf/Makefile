#	$NetBSD: Makefile,v 1.2 2011/05/25 19:21:17 he Exp $

WARNS?=	2	# XXX -Wextra

NOLINT= # defined

.include <bsd.own.mk>
.include <${.CURDIR}/../../Makefile.inc>

.PATH: ${HEIMDIST}/lib/krb5

PROG= verify_krb5_conf
MAN= verify_krb5_conf.8

SRCS= verify_krb5_conf.c

#CPPFLAGS+= -I.					\
#	-I${DIST}/heimdal/lib/asn1		\
#	-I${NETBSDSRCDIR}/include/heimdal	\
#	-I${DESTDIR}/usr/include/krb5		\
#	-DHAVE_CONFIG_H

.include <${HEIMBASE}/Makefile.rules.inc>
.include <bsd.prog.mk>
