# Makefile for the mmc driver.
PROGS=      mmc emmc
MAN.mmc=
MAN.emmc=

SRCS.mmc=  mmcblk.c mmchost_dummy.c sdhcreg.h sdmmcreg.h
SRCS.mmc += mmchost_mmchs.c 
SRCS.emmc= emmc.c mmcblk.c

DPADD+=	${LIBBLOCKDRIVER} ${LIBSYS}
LDADD+=	-lblockdriver -lsys
CLEANFILES+=.depend mmcblk.d

#
# This is a system driver.
CPPFLAGS+= -D_SYSTEM=1

.include <minix.service.mk>
