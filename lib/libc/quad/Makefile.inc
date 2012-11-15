#	$NetBSD: Makefile.inc,v 1.14 2012/08/06 08:17:58 matt Exp $
#	@(#)Makefile.inc	8.1 (Berkeley) 6/4/93

# Quad support
SRCS.quad=	cmpdi2.c divdi3.c fixdfdi.c fixsfdi.c fixunsdfdi.c \
	fixunssfdi.c floatdidf.c floatdisf.c floatundisf.c floatundidf.c \
	moddi3.c muldi3.c negdi2.c qdivrem.c \
	ucmpdi2.c udivdi3.c umoddi3.c 

.if   (${MACHINE_ARCH} == "m68k")
SRCS.quad+=	ashldi3.S ashrdi3.S lshrdi3.S
.elif (${MACHINE_ARCH} == "m68000")
SRCS.quad+=	ashldi3.S lshrdi3.S
.elif (${MACHINE_ARCH} == "earm" || ${MACHINE_ARCH} == "earmeb")
SRCS.quad+=	ashrdi3.c lshldi3.c lshrdi3.c
.else
SRCS.quad+=	ashldi3.c ashrdi3.c lshrdi3.c
.endif

# XXX as far as I can tell, these are never used and can be removed
SRCS.quad+=	adddi3.c anddi3.c iordi3.c notdi2.c subdi3.c xordi3.c

SRCS+=	${SRCS.quad}

.PATH: ${ARCHDIR}/quad ${.CURDIR}/quad
