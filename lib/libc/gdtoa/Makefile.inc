#	$NetBSD: Makefile.inc,v 1.9 2013/04/19 20:18:33 joerg Exp $

# gdtoa sources
.PATH: ${.CURDIR}/gdtoa
CPPFLAGS+=-I${.CURDIR}/gdtoa -I${.CURDIR}/locale

CPPFLAGS+=-DNO_FENV_H

# machine-dependent directory must provide the following:
# 	arith.h gd_qnan.h
CPPFLAGS+=-I${ARCHDIR}/gdtoa

# machine-dependent Makefile must define names of modules to use for
# float conversions:
# 	strtof(): strtof{,_vaxf}.c
# and for long double conversions on platforms with extended-precision
# format, if supported (strtod.c will define an alias otherwise):
#	strtold(): strtold_p{x,xL,Q}.c
#	helpers:   strtop{x,xL,Q}.c
.include "${ARCHDIR}/gdtoa/Makefile.inc"

# public interfaces
SRCS+=	strtod.c

# private interfaces interfacing to libc
SRCS+=	dtoa.c ldtoa.c hdtoa.c gdtoa.c

.if !defined(__MINIX)
# local locking implementation
SRCS+=	gdtoa_locks.c
.endif # !defined(__MINIX)

# private interfaces
SRCS+=	dmisc.c \
	gethex.c gmisc.c \
	hd_init.c hexnan.c \
	misc.c \
	smisc.c strtodg.c sum.c \
	ulp.c

# wrappers
SRCS+=	_strtof.c _strtold.c
#SRCS+=	_hdtoa.c _ldtoa.c

# XXX no hexadecimal floating-point string conversion for VAX FP yet
.if ${MACHINE_ARCH} != "vax"
SRCS+=	strtord.c
.endif
