#	$NetBSD: Makefile,v 1.10 2007/05/28 12:06:22 tls Exp $

USE_FORT?= yes	# network server

LIB=	wrap

SRCS=	hosts_access.c options.c shell_cmd.c rfc931.c eval.c hosts_ctl.c \
	refuse.c percent_x.c clean_exit.c fix_options.c socket.c \
	update.c misc.c diag.c
MAN=	hosts_access.3 hosts_access.5 hosts_options.5
MLINKS+=hosts_access.5 hosts.allow.5
MLINKS+=hosts_access.5 hosts.deny.5
MLINKS+=hosts_access.3 hosts_ctl.3
MLINKS+=hosts_access.3 request_init.3
MLINKS+=hosts_access.3 request_set.3

INCS= tcpd.h
INCSDIR=/usr/include

.include "Makefile.cflags"

.include <bsd.lib.mk>
