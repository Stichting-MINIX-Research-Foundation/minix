#	$NetBSD: Makefile.inc,v 1.1 2011/04/04 19:43:34 dyoung Exp $

AARM=	${SYSDIR}/arch/arm/arm/*.S
SARM=	${SYSDIR}/arch/arm/arm/*.[ch] ${SYSDIR}/arch/arm/include/*.h
SARM+=	${SYSDIR}/arch/arm/arm32/*.[ch]
SARM+=	${SYSDIR}/arch/arm/at91/*.[ch]
SARM+=	${SYSDIR}/arch/arm/ep93xx/*.[ch]
SARM+=	${SYSDIR}/arch/arm/footbridge/*.[ch]
SARM+=	${SYSDIR}/arch/arm/fpe-arm/*.[ch]
SARM+=	${SYSDIR}/arch/arm/gemini/*.[ch]
SARM+=	${SYSDIR}/arch/arm/imx/*.[ch]
SARM+=	${SYSDIR}/arch/arm/iomd/*.[ch]
SARM+=	${SYSDIR}/arch/arm/ixp12x0/*.[ch]
SARM+=	${SYSDIR}/arch/arm/mainbus/*.[ch]
SARM+=	${SYSDIR}/arch/arm/marvell/*.[ch]
SARM+=	${SYSDIR}/arch/arm/mpcore/*.[ch]
SARM+=	${SYSDIR}/arch/arm/ofw/*.[ch]
SARM+=	${SYSDIR}/arch/arm/omap/*.[ch]
SARM+=	${SYSDIR}/arch/arm/pic/*.[ch]
SARM+=	${SYSDIR}/arch/arm/s3c2xx0/*.[ch]
SARM+=	${SYSDIR}/arch/arm/sa11x0/*.[ch]
SARM+=	${SYSDIR}/arch/arm/vfp/*.[ch]
SARM+=	${SYSDIR}/arch/arm/xscale/*.[ch]
