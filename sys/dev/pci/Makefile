#	$NetBSD: Makefile,v 1.14 2015/03/28 14:41:28 msaitoh Exp $

# use 'make -f Makefile.pcidevs' to make pcidevs.h and pcidevs_data.h

#LSC: MINIX Not Yet imported
#SUBDIR=       hdaudio

INCSDIR= /usr/include/dev/pci

# Only install includes which are used by userland
INCS=	amrreg.h amrio.h if_lmc.h mlyio.h mlyreg.h \
	pcidevs.h pcidevs_data.h pciio.h pcireg.h \
	tgareg.h twereg.h tweio.h

.include <bsd.kinc.mk>
