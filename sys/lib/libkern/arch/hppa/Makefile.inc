#	$NetBSD: Makefile.inc,v 1.11 2009/09/22 09:57:16 pooka Exp $

SRCS+=	milli.S
SRCS+=	bcopy.S memcpy.S memmove.S

# XXX: spcopy does not really belong in libkern in the first place
.ifndef RUMPKERNEL
SRCS+=	spcopy.S
.endif

SRCS+= ashrdi3.c divdi3.c    
SRCS+= ffs.c bswap16.c bswap32.c
