# Makefile for the lwIP TCP/IP socket driver service (LWIP)

.include <bsd.own.mk>

PROG=	lwip
SRCS=	lwip.c mempool.c pchain.c addr.c addrpol.c tcpisn.c mcast.c ipsock.c \
	pktsock.c tcpsock.c udpsock.c rawsock.c ifdev.c ifaddr.c loopif.c \
	ethif.c ndev.c rttree.c route.c rtsock.c lnksock.c lldata.c mibtree.c \
	ifconf.c bpfdev.c bpf_filter.c util.c

FILES=${PROG}.conf
FILESNAME=${PROG}
FILESDIR= /etc/system.conf.d

CPPFLAGS+=	-I${NETBSDSRCDIR}/minix/lib/liblwip/dist/src/include
CPPFLAGS+=	-I${NETBSDSRCDIR}/minix/lib/liblwip/lib

# Disabling USE_INET6 only superficially hides IPv6 support in the service.
.if (${USE_INET6} != "no")
CPPFLAGS+=	-DINET6
.endif

# Some warnings are the result of usage of lwIP macros.  We must not generate
# errors for those, but even producing the warnings is not helpful, so we
# disable them altogether.
CPPFLAGS+=	-Wno-address

DPADD+=	${LIBLWIP} ${LIBSOCKEVENT} ${LIBSOCKDRIVER} ${LIBCHARDRIVER} \
	${LIBSYS} ${LIBTIMERS}
LDADD+=	-llwip -lsockevent -lsockdriver -lchardriver -lsys -ltimers

WARNS?=	5

.if defined(__MINIX)
.if !empty(DBG:M-Og) || !empty(CFLAGS:M-Og) || \
	!empty(DBG:M-g) || !empty(CFLAGS:M-g)
#LSC: -Wno-maybe-uninitialized while compiling with -DNDEBUG -Og
CWARNFLAGS.gcc+=	-Wno-maybe-uninitialized
.endif
.endif # defined(__MINIX)

.include <minix.service.mk>
