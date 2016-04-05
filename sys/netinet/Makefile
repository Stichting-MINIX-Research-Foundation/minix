#	$NetBSD: Makefile,v 1.26 2015/02/10 19:11:52 rjs Exp $

INCSDIR= /usr/include/netinet

INCS=	icmp6.h icmp_var.h if_ether.h if_inarp.h igmp.h \
	igmp_var.h in.h in_pcb.h in_pcb_hdr.h \
	in_systm.h \
	in_var.h ip.h ip_carp.h ip6.h \
	ip_icmp.h ip_mroute.h ip_var.h pim_var.h \
	tcp.h tcp_debug.h tcp_fsm.h tcp_seq.h tcp_timer.h tcp_var.h \
	tcpip.h udp.h udp_var.h \
	tcp_vtw.h

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

.PATH: ${NETBSDSRCDIR}/sys/external/bsd/ipf/netinet
