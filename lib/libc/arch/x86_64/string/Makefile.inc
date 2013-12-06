#	$NetBSD: Makefile.inc,v 1.6 2013/11/24 03:50:17 christos Exp $

# objects built from assembler sources
SRCS+=	bcmp.S bcopy.S ffs.S memchr.S memcmp.S \
	memcpy.S memmove.S memset.S strcat.S strchr.S \
	strcmp.S strcpy.S strlen.S strncmp.S strrchr.S swab.S
NO_SRCS+= bzero.c
