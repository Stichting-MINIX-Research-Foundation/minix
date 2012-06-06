#	$NetBSD: Makefile,v 1.1 2009/08/18 20:22:20 skrll Exp $

.include <bsd.own.mk>

SUBDIR=		dbsym mdsetimage ncdcs

# Speedup stubs for some subtrees that don't need to run these rules
includes-dbsym includes-mdsetimage:
	@true

.include <bsd.subdir.mk>
