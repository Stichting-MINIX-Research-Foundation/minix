#	$NetBSD: Makefile,v 1.6 2017/02/03 17:28:58 roy Exp $

LIB=	gnuefi
LIBISPRIVATE?= yes

GNUEFI_USE_CREAD?= no		# Read compressed kernels
GNUEFI_INCLUDE_NET?= yes		# Netboot via TFTP, NFS
GNUEFI_USE_LOADFILE?= no		# Generic executable loading support
GNUEFI_ENABLE_LS_OP?= no		# Filesystems ls operation

GNUEFIDIST=	${GNUEFIDIR}/../../external/bsd/gnu-efi/dist

CPPFLAGS=	-I${GNUEFIDIR} ${GNUEFICPPFLAGS} ${GNUEFIMISCCPPFLAGS}

.if defined(GNUEFI_EXTRADIR)
.-include "${GNUEFI_EXTRADIR}/Makefile.inc"
.endif

.include <bsd.own.mk>

LIBGNUEFI_ARCH?=	${MACHINE_ARCH}
LIBGNUEFI_CPU?=		${MACHINE_CPU}

.if ${LIBGNUEFI_ARCH} == "i386"
GNUEFIARCH=ia32
.else
GNUEFIARCH=${LIBGNUEFI_ARCH}
.endif

.if defined(LIBGNUEFI_ARCH) && !empty(LIBGNUEFI_ARCH) && \
	    exists(${GNUEFIDIR}/arch/${LIBGNUEFI_ARCH})
ARCHSUBDIR=	${LIBGNUEFI_ARCH}
.elif defined(MACHINE_ARCH) && !empty(MACHINE_ARCH) && \
	    exists(${GNUEFIDIR}/arch/${MACHINE_ARCH})
ARCHSUBDIR=	${MACHINE_ARCH}
.elif defined(MACHINE_CPU) && !empty(MACHINE_CPU) && \
	    exists(${GNUEFIDIR}/arch/${MACHINE_CPU})
ARCHSUBDIR=	${MACHINE_CPU}
.endif

M= ${GNUEFIDIR}/arch/${ARCHSUBDIR}

.if exists($M/Makefile.inc)
CPPFLAGS+= -I$M
.PATH.c: $M
.PATH.S: $M
.include "$M/Makefile.inc"
.endif

.PATH.c: ${GNUEFIDIR} ${GNUEFIDIST}/lib/${GNUEFIARCH} ${GNUEFIDIST}/lib ${GNUEFIDIST}/lib/runtime
.PATH.S: ${GNUEFIDIR} ${GNUEFIDIST}/lib/${GNUEFIARCH} ${GNUEFIDIST}/lib ${GNUEFIDIST}/lib/runtime

SRCS+=	boxdraw.c smbios.c console.c crc.c data.c debug.c dpath.c \
	error.c event.c guid.c hand.c hw.c init.c lock.c \
	misc.c print.c sread.c str.c \
	rtlock.c efirtlib.c rtstr.c vm.c rtdata.c \
	initplat.c math.c

CWARNFLAGS.gcc+= -Wno-unused-but-set-variable
COPTS.boxdraw.c= -Wno-missing-field-initializers
COPTS.guid.c= -Wno-missing-field-initializers
COPTS.smbios.c= -Wno-int-to-pointer-cast
COPTS.dpath.c= -Wno-missing-prototypes
COPTS.print.c= -Wno-missing-prototypes
COPTS.hw.c= -Wno-shadow

.include <bsd.lib.mk>

lib${LIB}.o:: ${OBJS:O}
	@echo building standard ${LIB} library
	@rm -f lib${LIB}.o
	@${LD} -r -o lib${LIB}.o `lorder ${OBJS} | tsort`

CPPFLAGS+=	-Wno-pointer-sign
