# $NetBSD: Makefile.inc,v 1.9 2013/08/21 03:05:35 matt Exp $

SRCS.string+=	memcmp.S memcpy.S memmove.S memset.S
SRCS.string+=	ffs.S
SRCS.string+=	bcopy.S bzero.S
SRCS.string+=	strcat.S
SRCS.string+=	strcpy.S strlcpy.S strncpy.S
.if empty(LIBC_MACHINE_ARCH:M*v7*)
SRCS.string+=	strcmp.S strncmp.S 
SRCS.string+=	strlen.S strnlen.S 
SRCS.string+=	strchr.S strrchr.S 
.endif

.for f in ${SRCS.string}
CPUFLAGS.${f}+=		-marm
.for s in o po pico ln d
${f:.S=.${s}}:		${ARCHDIR}/string/Makefile.inc
.endfor
.endfor

.if !empty(LIBC_MACHINE_ARCH:M*v7*)
SRCS.string+=	strcmp.S strncmp.S 
SRCS.string+=	strlen.S strnlen.S 
SRCS.string+=	strchr.S strrchr.S 
.endif

SRCS.string+=	strlcat.S

SRCS+=	${SRCS.string}
