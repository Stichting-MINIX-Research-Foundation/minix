# $NetBSD: Makefile,v 1.2 2011/02/20 05:42:34 joerg Exp $

.include <bsd.own.mk>

FILESDIR=/usr/share/examples/libsaslc/mech
FILESOWN=root
.if !defined(__MINIX)
FILESGRP=wheel
.else
FILESGRP=operator
.endif # !defined(__MINIX)
FILESMODE=444

FILES=
FILES+= ANONYMOUS.conf
FILES+= CRAM-MD5.conf
FILES+= DIGEST-MD5.conf
FILES+= EXTERNAL.conf
FILES+= GSSAPI.conf
FILES+= LOGIN.conf
FILES+= PLAIN.conf

.include <bsd.prog.mk>
