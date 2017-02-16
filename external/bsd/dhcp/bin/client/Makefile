# $NetBSD: Makefile,v 1.1 2013/03/24 15:54:30 christos Exp $

BINDIR=	/sbin
.include <bsd.own.mk>		# for MKDYNAMICROOT definition

SRCS   = dhclient.c clparse.c dhc6.c
PROG   = dhclient
MAN    = dhclient.8 dhclient.conf.5 dhclient.leases.5 dhclient-script.8
CPPFLAGS += -DCLIENT_PATH='"PATH=/usr/bin:/usr/sbin:/bin:/sbin"'

DHCPSRCDIR=	client

.if (${MKDYNAMICROOT} == "no")
LDSTATIC?=	-static
.endif

.include <bsd.prog.mk>
