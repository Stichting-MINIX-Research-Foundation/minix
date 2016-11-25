#	$NetBSD: Makefile,v 1.2 2015/03/28 14:09:59 jmcneill Exp $
#

RUMPTOP=${TOPRUMP}

.include <bsd.own.mk>
.include "${NETBSDSRCDIR}/sys/modules/hdaudio/Makefile.inc"
.include "${NETBSDSRCDIR}/sys/modules/hdaudio_pci/Makefile.inc"

LIB=	rumpdev_pci_hdaudio
   
.include "${RUMPTOP}/Makefile.rump"
.include <bsd.lib.mk>
.include <bsd.klinks.mk>
