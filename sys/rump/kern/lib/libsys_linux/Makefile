#	$NetBSD: Makefile,v 1.9 2015/05/09 12:03:10 pooka Exp $
#

.PATH:	${.CURDIR}/../../../../compat/linux/common

LIB=	rumpkern_sys_linux

SRCS=	linux_blkio.c linux_cdrom.c linux_errno.c linux_fdio.c		\
	linux_file.c linux_hdio.c linux_ioctl.c linux_ipc.c linux_misc.c\
	linux_mtio.c linux_signal.c linux_signo.c linux_socket.c	\
	linux_sysctl.c linux_termios.c linux_time.c linux_file64.c      \
	linux_pipe.c

SRCS+=	linux_rump.c
SRCS+=	sys_linux_component.c

# XXX
CPPFLAGS+= -I${RUMPTOP}/librump/rumpkern
CPPFLAGS+= -DINET6

SYS_P=rump_linux_sys
.include "../Makefile.sys"

.include <bsd.lib.mk>
.include <bsd.klinks.mk>
