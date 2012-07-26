# $NetBSD: Makefile,v 1.15 2002/08/27 14:46:20 lukem Exp $

.include <bsd.own.mk>		# for MKDYNAMICROOT definition

SRCS   = dhclient.c clparse.c dhc6.c
PROG   = dhclient
MAN    = dhclient.8 dhclient.conf.5 dhclient.leases.5 dhclient-script.8
CPPFLAGS += -DCLIENT_PATH='"PATH=/usr/bin:/usr/sbin:/bin:/sbin"'

DHCPSRCDIR=	client

# XXX
BINDIR?=	/sbin
.if (${MKDYNAMICROOT} == "no")
LDSTATIC?=	-static
.endif

.include <bsd.prog.mk>
