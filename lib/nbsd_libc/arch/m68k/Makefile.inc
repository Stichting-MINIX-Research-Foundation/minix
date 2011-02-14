#	$NetBSD: Makefile.inc,v 1.13 2008/02/09 02:41:06 mrg Exp $

SRCS+=	__sigaction14_sigtramp.c __sigtramp2.S __mmap.S

ASM+=	_lwp_getprivate.S mremap.S

.if ${MKSOFTFLOAT} != "no"
CPPFLAGS+= -DSOFTLOFLOAT_NEED_FIXUNS -DSOFTFLOAT -DSOFTFLOATM68K_FOR_GCC
.  include <softfloat/Makefile.inc>
.endif
