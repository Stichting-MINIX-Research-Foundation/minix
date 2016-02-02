# $NetBSD: Makefile,v 1.2 2011/05/25 19:21:17 he Exp $

.include "bsd.own.mk"

HEIMBASE=${.CURDIR}/..

.if (${MKKERBEROS} != "no")
SUBDIR = \
	libheimbase	\
	libcom_err	\
	libroken	\
	libvers		\
	.WAIT		\
	libsl		\
	.WAIT		\
	libasn1		\
	libwind		\
	.WAIT		\
	libhx509	\
	.WAIT		\
	libkrb5		\
	.WAIT		\
	libhdb		\
	libkafs		\
	libheimntlm	\
	.WAIT		\
	libkdc		\
	libgssapi	\
	libkadm5srv	\
	libkadm5clnt

.endif	# MKKERBEROS != no

.include <bsd.subdir.mk>
