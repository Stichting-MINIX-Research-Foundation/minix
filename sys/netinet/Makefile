#	$NetBSD: Makefile,v 1.25 2012/09/15 17:41:59 plunky Exp $

INCSDIR= /usr/include/netinet

INCS=	\
	in.h \
	in_systm.h \
	\
	ip.h \
	ip_icmp.h \
	ip_var.h \
	tcp.h \
	udp.h udp_var.h \
	

.if !defined(__MINIX)
# ipfilter headers
# XXX shouldn't be here
.include <bsd.own.mk>

INCS+=	ip_compat.h	# always needed by kdump(1)

.if (${MKIPFILTER} != "no")
INCS+=	ip_auth.h ip_fil.h ip_frag.h ip_htable.h ip_nat.h \
	ip_lookup.h ip_pool.h ip_proxy.h ip_scan.h ip_state.h ip_sync.h \
	ipf_rb.h ipl.h
.endif
.endif # !defined(__MINIX)

.include <bsd.kinc.mk>

.if !defined(__MINIX)
.PATH: ${NETBSDSRCDIR}/sys/external/bsd/ipf/netinet
.endif # !defined(__MINIX)
