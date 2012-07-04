.PATH: \
	${.CURDIR}/core ${.CURDIR}/core/ipv4 ${.CURDIR}/core/ipv6 ${.CURDIR}/core/snmp

SRCS += \
	def.c \
	dns.c \
	tcp_out.c \
	timers.c \
	mem.c \
	init.c \
	tcp.c \
	raw.c \
	dhcp.c \
	sys.c \
	udp.c \
	tcp_in.c \
	netif.c \
	memp.c \
	pbuf.c \
	stats.c \
	inet_chksum.c \
	\
	ip4.c \
	igmp.c \
	icmp.c \
	ip4_addr.c \
	ip_frag.c \
	autoip.c \
	\
	dhcp6.c \
	ethip6.c \
	icmp6.c \
	inet6.c \
	ip6.c \
	ip6_addr.c \
	ip6_frag.c \
	mld6.c \
	nd6.c \
	\
	mib_structs.c \
	mib2.c \
	msg_out.c \
	asn1_enc.c \
	asn1_dec.c \
	msg_in.c
