# $NetBSD: Makefile,v 1.3 2021/10/06 11:26:21 jmcneill Exp $

PROG=		bootriscv64.efi
OBJFMT=		binary
NEWVERSWHAT=	"efiboot (riscv64)"
GNUEFIARCH=	riscv64
LIBGNUEFI_ARCH=	riscv64
OBJCOPY_FORMAT=	-O binary

EFIBOOT_FDT=	yes

EXTRA_SOURCES=	efibootriscv64.c

COPTS+=		-mcmodel=medany
AFLAGS+=	-x assembler-with-cpp
CFLAGS+=	-DEFIBOOT_ALIGN=0x200000
CFLAGS+=	-DEFIBOOT_MODULE_MACHINE=\"riscv\"

.include "${.CURDIR}/../Makefile.efiboot"

release: check_RELEASEDIR
	${HOST_INSTALL_FILE} -m ${BINMODE} ${PROG} \
		${RELEASEDIR}/${RELEASEMACHINEDIR}/installation/misc
