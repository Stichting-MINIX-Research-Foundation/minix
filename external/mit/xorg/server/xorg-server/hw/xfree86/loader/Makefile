#	$NetBSD: Makefile,v 1.12 2012/03/27 00:36:06 joerg Exp $

.include "../../../Makefile.serverlib"
.include "../../../Makefile.servermod"

LIB=	loader
LOADERDIR=	${X11SRCDIR.xorg-server}/hw/xfree86/${LIB}
.PATH:	${LOADERDIR}

SRCS=	loader.c loadext.c loadmod.c os.c sdksyms.c

.if ${MACHINE_ARCH} == "sparc" || ${MACHINE_ARCH} == "sparc64"
#SRCS+=	SparcMulDiv.S
.endif

CPPFLAGS+=	-I${X11SRCDIR.xorg-server}/hw/xfree86/${LIB} \
		-I${X11SRCDIR.xorg-server}/hw/xfree86/parser \
		-I${X11SRCDIR.xorg-server}/hw/xfree86/dixmods/extmod \
		-I${X11SRCDIR.xorg-server}/hw/xfree86/vbe \
		-I${X11SRCDIR.xorg-server}/miext/cw \
		-I${X11SRCDIR.xorg-server}/miext/sync \
		-I${X11SRCDIR.xorg-server}/hw/xfree86/int10 \
		-I${X11SRCDIR.xorg-server}/hw/xfree86/ddc \
		-I${X11SRCDIR.xorg-server}/hw/xfree86/i2c \
		-I${X11SRCDIR.xorg-server}/hw/xfree86/modes \
		-I${X11SRCDIR.xorg-server}/hw/xfree86/ramdac \
		-I${X11SRCDIR.xorg-server}/hw/xfree86/common \
		-I${X11SRCDIR.xorg-server}/hw/xfree86/os-support/bus \
		-I${X11SRCDIR.xorg-server}/os \
		-I${X11SRCDIR.xorg-server}/Xext \
		-I${X11SRCDIR.xorg-server}/mi \
		-I${X11SRCDIR.xorg-server}/xfixes \
		-I${DESTDIR}${X11INCDIR}/X11 \
		-I${DESTDIR}${X11INCDIR}/pixman-1 \
		-I${DESTDIR}${X11INCDIR}/xorg

CPPFLAGS+=	${X11FLAGS.OS_DEFINES} -DDLOPEN_SUPPORT -DHAS_SHM \
		-DIN_LOADER \
		${x11FLAGS.DIX}

.if ${MACHINE_ARCH} == "x86_64"
CPPFLAGS+=	-DUseMMAP
.endif

CPPFLAGS.loadmod.c=	${X11FLAGS.EXTENSION} -DDEFAULT_MODULE_PATH=\"${X11USRLIBDIR}/modules\"
CPPFLAGS.sdksyms.c=	${X11FLAGS.EXTENSION}

#COPTS.sdksyms.c=		-Wno-error
COPTS.loadmod.c=		-Wno-error

sdksyms.c: sdksyms.sh
	CPP=${CPP:Q} AWK=${TOOL_AWK:Q} ${HOST_SH} $(LOADERDIR)/sdksyms.sh $(X11SRCDIR.xorg-server) ${CFLAGS} ${CPPFLAGS}
sdksyms.d: sdksyms.c
CLEANFILES+=	sdksyms.c

.include <bsd.x11.mk>
.include <bsd.lib.mk>
