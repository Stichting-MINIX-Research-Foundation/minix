# Makefile for memory driver (MEMORY)

.include <bsd.own.mk>

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

.mfs.o:
	${_MKTARGET_CREATE}
	${OBJCOPY} -Ibinary -B${MACHINE_CPU} -O${MACHINE_GNU_PLATFORM} $< $@

CLEANFILES+=	../ramdisk/image
../ramdisk/image: .PHONY
	${MAKE} -C ${RAMDISK_PATH} image

CLEANFILES+=	imgrd.mfs
imgrd.mfs: ../ramdisk/image
	${HOST_LN} -fs ../ramdisk/image $@

.include <minix.bootprog.mk>
