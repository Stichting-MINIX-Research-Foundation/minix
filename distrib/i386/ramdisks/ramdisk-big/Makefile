#	$NetBSD: Makefile,v 1.4 2009/09/20 22:50:50 abs Exp $

BOOTMODEL=	big
IMAGE=		ramdisk-${BOOTMODEL}.fs
IMAGESIZE=	5000k
IMAGEDEPENDS= 	
USE_SYSINST=	yes
SMALLPROG_INET6=1

.include "${.CURDIR}/../common/Makefile.ramdisk"
.include "${DISTRIBDIR}/common/Makefile.dhcpcd"

.if ${USE_INET6} != "no"
LISTS+=		${DISTRIBDIR}/common/list.inet6
.endif

MAKEFS_FLAGS+=	-f 20
