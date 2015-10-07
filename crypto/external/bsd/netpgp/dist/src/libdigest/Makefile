LIB=netdigest
SRCS= tiger.c digest.c
MKMAN=no
WARNS=4
CPPFLAGS+=-I${EXTDIST}

INCS=digest.h tiger.h
INCSDIR=/usr/include/netpgp
EXTDIST=${.CURDIR}

.include <bsd.lib.mk>

.if ${HAVE_GCC} >= 45
#COPTS.isns_pdu.c+=     -fno-strict-aliasing
CPPFLAGS+=     -fno-strict-aliasing
.endif
