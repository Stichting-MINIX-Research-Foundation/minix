#	$NetBSD: Makefile.inc,v 1.6 2015/04/17 12:51:05 martin Exp $

SRCS+=	_lwp.c
SRCS+=	bswap16.c bswap32.c bswap64.c
SRCS+=	setjmp.S _setjmp.S sigsetjmp.S
SRCS+=	flt_rounds.c fpgetround.c fpsetround.c fpgetmask.c fpsetmask.c
SRCS+=	isinff_ieee754.c isinfd_ieee754.c
SRCS+=	isnanf_ieee754.c isnand_ieee754.c
SRCS+=	fpclassifyf_ieee754.c fpclassifyd_ieee754.c

