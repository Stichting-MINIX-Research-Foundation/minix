# Makefile for memory driver (MEMORY)
.include <bsd.own.mk>

USE_BITCODE:=no

PROG=	memory
SRCS=	memory.c imgrd.mfs
OBJS=	${SRCS:N*.h:R:S/$/.o/g}
MKBUILDEXT2RD?=	no

RAMDISK_PATH= ${NETBSDSRCDIR}/minix/drivers/storage/ramdisk
DPADD+=	${LIBBLOCKDRIVER} ${LIBCHARDRIVER}
LDADD+=	-lblockdriver -lchardriver

CPPFLAGS.memory.c+=	-I${NETBSDSRCDIR}/minix

imgrd.d: touch-genfiles
touch-genfiles:
	[ -e ../ramdisk/image ] || touch -t 197001020000.00 ../ramdisk/image


.SUFFIXES:      .mfs .c .o

.mfs.o:
	${_MKTARGET_CREATE}
	${OBJCOPY} -Ibinary -B${MACHINE_CPU} -O${MACHINE_GNU_PLATFORM} $< $@

CLEANFILES+=	../ramdisk/image
# BJG - don't invoke parallel Makes
#../ramdisk/image: .PHONY
#	${MAKE} -C ${RAMDISK_PATH} image

CLEANFILES+=	imgrd.mfs
imgrd.mfs: ../ramdisk/image
	${HOST_LN} -fs ../ramdisk/image $@

.include <minix.service.mk>
