#	$NetBSD: ui.inc,v 1.1 2009/07/19 23:30:45 christos Exp $
#
#	@(#) Copyright (c) 1995 Simon J. Gerraty
#
#	SRCS extracted from src/crypto/dist/openssl/crypto/ui/Makefile
#

.PATH:	${OPENSSLSRC}/crypto/ui


UI_SRCS = ui_err.c ui_lib.c ui_openssl.c ui_util.c $(COMPATSRC)
SRCS += ${UI_SRCS}

.for cryptosrc in ${UI_SRCS}
CPPFLAGS.${cryptosrc} = -I${OPENSSLSRC}/crypto/ui
.endfor
