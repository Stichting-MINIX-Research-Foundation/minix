#	$NetBSD: cms.inc,v 1.4 2012/07/26 19:58:40 christos Exp $
#
#	@(#) Copyright (c) 1995 Simon J. Gcmsaty
#
#	SRCS extracted from src/crypto/dist/openssl/crypto/cms/Makefile
#

.PATH:	${OPENSSLSRC}/crypto/cms


CMS_SRCS = cms_lib.c cms_asn1.c cms_att.c cms_io.c cms_smime.c cms_err.c \
    cms_sd.c cms_dd.c cms_cd.c cms_env.c cms_enc.c cms_ess.c cms_pwri.c
SRCS += ${CMS_SRCS}

.for cryptosrc in ${CMS_SRCS}
CPPFLAGS.${cryptosrc} = -I${OPENSSLSRC}/crypto/cms
.endfor
