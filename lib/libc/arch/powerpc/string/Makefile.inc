#	$NetBSD: Makefile.inc,v 1.13 2010/03/14 00:26:10 mrg Exp $

SRCS+=  bzero.S ffs.S strlen.S
NO_SRCS+= memset.S

# disable the asm versions of these because they break the explora.
# the special rules here are to override the suffix rules which seem
# to prefer .S files over .c
.if ${MACHINE} == "evbppc"
bcopy.o: bcopy.c
bcopy.po: bcopy.c
bcopy.pico: bcopy.c
memcpy.o: memcpy.c
memcpy.po: memcpy.c
memcpy.pico: memcpy.c
memcmp.o: memcmp.c
memcmp.po: memcmp.c
memcmp.pico: memcmp.c
memmove.o: memmove.c
memmove.po: memmove.c
memmove.pico: memmove.c
.else
SRCS+=	memcmp.S bcopy.S memcpy.S memmove.S
.endif
