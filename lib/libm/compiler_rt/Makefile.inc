# $NetBSD: Makefile.inc,v 1.1 2013/10/15 23:59:36 joerg Exp $

COMPILER_RT_SRCDIR=	${NETBSDSRCDIR}/sys/external/bsd/compiler_rt/dist

.if ${MACHINE_ARCH} == "powerpc"
COMPILER_RT_CPU_DIR=	${COMPILER_RT_SRCDIR}/lib/ppc
COMPILER_RT_ARCH_DIR=	${COMPILER_RT_SRCDIR}/lib/ppc
.else
COMPILER_RT_CPU_DIR=	${COMPILER_RT_SRCDIR}/lib/${MACHINE_CPU}
COMPILER_RT_ARCH_DIR=	${COMPILER_RT_SRCDIR}/lib/${MACHINE_ARCH}
.endif

.if !defined(__MINIX)
.PATH: ${COMPILER_RT_CPU_DIR}
.PATH: ${COMPILER_RT_ARCH_DIR}
.endif # !defined(__MINIX)
.PATH: ${COMPILER_RT_SRCDIR}/lib

# Needs scaling support
GENERIC_SRCS+= \
	mulxc3.c \
	mulsc3.c \
	divxc3.c \
	divdc3.c \
	divsc3.c

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
