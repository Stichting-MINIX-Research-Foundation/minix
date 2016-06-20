#	$NetBSD: Makefile,v 1.6 2005/05/14 17:58:56 tshiozak Exp $

.include <bsd.own.mk>

LIB=	intl
SRCS=	gettext.c textdomain.c gettext_iconv.c gettext_dummy.c strhash.c \
	sysdep.c plural_parser.c
INCS=	libintl.h
INCSDIR=/usr/include

#CFLAGS+=-g
CPPFLAGS+=-I${.CURDIR} -I${.CURDIR}/../libc

MAN=	gettext.3
MLINKS=	gettext.3 dgettext.3 gettext.3 dcgettext.3 \
	gettext.3 ngettext.3 gettext.3 dngettext.3 \
	gettext.3 dcngettext.3 gettext.3 textdomain.3 \
	gettext.3 bindtextdomain.3 gettext.3 bind_textdomain_codeset.3

.include <bsd.lib.mk>
