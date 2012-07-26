# $NetBSD: Makefile,v 1.14 2008/08/29 00:02:25 gmcgarry Exp $

.include <bsd.own.mk>

SRCS   = dhcpd.c dhcp.c bootp.c confpars.c db.c class.c failover.c \
         omapi.c mdb.c stables.c salloc.c ddns.c mdb6.c dhcpv6.c \
	 dhcpleasequery.c
PROG   = dhcpd
MAN    = dhcpd.8 dhcpd.conf.5 dhcpd.leases.5

DHCPSRCDIR=	server

#.if ${MKSHARE} != "no"
#FILESDIR= /usr/share/examples/dhcp
#FILES=	dhcpd.conf
#.endif

.if defined(HAVE_PCC)
COPTS += -Wno-unreachable-code
.endif

.include <bsd.prog.mk>
