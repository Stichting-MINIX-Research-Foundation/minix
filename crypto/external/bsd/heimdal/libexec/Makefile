# $NetBSD: Makefile,v 1.1 2011/04/13 19:16:56 elric Exp $

.include "bsd.own.mk"

.if (${MKKERBEROS} != "no")

SUBDIR =		\
	digest-service	\
	hpropd		\
	ipropd-master	\
	ipropd-slave	\
	kadmind		\
	kpasswdd

.endif	# MKKERBEROS != no

.include <bsd.subdir.mk>
