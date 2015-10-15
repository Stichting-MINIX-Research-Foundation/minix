# $NetBSD: Makefile,v 1.5 2014/02/12 04:08:31 jmmv Exp $

.include <bsd.init.mk>

.PATH:		${SRCDIR}/tools/sample

CONFIGFILES=	NetBSD.conf atf-run.hooks common.conf
FILESDIR=	/etc/atf
FILESMODE=	644

.include <bsd.prog.mk> # For clean and cleandir
.include <bsd.files.mk>
