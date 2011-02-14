#	$NetBSD: Makefile.inc,v 1.1 2007/11/13 15:21:20 ad Exp $
#	@(#)Makefile.inc	8.3 (Berkeley) 10/24/94

.PATH: ${.CURDIR}/misc

# constructor
SRCS+=	initfini.c

# for -fstack-protector
SRCS+=	stack_protector.c
