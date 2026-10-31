#	$NetBSD: Makefile,v 1.3 2017/01/24 18:04:03 christos Exp $

BOOTMODEL=	minix
IMAGE=		ramdisk-${BOOTMODEL}.fs
IMAGESIZE=	1504k
#MAKEDEVTARGETS=	std md0 fd0 fd1 wd0 wd1 wd2 wd3 cd0 cd1 ipty ttyv0
#MAKEDEVTARGETS=	ramdisk
MAKEFS_FLAGS+=	-f 10
#USE_SYSINST=	en

.include "${.CURDIR}/../common/Makefile.ramdisk"
