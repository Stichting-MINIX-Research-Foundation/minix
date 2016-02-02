# $NetBSD: Makefile,v 1.1 2011/04/13 19:16:58 elric Exp $

.include "bsd.own.mk"

.if (${MKKERBEROS} != "no")
SUBDIR =		\
	hprop		\
	iprop-log	\
	kadmin		\
	kcm		\
	kdc		\
	kdigest		\
	kimpersonate	\
	kstash		\
	ktutil

.endif	# MKKERBEROS != no

.include <bsd.subdir.mk>
