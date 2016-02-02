#	$NetBSD: x509v3.inc,v 1.1 2009/07/19 23:30:45 christos Exp $
#
#	@(#) Copyright (c) 1995 Simon J. Gerraty
#
#	SRCS extracted from src/crypto/dist/openssl/crypto/x509v3/Makefile
#

.PATH:	${OPENSSLSRC}/crypto/x509v3


X509V3_SRCS = pcy_cache.c pcy_data.c pcy_lib.c pcy_map.c pcy_node.c \
pcy_tree.c v3_addr.c v3_akey.c v3_akeya.c v3_alt.c v3_asid.c v3_bcons.c \
v3_bitst.c v3_conf.c v3_cpols.c v3_crld.c v3_enum.c v3_extku.c v3_genn.c \
v3_ia5.c v3_info.c v3_int.c v3_lib.c v3_ncons.c v3_ocsp.c v3_pci.c v3_pcia.c \
v3_pcons.c v3_pku.c v3_pmaps.c v3_prn.c v3_purp.c v3_skey.c v3_sxnet.c \
v3_utl.c v3err.c
SRCS += ${X509V3_SRCS}

.for cryptosrc in ${X509V3_SRCS}
CPPFLAGS.${cryptosrc} = -I${OPENSSLSRC}/crypto/x509v3
.endfor
