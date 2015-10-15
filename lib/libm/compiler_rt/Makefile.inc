# $NetBSD: Makefile.inc,v 1.9 2014/10/22 10:33:27 joerg Exp $

COMPILER_RT_SRCDIR=	${NETBSDSRCDIR}/sys/external/bsd/compiler_rt/dist

.if ${MACHINE_CPU} == "powerpc"
COMPILER_RT_CPU_DIR=	${COMPILER_RT_SRCDIR}/lib/builtins/ppc
COMPILER_RT_ARCH_DIR=	${COMPILER_RT_SRCDIR}/lib/builtins/ppc
.else
COMPILER_RT_CPU_DIR=	${COMPILER_RT_SRCDIR}/lib/builtins/${MACHINE_CPU}
COMPILER_RT_ARCH_DIR=	${COMPILER_RT_SRCDIR}/lib/builtins/${MACHINE_ARCH}
.endif

.if !defined(__MINIX)
.PATH: ${COMPILER_RT_CPU_DIR}
.PATH: ${COMPILER_RT_ARCH_DIR}
.endif # !defined(__MINIX)
.PATH: ${COMPILER_RT_SRCDIR}/lib/builtins

# Needs scaling support
GENERIC_SRCS+= \
	muldc3.c \
	mulsc3.c \
	mulxc3.c \
	divdc3.c \
	divsc3.c \
	divxc3.c

.if ${MACHINE_CPU} == "powerpc"
GENERIC_SRCS+= \
	divtc3.c \
	multc3.c
.endif

.if !defined(__MINIX)
.for src in ${GENERIC_SRCS}
.  if exists(${COMPILER_RT_CPU_DIR}/${src:R}.S) || \
      exists(${COMPILER_RT_ARCH_DIR}/${src:R}.S)
SRCS+=	${src:R}.S
.  else
SRCS+=	${src}
COPTS.${src}+=	-Wno-error=missing-prototypes
.  endif
.endfor
.else
# For MINIX: do not pull in the assembly symbols, as they are not PIC ready
.for src in ${GENERIC_SRCS}
SRCS+=	${src}
COPTS.${src}+=	-Wno-error=missing-prototypes
.endfor
.endif # !defined(__MINIX)
