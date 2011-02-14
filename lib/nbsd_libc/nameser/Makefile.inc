#	$NetBSD: Makefile.inc,v 1.1 2004/05/20 20:02:26 christos Exp $

# net sources
.PATH: ${.CURDIR}/nameser

SRCS+=	ns_name.c ns_netint.c ns_parse.c ns_print.c \
	ns_samedomain.c ns_ttl.c
