# $NetBSD: Makefile.inc,v 1.1 2010/11/13 02:07:28 tsutsui Exp $

XORGCONF_FILES=						\
		etc.${MACHINE}/xorg.conf.ws003sh	\
		etc.${MACHINE}/xorg.conf.ws007sh	\
		etc.${MACHINE}/xorg.conf.ws011sh	\
		etc.${MACHINE}/xorg.conf.ws020sh

.include "../etc.hpc/Makefile.inc"
