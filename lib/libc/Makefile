# Makefile for libc

.include "Makefile.inc"

LIB=		c

.include "${.CURDIR}/ansi/Makefile.inc"
.include "${.CURDIR}/asyn/Makefile.inc"
.include "${.CURDIR}/db/Makefile.inc"
.include "${.CURDIR}/ip/Makefile.inc"
.include "${.CURDIR}/math/Makefile.inc"
.include "${.CURDIR}/other/Makefile.inc"
.include "${.CURDIR}/posix/Makefile.inc"
.include "${.CURDIR}/regex/Makefile.inc"
.include "${.CURDIR}/stdio/Makefile.inc"
.include "${.CURDIR}/stdtime/Makefile.inc"
.include "${.CURDIR}/syscall/Makefile.inc"
.include "${.CURDIR}/sysvipc/Makefile.inc"
.include "${.CURDIR}/wchar/Makefile.inc"

.include "${.CURDIR}/arch/${ARCH}/int64/Makefile.inc"
.include "${.CURDIR}/arch/${ARCH}/misc/Makefile.inc"
.include "${.CURDIR}/arch/${ARCH}/math/Makefile.inc"
.include "${.CURDIR}/arch/${ARCH}/rts/Makefile.inc"
.include "${.CURDIR}/arch/${ARCH}/string/Makefile.inc"

.if ${COMPILER_TYPE} == "ack"
.include "${.CURDIR}/ack/fphook/Makefile.inc"
.include "${.CURDIR}/ack/math/Makefile.inc"
.include "${.CURDIR}/ack/rts/Makefile.inc"
.elif ${COMPILER_TYPE} == "gnu"
.include "${.CURDIR}/gnu/ieee_float/Makefile.inc"
.include "${.CURDIR}/gnu/arch/${ARCH}/rts/Makefile.inc"
.endif

.include <bsd.lib.mk>
