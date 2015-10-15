#	$NetBSD: Makefile.inc,v 1.1 2014/09/03 19:34:26 matt Exp $

SRCS+=  bzero.S clz.S ctz.S ffs.S strlen.S memset.S

memcpy.o: memcpy.c
memcpy.po: memcpy.c
memcpy.pico: memcpy.c

# disable the asm versions of these because they break the explora.
# the special rules here are to override the suffix rules which seem
# to prefer .S files over .c
.if ${MACHINE} == "evbppc"
bcopy.o: bcopy.c
bcopy.po: bcopy.c
bcopy.pico: bcopy.c
memcmp.o: memcmp.c
memcmp.po: memcmp.c
memcmp.pico: memcmp.c
memmove.o: memmove.c
memmove.po: memmove.c
memmove.pico: memmove.c
.else
SRCS+=	memcmp.S bcopy.S memmove.S
.endif
