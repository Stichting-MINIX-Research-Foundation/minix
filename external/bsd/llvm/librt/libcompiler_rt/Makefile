#	$NetBSD: Makefile,v 1.1 2012/06/17 16:51:32 joerg Exp $

LIB=	compiler_rt
USE_SHLIBDIR=	yes
CWARNFLAGS+=	-Wno-missing-prototypes
WARNS=		4
NOLINT=

.include <bsd.init.mk>

LDFLAGS+=	-nodefaultlibs

# Excluded:
#	apple_versioning.c
#	clear_cache.c
#	enable_execute_stack.c
#	eprintf.c
#	trampoline_setup.c
#
# Must be optimised:
#	atomic.c

GENERIC_SRCS+= \
	absvdi2.c \
	absvsi2.c \
	absvti2.c \
	adddf3.c \
	addsf3.c \
	addvdi3.c \
	addvsi3.c \
	addvti3.c \
	ashldi3.c \
	ashlti3.c \
	ashrdi3.c \
	ashrti3.c \
	atomic.c \
	clzdi2.c \
	clzsi2.c \
	clzti2.c \
	cmpdi2.c \
	cmpti2.c \
	comparedf2.c \
	comparesf2.c \
	ctzdi2.c \
	ctzsi2.c \
	ctzti2.c \
	divdc3.c \
	divdf3.c \
	divdi3.c \
	divmoddi4.c \
	divmodsi4.c \
	divsc3.c \
	divsf3.c \
	divsi3.c \
	divti3.c \
	divxc3.c \
	extendsfdf2.c \
	ffsdi2.c \
	ffsti2.c \
	fixdfdi.c \
	fixdfsi.c \
	fixdfti.c \
	fixsfdi.c \
	fixsfsi.c \
	fixsfti.c \
	fixunsdfdi.c \
	fixunsdfsi.c \
	fixunsdfti.c \
	fixunssfdi.c \
	fixunssfsi.c \
	fixunssfti.c \
	fixunsxfdi.c \
	fixunsxfsi.c \
	fixunsxfti.c \
	fixxfdi.c \
	fixxfti.c \
	floatdidf.c \
	floatdisf.c \
	floatdixf.c \
	floatsidf.c \
	floatsisf.c \
	floattidf.c \
	floattisf.c \
	floattixf.c \
	floatundidf.c \
	floatundisf.c \
	floatundixf.c \
	floatunsidf.c \
	floatunsisf.c \
	floatuntidf.c \
	floatuntisf.c \
	floatuntixf.c \
	gcc_personality_v0.c \
	int_util.c \
	lshrdi3.c \
	lshrti3.c \
	moddi3.c \
	modsi3.c \
	modti3.c \
	muldc3.c \
	muldf3.c \
	muldi3.c \
	mulodi4.c \
	mulosi4.c \
	muloti4.c \
	mulsc3.c \
	mulsf3.c \
	multi3.c \
	mulvdi3.c \
	mulvsi3.c \
	mulvti3.c \
	mulxc3.c \
	negdf2.c \
	negdi2.c \
	negsf2.c \
	negti2.c \
	negvdi2.c \
	negvsi2.c \
	negvti2.c \
	paritydi2.c \
	paritysi2.c \
	parityti2.c \
	popcountdi2.c \
	popcountsi2.c \
	popcountti2.c \
	powidf2.c \
	powisf2.c \
	powitf2.c \
	powixf2.c \
	subdf3.c \
	subsf3.c \
	subvdi3.c \
	subvsi3.c \
	subvti3.c \
	truncdfsf2.c \
	ucmpdi2.c \
	ucmpti2.c \
	udivdi3.c \
	udivmoddi4.c \
	udivmodsi4.c \
	udivmodti4.c \
	udivsi3.c \
	udivti3.c \
	umoddi3.c \
	umodsi3.c \
	umodti3.c

CPU_DIR=	${COMPILER_RT_SRCDIR}/lib/${MACHINE_CPU}
ARCH_DIR=	${COMPILER_RT_SRCDIR}/lib/${MACHINE_ARCH}

.PATH: ${CPU_DIR}
.PATH: ${ARCH_DIR}
.PATH: ${COMPILER_RT_SRCDIR}/lib

.for src in ${GENERIC_SRCS}
.  if exists(${CPU_DIR}/${src:R}.S) || exists(${ARCH_DIR}/${src:R}.S)
SRCS+=	${src:R}.S
.  else
SRCS+=	${src}
.  endif
.endfor

.include <bsd.lib.mk>
