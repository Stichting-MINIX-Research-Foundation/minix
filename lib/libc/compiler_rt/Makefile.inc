# $NetBSD: Makefile.inc,v 1.27 2014/09/03 19:29:14 matt Exp $

COMPILER_RT_SRCDIR=	${NETBSDSRCDIR}/sys/external/bsd/compiler_rt/dist

.if ${LIBC_MACHINE_ARCH} == "powerpc" || ${LIBC_MACHINE_ARCH} == "powerpc64"
COMPILER_RT_CPU_DIR=	${COMPILER_RT_SRCDIR}/lib/builtins/ppc
COMPILER_RT_ARCH_DIR=	${COMPILER_RT_SRCDIR}/lib/builtins/ppc
.else
COMPILER_RT_CPU_DIR=	${COMPILER_RT_SRCDIR}/lib/builtins/${LIBC_MACHINE_CPU}
COMPILER_RT_ARCH_DIR=	${COMPILER_RT_SRCDIR}/lib/builtins/${LIBC_MACHINE_ARCH}
.endif

.if defined(__MINIX)
LIBC_MACHINE_CPU?=      ${MACHINE_CPU}
.endif # defined(__MINIX)

.PATH: ${COMPILER_RT_CPU_DIR}
.PATH: ${COMPILER_RT_ARCH_DIR}
.PATH: ${COMPILER_RT_SRCDIR}/lib/builtins
.PATH: ${COMPILER_RT_SRCDIR}/lib/profile

# Complex support needs parts of libm
#GENERIC_SRCS+=
#	mulxc3.c \
#	mulsc3.c \
#	divxc3.c \
#	divdc3.c \
#	divsc3.c

# Implemented on top of our atomic interface.
#GENERIC_SRCS+= atomic.c

.if ${HAVE_LIBGCC_EH} == "no"
GENERIC_SRCS+= \
	gcc_personality_v0.c
.endif

.if 0
# Conflicts with soft-float
GENERIC_SRCS+= \
	comparedf2.c \
	comparesf2.c \
	adddf3.c \
	addsf3.c \
	addtf3.c \
	divdf3.c \
	divsf3.c \
	divtf3.c \
	extendsfdf2.c \
	extendsftf2.c \
	extenddftf2.c \
	fixdfsi.c \
	fixdfti.c \
	fixsfsi.c \
	fixsfti.c \
	floatsidf.c \
	floatsisf.c \
	floatunsidf.c \
	floatunsisf.c \
	muldf3.c \
	mulsf3.c \
	multf3.c \
	subdf3.c \
	subsf3.c \
	subtf3.c \
	truncdfsf2.c \
	trunctfdf2.c \
	trunctfsf2.c
.endif

GENERIC_SRCS+= \
	absvsi2.c \
	absvti2.c \
	addvsi3.c \
	addvti3.c \
	ashlti3.c \
	ashrti3.c \
	clzti2.c \
	cmpti2.c \
	ctzti2.c \
	divti3.c \
	ffsti2.c \
	fixsfdi.c \
	fixdfdi.c \
	fixunsdfdi.c \
	fixunsdfsi.c \
	fixunssfdi.c \
	fixunssfsi.c \
	fixunsxfdi.c \
	fixunsxfsi.c \
	fixxfdi.c \
	floatdidf.c \
	floatdisf.c \
	floatdixf.c \
	floatundidf.c \
	floatundisf.c \
	floatundixf.c \
	int_util.c \
	lshrti3.c \
	modti3.c \
	muldc3.c \
	mulosi4.c \
	muloti4.c \
	multi3.c \
	mulvsi3.c \
	mulvti3.c \
	negdf2.c \
	negsf2.c \
	negti2.c \
	negvsi2.c \
	negvti2.c \
	paritysi2.c \
	parityti2.c \
	popcountsi2.c \
	popcountti2.c \
	powidf2.c \
	powisf2.c \
	powitf2.c \
	powixf2.c \
	subvsi3.c \
	subvti3.c \
	ucmpti2.c \
	udivmodti4.c \
	udivti3.c \
	umodti3.c

.if ${MACHINE_ARCH} != "aarch64"
GENERIC_SRCS+= \
	fixunsdfti.c \
	fixunssfti.c \
	fixunsxfti.c \
	fixxfti.c \
	floattidf.c \
	floattisf.c \
	floattixf.c \
	floatuntidf.c \
	floatuntisf.c \
	floatuntixf.c
.endif

# These have h/w instructions which are always used.
.if ${LIBC_MACHINE_ARCH} != "alpha" && ${LIBC_MACHINE_CPU} != "aarch64" \
    && ${LIBC_MACHINE_CPU} != "powerpc" && ${LIBC_MACHINE_CPU} != "or1k"
