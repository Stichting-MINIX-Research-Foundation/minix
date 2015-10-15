# $NetBSD: Makefile.inc,v 1.2 2015/04/15 19:13:46 mrg Exp $

SRCS+=	__sigaction14_sigtramp.c __sigtramp2.S
#SRCS+= qdivrem.c

CPPFLAGS += -I.

.PATH: ${ARCHDIR}/softfloat
SRCS+=	qp.c
CPPFLAGS+=	-DSOFTFLOATAARCH64_FOR_GCC -DEXCEPTIONS_WITH_SOFTFLOAT
CPPFLAGS+=	-DSOFTFLOAT_NEED_FIXUNS
#
# Can't directly use softfloat.c due to .PATH issues with the other source
# in softfloat. Just provide a wrapper which pulls it in via a #include
SRCS.softfloat= softfloat-wrapper.c
SRCS+= ${SRCS.softfloat}
CPPFLAGS+=      -I${ARCHDIR}/softfloat -I${.CURDIR}/softfloat \
		-I${.CURDIR}/softfloat/bits64
