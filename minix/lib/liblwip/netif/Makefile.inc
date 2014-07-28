.PATH:  ${.CURDIR}/netif ${.CURDIR}/netif/ppp

SRCS += \
	etharp.c \
	ethernetif.c \
	slipif.c \
	\
	chap.c \
	magic.c \
	pap.c \
	fsm.c \
	vj.c \
	md5.c \
	ppp_oe.c \
	lcp.c \
	ipcp.c \
	auth.c \
	ppp.c \
	randm.c \
	chpms.c
