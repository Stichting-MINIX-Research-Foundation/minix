##	$NetBSD: Makefile.inc,v 1.6 2002/02/17 20:03:11 thorpej Exp $

SRCS+= netif_small.c

.if (${USE_NETIF} == "3c509")
SRCS+= 3c509.c elink3.c
.endif

.if (${USE_NETIF} == "3c590")
SRCS+= 3c590.c elink3.c
.endif

.if (${USE_NETIF} == "wd80x3")
SRCS+= wd80x3.c dp8390.c
.endif

.if (${USE_NETIF} == "pcnet_pci")
SRCS+= pcnet_pci.c am7990.c
.endif

.if (${USE_NETIF} == "pcnet_isapnp")
SRCS+= pcnet_isapnp.c am7990.c
.endif

.if (${USE_NETIF} == "i82557")
SRCS+= i82557.c
.endif

.if (${USE_NETIF} == "3c90xb")
SRCS+= 3c90xb.c
.endif

.if (${USE_NETIF} == "ne2000_isa")
SRCS+= ne.c dp8390.c
CPPFLAGS+= -DSUPPORT_NE2000
.endif
