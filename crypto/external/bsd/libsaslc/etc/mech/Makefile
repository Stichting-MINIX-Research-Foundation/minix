# $NetBSD: Makefile,v 1.3 2014/01/02 17:57:58 tho Exp $

.include <bsd.own.mk>

.if ${MKSHARE} != "no"
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
.endif

.include <bsd.prog.mk>
