#	$NetBSD: Makefile,v 1.5 2015/08/30 08:33:29 christos Exp $
#

NOLINT=	#defined

.include <bsd.own.mk>

.PATH:	${.CURDIR}/../../../../netbt

LIB=	rumpnet_netbt

SRCS=	bt_proto.c bt_sysctl.c						\
	hci_event.c hci_ioctl.c hci_link.c hci_misc.c hci_socket.c	\
	hci_unit.c							\
	l2cap_lower.c l2cap_misc.c l2cap_signal.c l2cap_socket.c	\
	l2cap_upper.c							\
	rfcomm_dlc.c rfcomm_session.c rfcomm_socket.c rfcomm_upper.c	\
	sco_socket.c sco_upper.c

.if ${HAVE_GCC:U} == "48"
.if ${MACHINE_ARCH} == "sh3eb" || ${MACHINE_ARCH} == "sh3el"
COPTS.l2cap_signal.c=	-O0
.endif
.endif
SRCS+=	netbt_component.c

.include <bsd.lib.mk>
.include <bsd.klinks.mk>
