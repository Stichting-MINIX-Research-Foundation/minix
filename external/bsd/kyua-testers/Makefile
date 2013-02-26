# $NetBSD: Makefile,v 1.2 2013/02/23 14:58:11 jmmv Exp $

SUBDIR= lib .WAIT libexec share tests

.for dir in libexec share tests
includes-${dir}: .PHONY
	@true
.endfor

.include <bsd.subdir.mk>
