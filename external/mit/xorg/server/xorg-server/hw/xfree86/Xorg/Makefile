#	$NetBSD: Makefile,v 1.5 2012/10/03 21:50:14 gsutre Exp $

.include <bsd.own.mk>

.include "../../../Makefile.Xserver"

PROG=		Xorg
BINMODE=	4711

.PATH:		${X11SRCDIR.xorg-server}/hw/xfree86
SRCS=		xorg.c libxorg.c

CPPFLAGS+=	-I${DESTDIR}${X11INCDIR}/X11 \
		-I${X11SRCDIR.xc}/include/fonts \
		-I${X11SRCDIR.xorg-server}/fb \
		-I${X11SRCDIR.xorg-server}/mi \
		-I${X11SRCDIR.xorg-server}/include \
		-I${X11SRCDIR.xorg-server}/os \
		-I${X11SRCDIR.xorg-server}/Xext \
		-I${DESTDIR}${X11INCDIR}/X11/extensions \
		-I${DESTDIR}${X11INCDIR}/pixman-1 \
		-I${DESTDIR}${X11INCDIR}/xorg \
		-I${X11SRCDIR.xorg-server}/render \
		-DHAVE_DIX_CONFIG_H

CPPFLAGS+=	${X11FLAGS.OS_DEFINES} ${X11FLAGS.DIX} \
		${X11FLAGS.EXTENSION}

LDFLAGS+=	-rdynamic

LDADD+=	-Wl,--whole-archive
LDADD+=	${LDADD.dix}
LDADD+=	${LDADD.hw/xfree86/init}
LDADD+=	${LDADD.config}
LDADD+=	${LDADD.hw/xfree86/loader}
LDADD+=	${LDADD.hw/xfree86/common}
LDADD+=	${LDADD.hw/xfree86/xorgos}
#LDADD+=	${LDADD.osandcommon}
LDADD+=	${LDADD.hw/xfree86/xf86config}
LDADD+=	${LDADD.hw/xfree86/dixmods}
LDADD+=	${LDADD.hw/xfree86/xf86modes}
LDADD+=	${LDADD.hw/xfree86/ramdac}
LDADD+=	${LDADD.hw/xfree86/ddc}
LDADD+=	${LDADD.hw/xfree86/i2c}
LDADD+=	${LDADD.composite}
LDADD+=	${LDADD.mi}
LDADD+=	${LDADD.xfixes}
LDADD+=	${LDADD.Xext/Xextbuiltin}
LDADD+=	${LDADD.render}
LDADD+=	${LDADD.randr}
LDADD+=	${LDADD.damageext}
LDADD+=	${LDADD.miext/damage}
LDADD+=	${LDADD.miext/shadow}
LDADD+=	${LDADD.miext/cw}
LDADD+=	${LDADD.miext/sync}
LDADD+=	${LDADD.Xi}
LDADD+=	${LDADD.xkb}
LDADD+=	${LDADD.os}
LDADD+=	${LDADD.glx}
LDADD+=	${LDADD.hw/xfree86/dri}
LDADD+=	${LDADD.hw/xfree86/dri2}
LDADD+=	${LDADD.hw/xfree86/dixmods/fb}
LDADD+=	${LDADD.hw/xfree86/dixmods/xorgxkb}
LDADD+=	-lpixman-1 -lXfont -lfreetype -lXau -lfontenc -lXdmcp
LDADD+=	-lpciaccess -ldrm
.if !defined(__MINIX)
LDADD+=	-lcrypto
.endif # !defined(__MINIX)
LDADD+=	-Wl,--no-whole-archive

.if !defined(__MINIX)
.if ${MACHINE_ARCH} == "i386"
LDADD+=	-li386
DPADD+=	${LIBI386}
.elif ${MACHINE_ARCH} == "x86_64"
LDADD+=	-lx86_64
DPADD+=	${LIBX86_64}
.elif ${MACHINE_ARCH} == "alpha"
LDADD+=	-lalpha
DPADD+=	${LIBALPHA}
.endif
.endif # !defined(__MINIX)

LDADD+=	-lm

DPADD+=	${DPADD.dix} \
	${DPADD.fb} \
	${DPADD.mi} \
	${DPADD.xfixes} \
	${DPADD.Xext} \
	${DPADD.config} \
	${DPADD.dbe} \
	${DPADD.record} \
	${DPADD.render} \
	${DPADD.randr} \
	${DPADD.damageext} \
	${DPADD.miext/damage} \
	${DPADD.miext/shadow} \
	${DPADD.miext/cw} \
	${DPADD.miext/sync} \
	${DPADD.Xi} \
	${DPADD.xkb} \
	${DPADD.composite} \
	${DPADD.os} \
	${DPADD.glx} \
	${DPADD.hw/xfree86/dri} \
	${DPADD.hw/xfree86/dri2} \
	${DPADD.hw/xfree86/dixmods/fb} \
	${DPADD.hw/xfree86/fb} \
	${DPADD.hw/xfree86/init} \
	${DPADD.hw/xfree86/common} \
	${DPADD.hw/xfree86/loader} \
	${DPADD.hw/xfree86/xf86config} \
	${DPADD.hw/xfree86/dixmods} \
	${DPADD.hw/xfree86/xf86modes} \
	${DPADD.hw/xfree86/ramdac} \
	${DPADD.hw/xfree86/ddc} \
	${DPADD.hw/xfree86/i2c} \
	${LIBCWRAPPER} \
	${LIBXFONT} ${LIBFREETYPE} ${LIBXAU} ${LIBFONTENC} \
	${LIBPIXMAN1} ${LIBXDMCP} ${LIBZ} ${LIBM} ${LIBPCIACCESS} \
	# LSC: For MINIX
	#${LIBCRYPTO}

SYMLINKS=	${X11BINDIR}/Xorg ${X11BINDIR}/X

# Done in ../doc
MKMAN=	no

FILES=		xorg-server.m4
FILESDIR=	${X11ROOTDIR}/share/aclocal

.PATH:		${X11SRCDIR.xorg-server}

.include <bsd.x11.mk>
.include <bsd.prog.mk>
.include <bsd.files.mk>
