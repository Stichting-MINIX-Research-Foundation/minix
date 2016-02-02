#	$NetBSD: Makefile,v 1.10 2015/01/23 12:34:09 hkenken Exp $

# Makefile for evbarm tags file and boot blocks

# Find where ARM source files are for inclusion in tags
.include <../arm/Makefile.inc>

TEVBARM=	${SYSDIR}/arch/evbarm/tags
SEVBARM=	${SYSDIR}/arch/evbarm/adi_brh/*.[ch]
SEVBARM+=	${SYSDIR}/arch/evbarm/armadillo/*.[ch]
SEVBARM+=	${SYSDIR}/arch/evbarm/beagle/*.[ch]
SEVBARM+=	${SYSDIR}/arch/evbarm/cp3100/*.[ch]
SEVBARM+=	${SYSDIR}/arch/evbarm/dev/*.[ch]
SEVBARM+=	${SYSDIR}/arch/evbarm/devkit8000/*.[ch]
SEVBARM+=	${SYSDIR}/arch/evbarm/evbarm/*.[ch]
SEVBARM+=	${SYSDIR}/arch/evbarm/g42xxeb/*.[ch]
SEVBARM+=	${SYSDIR}/arch/evbarm/gemini/*.[ch]
SEVBARM+=	${SYSDIR}/arch/evbarm/gumstix/*.[ch]
SEVBARM+=	${SYSDIR}/arch/evbarm/hdl_g/*.[ch]
SEVBARM+=	${SYSDIR}/arch/evbarm/ifpga/*.[ch]
SEVBARM+=	${SYSDIR}/arch/evbarm/imx31/*.[ch]
SEVBARM+=	${SYSDIR}/arch/evbarm/include/*.h
SEVBARM+=	${SYSDIR}/arch/evbarm/integrator/*.[ch]
SEVBARM+=	${SYSDIR}/arch/evbarm/iq31244/*.[ch]
SEVBARM+=	${SYSDIR}/arch/evbarm/iq80310/*.[ch]
SEVBARM+=	${SYSDIR}/arch/evbarm/iq80321/*.[ch]
SEVBARM+=	${SYSDIR}/arch/evbarm/ixdp425/*.[ch]
SEVBARM+=	${SYSDIR}/arch/evbarm/ixm1200/*.[ch]
SEVBARM+=	${SYSDIR}/arch/evbarm/lubbock/*.[ch]
SEVBARM+=	${SYSDIR}/arch/evbarm/marvell/*.[ch]
SEVBARM+=	${SYSDIR}/arch/evbarm/mini2440/*.[ch]
SEVBARM+=	${SYSDIR}/arch/evbarm/mpcsa/*.[ch]
SEVBARM+=	${SYSDIR}/arch/evbarm/netwalker/*.[ch]
SEVBARM+=	${SYSDIR}/arch/evbarm/npwr_fc/*.[ch]
SEVBARM+=	${SYSDIR}/arch/evbarm/nslu2/*.[ch]
SEVBARM+=	${SYSDIR}/arch/evbarm/rpi/*.[ch]
SEVBARM+=	${SYSDIR}/arch/evbarm/osk5912/*.[ch]
SEVBARM+=	${SYSDIR}/arch/evbarm/smdk2xx0/*.[ch]
SEVBARM+=	${SYSDIR}/arch/evbarm/tisdp24xx/*.[ch]
SEVBARM+=	${SYSDIR}/arch/evbarm/tsarm/*.[ch]
SEVBARM+=	${SYSDIR}/arch/evbarm/viper/*.[ch]
SEVBARM+=	${SYSDIR}/arch/evbarm/zynq/*.[ch]
SEVBARM+=	${SYSDIR}/arch/arm/xscale/*.[ch]

AEVBARM=	${SYSDIR}/arch/evbarm/adi_brh/*.S
AEVBARM+=	${SYSDIR}/arch/evbarm/armadillo/*.S
AEVBARM+=	${SYSDIR}/arch/evbarm/beagle/*.S
AEVBARM+=	${SYSDIR}/arch/evbarm/g42xxeb/*.S
AEVBARM+=	${SYSDIR}/arch/evbarm/gemini/*.S
AEVBARM+=	${SYSDIR}/arch/evbarm/gumstix/*.S
AEVBARM+=	${SYSDIR}/arch/evbarm/hdl_g/*.S
AEVBARM+=	${SYSDIR}/arch/evbarm/imx31/*.S
AEVBARM+=	${SYSDIR}/arch/evbarm/ixdp425/*.S
AEVBARM+=	${SYSDIR}/arch/evbarm/ixm1200/*.S
AEVBARM+=	${SYSDIR}/arch/evbarm/lubbock/*.S
AEVBARM+=	${SYSDIR}/arch/evbarm/marvell/*.S
AEVBARM+=	${SYSDIR}/arch/evbarm/mini2440/*.S
AEVBARM+=	${SYSDIR}/arch/evbarm/mpcsa/*.S
AEVBARM+=	${SYSDIR}/arch/evbarm/netwalker/*.S
AEVBARM+=	${SYSDIR}/arch/evbarm/nslu2/*.S
AEVBARM+=	${SYSDIR}/arch/evbarm/rpi/*.S
AEVBARM+=	${SYSDIR}/arch/evbarm/smdk2xx0/*.S
AEVBARM+=	${SYSDIR}/arch/evbarm/tisdp24xx/*.S
AEVBARM+=	${SYSDIR}/arch/evbarm/tsarm/*.S
AEVBARM+=	${SYSDIR}/arch/evbarm/viper/*.S
AEVBARM+=	${SYSDIR}/arch/arm/xscale/*.S
AEVBARM+=	${SYSDIR}/arch/evbarm/integrator/*.S
AEVBARM+=	${SYSDIR}/arch/evbarm/iq80310/*.S
AEVBARM+=	${SYSDIR}/arch/evbarm/iq80321/*.S
AEVBARM+=	${SYSDIR}/arch/evbarm/zynq/*.S

# Directories in which to place tags links
DEVBARM=	include

.if !defined(__MINIX)
.include "../../kern/Make.tags.inc"

tags:
	-rm -f ${TEVBARM}
	-echo ${SEVBARM} ${SARM} | xargs ctags -wadtf ${TEVBARM}
	-${FINDCOMM} | xargs ctags -wadtf ${TEVBARM}
	egrep "^ENTRY(.*)|^ALTENTRY(.*)" ${AEVBARM} ${AARM} | \
	    ${TOOL_SED} -e \
		"s;\([^:]*\):\([^(]*\)(\([^, )]*\)\(.*\);\3	\1	/^\2(\3\4$$/;" \
	    >> ${TEVBARM}
	sort -o ${TEVBARM} ${TEVBARM}

links:
	-for i in ${DEVBARM}; do \
	    cd $$i && rm -f tags; ln -s ../tags tags; done
.endif # !defined(__MINIX)


SUBDIR=	include
.include <bsd.subdir.mk>
