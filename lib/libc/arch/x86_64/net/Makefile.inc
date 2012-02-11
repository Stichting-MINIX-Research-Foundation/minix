#	$NetBSD: Makefile.inc,v 1.3 2009/12/06 07:12:18 uebayasi Exp $

# objects built from assembler sources (need lint stubs)
# hton* and nto* functions provided by ../gen/byte_swap_*.S
SRCS+=  

LSRCS.x86_64.net=Lint_htonl.c Lint_htons.c Lint_ntohl.c Lint_ntohs.c
LSRCS+=		${LSRCS.x86_64.net}
DPSRCS+=	${LSRCS.x86_64.net}
CLEANFILES+=	${LSRCS.x86_64.net}
