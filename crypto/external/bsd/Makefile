#	$NetBSD: Makefile,v 1.12 2013/02/12 20:31:14 christos Exp $

.include <bsd.own.mk>

.if defined(__MINIX)
SUBDIR+= openssl .WAIT heimdal netpgp .WAIT libsaslc
.else
SUBDIR+= openssl .WAIT heimdal netpgp .WAIT openssh libsaslc
.endif # defined(__MINIX)

.include <bsd.subdir.mk>