GENERIC_SRCS+= \
	clzsi2.c
.endif

# These have h/w instructions which are always used.
.if ${LIBC_MACHINE_ARCH} != "alpha" && ${LIBC_MACHINE_ARCH} != "vax" \
    && ${LIBC_MACHINE_CPU} != "aarch64" && ${LIBC_MACHINE_CPU} != "powerpc" \
    && ${LIBC_MACHINE_CPU} != "or1k"
GENERIC_SRCS+= \
	ctzsi2.c
.endif

# These have h/w instructions which are always used.
.if ${LIBC_MACHINE_ARCH} != "alpha" && ${LIBC_MACHINE_CPU} != "powerpc" \
    && ${LIBC_MACHINE_CPU} != "aarch64" && ${LIBC_MACHINE_ARCH} != "vax"
GENERIC_SRCS+= \
	divmodsi4.c \
	divsi3.c \
	modsi3.c \
	udivmodsi4.c \
	umodsi3.c

. if ${LIBC_MACHINE_CPU} != "sh3"
# On sh3 __udivsi3 is gcc "millicode" with special calling convention
# (less registers clobbered than usual).  Each DSO that needs it gets
# its own hidden copy from libgcc.a.
GENERIC_SRCS+= \
	udivsi3.c
. endif
.endif


GENERIC_SRCS+= \
	absvdi2.c \
	addvdi3.c \
	mulodi4.c \
	mulvdi3.c \
	negvdi2.c \
	paritydi2.c \
	popcountdi2.c \
	subvdi3.c

# These have h/w instructions which are always used.
.if ${LIBC_MACHINE_ARCH} != "alpha" && ${LIBC_MACHINE_CPU} != "powerpc64" \
    && ${LIBC_MACHINE_ARCH} != "aarch64" && ${LIBC_MACHINE_CPU} != "or1k"
GENERIC_SRCS+= \
	clzdi2.c \
	ctzdi2.c \
	ffsdi2.c
.endif

# Don't need these on 64-bit machines.
.if empty(LIBC_MACHINE_ARCH:M*64*) && ${LIBC_MACHINE_ARCH} != "alpha"
GENERIC_SRCS+= \
	cmpdi2.c \
	ashldi3.c \
	ashrdi3.c \
	divdi3.c \
	divmoddi4.c \
	lshrdi3.c \
	moddi3.c \
	muldi3.c \
	negdi2.c \
	ucmpdi2.c \
	udivdi3.c \
	udivmoddi4.c \
	umoddi3.c
.endif

GENERIC_SRCS+= \
	GCDAProfiling.c \
	InstrProfiling.c \
	InstrProfilingBuffer.c \
	InstrProfilingFile.c \
	InstrProfilingPlatformOther.c

.if ${LIBC_MACHINE_ARCH} == "powerpc" || ${LIBC_MACHINE_ARCH} == "powerpc64"
GENERIC_SRCS+= \
	fixtfdi.c \
	fixunstfdi.c \
	floatditf.c \
	floatunditf.c \
	gcc_qadd.c \
	gcc_qdiv.c \
	gcc_qmul.c \
	gcc_qsub.c
.endif

.if ${LIBC_MACHINE_CPU} == "aarch64"
GENERIC_SRCS+= \
	clear_cache.c
.endif

.if ${LIBC_MACHINE_CPU} == "arm"
.if !empty(LIBC_MACHINE_ARCH:Mearm*)
GENERIC_SRCS+= \
	aeabi_idivmod.S \
	aeabi_ldivmod.S \
	aeabi_uidivmod.S \
	aeabi_uldivmod.S
.endif
GENERIC_SRCS+= \
	clear_cache.c
# Not yet, overlaps with softfloat
#	aeabi_dcmp.S \
#	aeabi_fcmp.S
# Not yet, requires ARMv6
#GENERIC_SRCS+= \
#	bswapdi2.S \
#	bswapsi2.S
.endif

.for src in ${GENERIC_SRCS}
.  if exists(${COMPILER_RT_CPU_DIR}/${src:R}.S) || \
      exists(${COMPILER_RT_ARCH_DIR}/${src:R}.S)
SRCS+=	${src:R}.S
.  else
SRCS+=	${src}
.    if ${src:E} != "cc"
COPTS.${src}+=	-Wno-missing-prototypes \
		-Wno-old-style-definition \
		-Wno-strict-prototypes \
		-Wno-uninitialized \
		-Wno-cast-qual
.    endif
.  endif
.endfor
