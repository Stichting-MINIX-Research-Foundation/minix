#	$NetBSD: Makefile,v 1.17 2011/07/17 23:46:49 dyoung Exp $

INCSDIR= /usr/include/evbarm

INCS=	ansi.h asm.h \
	bswap.h \
	cdefs.h cpu.h \
	disklabel.h \
	elf_machdep.h endian.h endian_machdep.h \
	float.h \
	ieee.h ieeefp.h \
	int_const.h int_fmtio.h int_limits.h int_mwgwtypes.h int_types.h \
	\
	limits.h lock.h \
	math.h mcontext.h mutex.h \
	param.h profile.h \
	rwlock.h \
	setjmp.h signal.h \
	types.h \
	vmparam.h \
	wchar_limits.h

.if !defined(__MINIX)
INCS+=	joystick.h
.endif

.include <bsd.kinc.mk>
