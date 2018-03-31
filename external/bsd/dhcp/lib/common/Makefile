# $NetBSD: Makefile,v 1.1 2013/03/24 15:54:31 christos Exp $

LIBISPRIVATE=	yes

LIB    = dhcp
SRCS   = raw.c parse.c nit.c icmp.c dispatch.c conflex.c upf.c bpf.c socket.c \
	 lpf.c dlpi.c packet.c tr.c ethernet.c memory.c print.c options.c \
	 inet.c tree.c tables.c alloc.c fddi.c ctrace.c \
	 dns.c resolv.c execute.c discover.c comapi.c ns_name.c
MAN    = dhcp-options.5 dhcp-eval.5

DHCPSRCDIR=	common

.include <bsd.lib.mk>

.if defined(__MINIX)
.if !empty(DBG:M-Og) || !empty(CFLAGS:M-Og) || \
	!empty(DBG:M-g) || !empty(CFLAGS:M-g)
#LSC: -Wno-maybe-uninitialized while compiling with -DNDEBUG -Og
CWARNFLAGS.gcc+=	-Wno-maybe-uninitialized
.endif
.endif # defined(__MINIX)
