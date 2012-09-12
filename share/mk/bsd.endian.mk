#	$NetBSD: bsd.endian.mk,v 1.16 2012/08/05 04:11:35 matt Exp $

.if !defined(_BSD_ENDIAN_MK_)
_BSD_ENDIAN_MK_=1

.include <bsd.init.mk>

.if ${MACHINE_ARCH} == "alpha" || \
    ${MACHINE_ARCH} == "arm" || \
    ${MACHINE_ARCH} == "earm" || \
    ${MACHINE_ARCH} == "i386" || \
    ${MACHINE_ARCH} == "ia64" || \
    ${MACHINE_ARCH} == "vax" || \
    ${MACHINE_ARCH} == "x86_64" || \
    ${MACHINE_ARCH:C/^.*el$/el/} == "el"
TARGET_ENDIANNESS=	1234
.elif ${MACHINE_ARCH} == "hppa" || \
      ${MACHINE_ARCH} == "m68000" || \
      ${MACHINE_ARCH} == "m68k" || \
      ${MACHINE_ARCH} == "powerpc" || \
      ${MACHINE_ARCH} == "powerpc64" || \
      ${MACHINE_ARCH} == "sparc" || \
      ${MACHINE_ARCH} == "sparc64" || \
      ${MACHINE_ARCH:C/^.*eb$/eb/} == "eb"
TARGET_ENDIANNESS=	4321
.endif

.endif  # !defined(_BSD_ENDIAN_MK_)
