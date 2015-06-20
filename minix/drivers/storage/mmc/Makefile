# Makefile for the mmc driver.
PROG=      mmc emmc
SRCS.mmc=  mmcblk.c mmchost_dummy.c sdhcreg.h sdmmcreg.h
SRCS.emmc= emmc.c mmcblk.c

.if ${MACHINE_ARCH} == "earm"
SRCS.mmc += mmchost_mmchs.c 
.endif

DPADD+=	${LIBBLOCKDRIVER} ${LIBSYS}
LDADD+=	-lblockdriver -lsys
CLEANFILES+=.depend mmcblk.d

#
# This is a system driver.
CPPFLAGS+= -D_SYSTEM=1

.include <minix.service.mk>
