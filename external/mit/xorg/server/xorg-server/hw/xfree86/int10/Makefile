# $NetBSD: Makefile,v 1.8 2011/08/11 23:15:41 joerg Exp $

XMODULEDIR=	${X11USRLIBDIR}/modules

.include "../../../Makefile.serverlib"
.include "../../../Makefile.servermod"

LIB=		int10

.PATH:          ${X11SRCDIR.xorg-server}/hw/xfree86/int10
SRCS=		x86emu.c generic.c xf86x86emu.c xf86int10.c helper_mem.c \
		helper_exec.c xf86int10module.c

CPPFLAGS+=	-D__DRIVER__ -DFORCE_POST -D_CEXPORT= -DNO_LONG_LONG \
		-D_X86EMU -DNO_SYS_HEADERS

.if ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "x86_64"
CPPFLAGS+=	-D_PC
.endif

# do not muck around in IO space trying to enable / disable non-PCI VGAs
.if ${MACHINE} == "macppc" || ${MACHINE} == "sparc64"
CPPFLAGS+=	-DNO_LEGACY_VGA
.endif

CPPFLAGS+=	-I${X11SRCDIR.xorg-server}/hw/xfree86/common \
		-I${X11SRCDIR.xorg-server}/hw/xfree86/os-support \
		-I${X11SRCDIR.xorg-server}/hw/xfree86/os-support/bus \
		-I${X11SRCDIR.xorg-server}/hw/xfree86/${LIB} \
		-I${X11SRCDIR.xorg-server}/hw/xfree86/x86emu \
		-I${X11SRCDIR.xorg-server}/Xext \
		-I${X11SRCDIR.xorg-server}/include \
		-I${DESTDIR}${X11INCDIR}/X11 \
		-I${DESTDIR}${X11INCDIR}/X11/extensions \
		-I${DESTDIR}${X11INCDIR}/xorg \
		-I${DESTDIR}${X11INCDIR}/pixman-1

COPTS.xf86x86emu.c=	-Wno-error	# XXX?

CWARNFLAGS.clang+=	-Wno-format

.include <bsd.x11.mk>
LIBDIR=	${XMODULEDIR}
.include <bsd.lib.mk>
