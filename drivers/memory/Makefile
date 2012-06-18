# Makefile for memory driver (MEMORY)

PROG=	memory
SRCS=	memory.c imgrd.mfs
OBJS=	${SRCS:N*.h:R:S/$/.o/g}
MKBUILDEXT2RD?=	no

RAMDISK_PATH= ${NETBSDSRCDIR}/drivers/ramdisk
DPADD+=	${LIBBLOCKDRIVER} ${LIBCHARDRIVER} ${LIBSYS}
LDADD+=	-lblockdriver -lchardriver -lsys

MAN=

BINDIR?= /usr/sbin

CPPFLAGS.memory.c+=	-I${NETBSDSRCDIR}

imgrd.d: touch-genfiles
touch-genfiles:
	[ -e ../ramdisk/image ] || touch -t 197001020000.00 ../ramdisk/image


.SUFFIXES:      .mfs .c .o

# 'elf32-${ARCH}-minix' below should really be ${MACHINE_GNU_PLATFORM}
# but bsd.own.mk has to be upgraded for that.
.mfs.o:
	${_MKTARGET_CREATE}
	${OBJCOPY} -Ibinary -B${MACHINE_ARCH} -Oelf32-${MACHINE_ARCH}-minix $< $@

${RAMDISK_PATH}/image: .PHONY
	${MAKE} -C ${RAMDISK_PATH} image

imgrd.mfs: ${RAMDISK_PATH}/image
	ln -fs ${RAMDISK_PATH}/image $@
CLEANFILES+=	imgrd.mfs

.include <minix.bootprog.mk>
