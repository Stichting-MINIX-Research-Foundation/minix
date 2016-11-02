#	$NetBSD: Makefile,v 1.32 2012/10/27 22:36:14 alnsn Exp $

INCSDIR= /usr/include/net

INCS=	bpf.h bpfdesc.h bpfjit.h dlt.h ethertypes.h if.h if_arp.h \
	if_dl.h if_ether.h \
	if_ieee1394.h if_media.h \
	if_pflog.h \
	if_types.h \
	pfil.h pfkeyv2.h pfvar.h radix.h \
	route.h 

.if !defined(__MINIX)
SUBDIR=	agr npf
.endif # !defined(__MINIX)

.include <bsd.kinc.mk>

.PATH: ${NETBSDSRCDIR}/sys/dist/pf/net
