.PATH: \
	${.CURDIR}/core ${.CURDIR}/core/ipv4 ${.CURDIR}/core/snmp

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
	\
	ip.c \
	igmp.c \
	icmp.c \
	ip_addr.c \
	ip_frag.c \
	inet.c \
	autoip.c \
	inet_chksum.c \
	\
	mib_structs.c \
	mib2.c \
	msg_out.c \
	asn1_enc.c \
	asn1_dec.c \
	msg_in.c
