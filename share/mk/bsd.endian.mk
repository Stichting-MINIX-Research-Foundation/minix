#	$NetBSD: bsd.endian.mk,v 1.22 2014/09/19 17:45:42 matt Exp $

.if !defined(_BSD_ENDIAN_MK_)
_BSD_ENDIAN_MK_=1

.include <bsd.init.mk>

.if ${MACHINE_ARCH} == "aarch64" || \
    ${MACHINE_ARCH} == "alpha" || \
    ${MACHINE_ARCH} == "arm" || \
    (!empty(MACHINE_ARCH:Mearm*) && empty(MACHINE_ARCH:Mearm*eb)) || \
    ${MACHINE_ARCH} == "i386" || \
    ${MACHINE_ARCH} == "ia64" || \
    ${MACHINE_ARCH} == "vax" || \
    ${MACHINE_ARCH} == "riscv32" || \
    ${MACHINE_ARCH} == "riscv64" || \
    ${MACHINE_ARCH} == "x86_64" || \
    ${MACHINE_ARCH:C/^.*el$/el/} == "el"
TARGET_ENDIANNESS=	1234
.elif ${MACHINE_ARCH} == "coldfire" || \
      ${MACHINE_ARCH} == "hppa" || \
      ${MACHINE_ARCH} == "m68000" || \
      ${MACHINE_ARCH} == "m68k" || \
      ${MACHINE_ARCH} == "or1k" || \
      ${MACHINE_ARCH} == "powerpc" || \
      ${MACHINE_ARCH} == "powerpc64" || \
      ${MACHINE_ARCH} == "sparc" || \
      ${MACHINE_ARCH} == "sparc64" || \
      ${MACHINE_ARCH:C/^.*eb$/eb/} == "eb"
TARGET_ENDIANNESS=	4321
.endif

.endif  # !defined(_BSD_ENDIAN_MK_)
