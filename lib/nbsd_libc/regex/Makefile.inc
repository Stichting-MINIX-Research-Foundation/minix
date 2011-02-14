#	$NetBSD: Makefile.inc,v 1.7 1997/11/14 02:04:46 mrg Exp $
#	@(#)Makefile.inc	8.1 (Berkeley) 6/4/93

# regex sources
.PATH: ${.CURDIR}/regex

CPPFLAGS+=-DPOSIX_MISTAKE

SRCS+=	regcomp.c regerror.c regexec.c regfree.c

MAN+=	regex.3 re_format.7

MLINKS+=regex.3 regcomp.3 regex.3 regexec.3 regex.3 regerror.3 \
	regex.3 regfree.3
