# Makefile for the mmc driver.
PROG=	mmc
SRCS=	mmcblk.c mmchost_mmchs.c mmchost_dummy.c mmclog.h sdhcreg.h sdmmcreg.h

DPADD+=	${LIBBLOCKDRIVER} ${LIBSYS}
LDADD+=	-lblockdriver -lsys
CLEANFILES+=.depend mmcblk.d

#
# This is a system driver.
CPPFLAGS+= -D_SYSTEM=1

MAN=

BINDIR?= /usr/sbin

.include <minix.service.mk>
