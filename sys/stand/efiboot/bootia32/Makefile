# $NetBSD: Makefile,v 1.12 2022/04/02 11:16:07 skrll Exp $

PROG=		bootia32.efi
NEWVERSWHAT=	"efiboot (ia32)"
CPUFLAGS=	-march=i686 -mtune=i686
GNUEFIARCH=	ia32
LIBKERN_ARCH=	i386
LIBGNUEFI_ARCH=	i386
KLINK_MACHINE=	i386
OBJCOPY_FORMAT=	--subsystem efi-app	--target=pei-i386

EFIBOOT_ACPI=	yes
EFIBOOT_MULTIBOOT2=	yes

.PATH:		${.CURDIR}/../x86
EXTRA_SOURCES=	efibootia32.c
EXTRA_SOURCES+=	efibootx86.c bootinfo.c efimemory.c
EXTRA_SOURCES+=	startprog32.S
EXTRA_SOURCES+=	multiboot32.S

# MINIX's clang is already i586-only and warns that -m32 is unused.
M32FLAG=	${${ACTIVE_CC} == "clang":?:-m32}
CFLAGS+=	${M32FLAG} -DACPI_32BIT_PHYSICAL_ADDRESS
AFLAGS+=	${M32FLAG}
CPPFLAGS+=	-DEFIBOOT_ALLOCATE_MAX_ADDRESS=0x100000000ULL
CPPFLAGS+=	-DEFIBOOT_X86
CFLAGS+=	-DEFIBOOT_ALIGN=0x200000
CFLAGS+=	-DEFIBOOT_MODULE_MACHINE=\"i386\"

.include "${.CURDIR}/../Makefile.efiboot"

release: check_RELEASEDIR
	${HOST_INSTALL_FILE} -m ${BINMODE} ${PROG} \
		${RELEASEDIR}/${RELEASEMACHINEDIR}/installation/misc
