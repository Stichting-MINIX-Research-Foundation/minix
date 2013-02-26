# $NetBSD: Makefile,v 1.3 2010/11/07 17:46:45 jmmv Exp $

.include <bsd.own.mk>

SRCDIR=		${NETBSDSRCDIR}/external/bsd/atf/dist
.PATH:		${SRCDIR}/atf-run/sample

CONFIGFILES=	NetBSD.conf atf-run.hooks common.conf
FILESDIR=	/etc/atf
FILESMODE=	644

.include <bsd.prog.mk> # For clean and cleandir
.include <bsd.files.mk>
