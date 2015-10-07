# $NetBSD: Makefile,v 1.1 2011/04/13 19:16:44 elric Exp $

.include <bsd.own.mk>

HEIMBASE=${.CURDIR}/..

.if (${MKKERBEROS} != "no")
SUBDIR =		\
	gsstool		\
	hxtool		\
	kcc		\
	kdestroy	\
	kgetcred	\
	kinit		\
	kpasswd		\
	krb5-config	\
	string2key	\
	verify_krb5_conf

.endif	# MKKERBEROS != no

.include <bsd.subdir.mk>
