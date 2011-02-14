#	$NetBSD: libcincludes.mk,v 1.1 2008/10/26 07:28:06 mrg Exp $

# Makefile fragment shared across several parts that want to look
# inside libc's include tree.

.if defined(LIBC_MACHINE_ARCH) && \
    exists(${NETBSDSRCDIR}/lib/libc/arch/${LIBC_MACHINE_ARCH}/SYS.h)
ARCHSUBDIR=	${LIBC_MACHINE_ARCH}
.elif exists(${NETBSDSRCDIR}/lib/libc/arch/${MACHINE_ARCH}/SYS.h)
ARCHSUBDIR=	${MACHINE_ARCH}
.elif exists(${NETBSDSRCDIR}/lib/libc/arch/${MACHINE_CPU}/SYS.h)
ARCHSUBDIR=	${MACHINE_CPU}
.else
.BEGIN:
	@echo no ARCHDIR for ${MACHINE_ARCH} nor ${MACHINE_CPU}
	@false
.endif

ARCHDIR=	${NETBSDSRCDIR}/lib/libc/arch/${ARCHSUBDIR}
