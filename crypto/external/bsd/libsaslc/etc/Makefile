# $NetBSD: Makefile,v 1.3 2011/02/20 05:17:47 christos Exp $

.include <bsd.own.mk>

SUBDIR=mech

FILESDIR=/usr/share/examples/libsaslc
FILESOWN=root
.if !defined(__MINIX)
FILESGRP=wheel
.else
FILESGRP=operator
.endif # !defined(__MINIX)
FILESMODE=444

FILES= saslc.conf
FILES+= README
CFG_MECH=
CFG_MECH+= ANONYMOUS.conf
CFG_MECH+= CRAM-MD5.conf
CFG_MECH+= DIGEST-MD5.conf
CFG_MECH+= EXTERNAL.conf
CFG_MECH+= GSSAPI.conf
CFG_MECH+= LOGIN.conf
CFG_MECH+= PLAIN.conf

.include <bsd.files.mk>
.include <bsd.subdir.mk>
