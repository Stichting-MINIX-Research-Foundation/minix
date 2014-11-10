#	$NetBSD: Makefile,v 1.4 2012/11/17 23:08:38 uwe Exp $

CONFIGFILES=		dot.cshrc dot.klogin dot.login dot.profile dot.shrc

FILESDIR=		/root
FILESMODE=		644
FILESMODE_dot.klogin=	600
.for F in ${CONFIGFILES}
FILESNAME_${F}=		${F:S/dot//}
.endfor

.include <bsd.own.mk>
.if !defined(__MINIX)
CONFIGLINKS=		/root/.cshrc	/.cshrc \
			/root/.profile	/.profile
.endif # !defined(__MINIX)

.include <bsd.files.mk>
.include <bsd.links.mk>
