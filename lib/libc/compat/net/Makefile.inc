#	$NetBSD: Makefile.inc,v 1.2 2012/01/20 14:08:05 joerg Exp $

.PATH: ${COMPATDIR}/net
SRCS+=	__cmsg_alignbytes.c compat_ns_addr.c compat_ns_ntoa.c
