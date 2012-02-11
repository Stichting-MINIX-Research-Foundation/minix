#	$NetBSD: Makefile.inc,v 1.5 2009/07/30 20:57:16 dsl Exp $

# objects built from assembler sources
SRCS+=	bcmp.S bcopy.S bzero.S ffs.S memchr.S memcmp.S \
	memcpy.S memmove.S memset.S strcat.S strchr.S \
	strcmp.S strcpy.S strlen.S strncmp.S strrchr.S swab.S
