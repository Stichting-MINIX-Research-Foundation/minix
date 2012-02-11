#	$NetBSD: Makefile.inc,v 1.6 2009/08/11 17:30:43 dsl Exp $

# objects built from assembler sources
SRCS+=	abs.S div.S labs.S ldiv.S
NO_SRCS+= llabs.S imaxabs.S imaxdiv.S
