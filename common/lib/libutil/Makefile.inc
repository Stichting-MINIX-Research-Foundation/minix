#	$NetBSD: Makefile.inc,v 1.4 2011/11/13 21:56:53 christos Exp $
.PATH.c: ${.PARSEDIR}
.if defined(__MINIX)
SRCS+=	snprintb.c
.else
SRCS+=	snprintb.c proc_compare.c getfstypename.c
.endif
COPTS.snprintb.c = -Wno-format-nonliteral
