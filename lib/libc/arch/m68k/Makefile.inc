#	$NetBSD: Makefile.inc,v 1.14 2011/11/22 15:25:28 joerg Exp $

SRCS+=	__sigaction14_sigtramp.c __sigtramp2.S __m68k_read_tp.S __mmap.S

ASM+=	_lwp_getprivate.S mremap.S

.if ${MKSOFTFLOAT} != "no"
CPPFLAGS+= -DSOFTLOFLOAT_NEED_FIXUNS -DSOFTFLOAT -DSOFTFLOATM68K_FOR_GCC
.  include <softfloat/Makefile.inc>
.endif
