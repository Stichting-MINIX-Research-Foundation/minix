# Makefile for memory driver (MEMORY)
PROG=	memory
SRCS=	memory.c imgrd.mfs
OBJS=	${SRCS:N*.h:R:S/$/.o/g}

DPADD+=	${LIBBLOCKDRIVER} ${LIBCHARDRIVER} ${LIBSYS}
LDADD+=	-lblockdriver -lchardriver -lsys

MAN=

BINDIR?= /usr/sbin

CPPFLAGS.memory.c+=	-I${MINIXSRCDIR}

imgrd.d: touch-genfiles
touch-genfiles:
	[ -e ../ramdisk/image ] || touch -t 197001020000.00 ../ramdisk/image


.SUFFIXES:      .mfs .c .o

# 'elf32-${ARCH}-minix' below should really be ${MACHINE_GNU_PLATFORM}
# but bsd.own.mk has to be upgraded for that.
.mfs.o:
	${_MKTARGET_CREATE}
	${OBJCOPY} -Ibinary -B${ARCH} -Oelf32-${ARCH}-minix $< $@

imgrd.mfs:
	ln -s ../ramdisk/image $@
CLEANFILES+=	imgrd.mfs

.include <minix.bootprog.mk>
