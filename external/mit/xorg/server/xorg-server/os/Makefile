#	$NetBSD: Makefile,v 1.11 2015/07/23 08:03:25 mrg Exp $

.include "../Makefile.serverlib"
.include "../Makefile.servermod"

LIB=	os

.PATH:	${X11SRCDIR.xorg-server}/os
SRCS=	WaitFor.c access.c auth.c connection.c io.c mitauth.c \
	oscolor.c osinit.c utils.c xdmauth.c \
	xstrans.c xprintf.c log.c xdmcp.c \
	rpcauth.c backtrace.c xprintf.c xsha1.c

CPPFLAGS+=	-I${X11SRCDIR.xorg-server}/include \
		-I${DESTDIR}${X11INCDIR}/X11 \
		-I${DESTDIR}${X11INCDIR}/X11/extensions \
		-I${DESTDIR}${X11INCDIR}/pixman-1 \
		-I${X11SRCDIR.xorg-server}/Xext \
		-I${X11SRCDIR.xorg-server}/render \
		-I${X11SRCDIR.xorg-server}/lbx \
		-I${X11SRCDIR.xorg-server}/../include

CPPFLAGS+=	${X11FLAGS.DIX} \
		${X11FLAGS.CONNECTION} \
		${X11FLAGS.EXTENSION} \
		-DHASXDMAUTH \
		${X11FLAGS.OS_DEFINES} \
		-DUSE_RGB_TXT \
		-UDDXTIME \
		-DHAVE_SHA1_IN_LIBC \
		-DMONOTONIC_CLOCK

.include "../Makefile.common"

.if 0
# XXX The XFree86 Xserver has its own GetTimeInMillis(), however if we are
#     building XFree86 and Xmacppc or Xsun* we want both copies.  The copy
#     here is linked into the non XFree86 server.
#.if ${XSERVER_XFREE86} == "no" || \
#    ${XSERVER_XMACPPC} != "no" || \
#    ${XSERVER_XSUN} != "no"
SRCS+=	GetTimeInMillis.c
SRCS+=	ddxInitGlobals.c
#.endif

XDMCP_DEFINES=		-DXDMCP -DXDMCP_NO_IPV6

CPPFLAGS.WaitFor.c=	${X11FLAGS.EXTENSION}
CPPFLAGS.access.c=	${XDMCP_DEFINES} -DBSD44SOCKETS -DHAS_GETIFADDRS
CPPFLAGS.auth.c=	${XDMCP_DEFINES} -DHAVE_ARC4RANDOM
CPPFLAGS.connection.c=	${XDMCP_DEFINES} -DBSD44SOCKETS
CPPFLAGS.io.c=		${X11FLAGS.EXTENSION}
CPPFLAGS.lbxio.c=	${X11FLAGS.EXTENSION}
CPPFLAGS.oscolor.c=	-DNDBM
CPPFLAGS.osinit.c=	-DADMPATH=\"/var/log/X\%smsgs\"
CPPFLAGS.transport.c=	-DBSD44SOCKETS
CPPFLAGS.utils.c=	${XDMCP_DEFINES} ${X11FLAGS.EXTENSION}
CPPFLAGS.xdmauth.c=	${XDMCP_DEFINES}
CPPFLAGS.xdmcp.c=	${XDMCP_DEFINES} -DBSD44SOCKETS
.endif

COPTS.xdmauth.c=	-Wno-error	# XXX
COPTS.xstrans.c=	-Wno-error	# XXX
COPTS.access.c=		-Wno-error	# XXX bigreqstr.h
COPTS.io.c=		-Wno-error	# XXX bigreqstr.h

.include <bsd.x11.mk>
.include <bsd.lib.mk>
