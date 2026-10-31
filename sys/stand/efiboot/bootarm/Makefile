# $NetBSD: Makefile,v 1.9.12.1 2026/04/02 18:39:54 martin Exp $

PROG=		bootarm.efi
OBJFMT=		binary
NEWVERSWHAT=	"efiboot (arm)"
GNUEFIARCH=	arm
LIBGNUEFI_ARCH=	arm
OBJCOPY_FORMAT=	-O binary

EFIBOOT_FDT=	yes

EXTRA_SOURCES=	efibootarm.c
EXTRA_SOURCES+=	cache.S

COPTS+=		-mfloat-abi=soft -mno-unaligned-access -mlittle-endian
COPTS+=		-ffreestanding -fno-unwind-tables
AFLAGS+=	-mlittle-endian
CFLAGS+=	-DEFIBOOT_ALIGN=0x1000000
#CFLAGS+=	-DEFIBOOT_RUNTIME_ADDRESS=0x00010000U
#CFLAGS+=	-DEFIBOOT_RUNTIME_SIZE=0x7ff0000U
CFLAGS+=	-DEFIBOOT_MODULE_MACHINE=\"evbarm\"
#CPPFLAGS.efifdt.c+=   -Wconversion
LDFLAGS+=	-N -EL

.include "${.CURDIR}/../Makefile.efiboot"

release: check_RELEASEDIR
	${HOST_INSTALL_FILE} -m ${BINMODE} ${PROG} \
		${RELEASEDIR}/${RELEASEMACHINEDIR}/installation/misc
