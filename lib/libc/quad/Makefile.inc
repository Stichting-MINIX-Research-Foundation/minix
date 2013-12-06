#	$NetBSD: Makefile.inc,v 1.17 2013/08/24 00:51:48 matt Exp $
#	@(#)Makefile.inc	8.1 (Berkeley) 6/4/93

# Quad support
SRCS.quad=	cmpdi2.c ucmpdi2.c 
SRCS.quad+=	divdi3.c moddi3.c 
.if empty(LIBC_MACHINE_ARCH:Mearm*)
SRCS.quad+=	fixdfdi.c fixsfdi.c
SRCS.quad+=	fixunsdfdi.c fixunssfdi.c
SRCS.quad+=	floatdidf.c floatdisf.c
SRCS.quad+=	floatundidf.c floatundisf.c
.endif
SRCS.quad+=	muldi3.c
SRCS.quad+=	negdi2.c
SRCS.quad+=	qdivrem.c
SRCS.quad+=	udivdi3.c umoddi3.c 

.if   (${MACHINE_ARCH} == "m68k")
SRCS.quad+=	ashldi3.S ashrdi3.S lshrdi3.S
.elif (${MACHINE_ARCH} == "m68000")
SRCS.quad+=	ashldi3.S lshrdi3.S
.elif !empty(LIBC_MACHINE_ARCH:Mearm*)
SRCS.quad+=	ashldi3.c ashrdi3.c lshrdi3.c lshldi3.c
SRCS.quad+=	floatdidf_ieee754.c floatdisf_ieee754.c
SRCS.quad+=	floatundidf_ieee754.c floatundisf_ieee754.c
SRCS.quad+=	fixdfdi_ieee754.c fixsfdi_ieee754.c
SRCS.quad+=	fixunsdfdi_ieee754.c fixunssfdi_ieee754.c
.else
SRCS.quad+=	ashldi3.c ashrdi3.c lshrdi3.c
.endif

# XXX as far as I can tell, these are never used and can be removed
SRCS.quad+=	adddi3.c anddi3.c iordi3.c notdi2.c subdi3.c xordi3.c

SRCS+=	${SRCS.quad}

.PATH: ${ARCHDIR}/quad ${.CURDIR}/quad
