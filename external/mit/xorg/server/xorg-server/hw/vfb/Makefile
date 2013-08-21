#	$NetBSD: Makefile,v 1.11 2011/08/02 09:11:38 mrg Exp $

.include <bsd.own.mk>

.include "../../Makefile.Xserver"

PROG=		Xvfb

.PATH:		${X11SRCDIR.xorg-server}/hw/vfb
.PATH:		${X11SRCDIR.xorg-server}/hw/vfb/man
.PATH:		${X11SRCDIR.xorg-server}/Xext
.PATH:		${X11SRCDIR.xorg-server}/Xi
.PATH:		${X11SRCDIR.xorg-server}/mi
SRCS=		InitInput.c InitOutput.c dpmsstubs.c miinitext.c stubs.c

.PATH:		${X11SRCDIR.xorg-server}/fb
SRCS+=		fbcmap_mi.c

CPPFLAGS+=	-I${DESTDIR}${X11INCDIR}/X11 \
		-I${X11SRCDIR.xorg-server}/fb \
		-I${X11SRCDIR.xorg-server}/mfb \
		-I${X11SRCDIR.xorg-server}/mi \
		-I${X11SRCDIR.xorg-server}/include \
		-I${X11SRCDIR.xorg-server}/os \
		-I${X11SRCDIR.xorg-server}/Xext \
		-I${DESTDIR}${X11INCDIR}/X11/extensions \
		-I${DESTDIR}${X11INCDIR}/pixman-1 \
		-I${DESTDIR}${X11INCDIR}/xorg \
		-I${X11SRCDIR.xorg-server}/render \
		-DHAVE_DIX_CONFIG_H \
		-DNO_HW_ONLY_EXTS \
		-DNO_MODULE_EXTS \
		-UGLXEXT

CPPFLAGS+=	${X11FLAGS.OS_DEFINES} ${X11FLAGS.DIX} \
		-DHAS_SHM -DHAS_MMAP -UXFree86LOADER

.include "../../Makefile.common"

.if ${XSERVER_XFREE86} == "no"
CPPFLAGS+=	-UDDXTIME
.endif

CPPFLAGS.InitInput.c=	${X11FLAGS.EXTENSION} \
			-UGLXEXT
CPPFLAGS.miinitext.c=	${X11FLAGS.EXTENSION} \
			-DNO_HW_ONLY_EXTS -DNO_MODULE_EXTS \
			-UXFree86LOADER \
			-UGLXEXT
CPPFLAGS.dpmsstubs.c=	${X11FLAGS.EXTENSION} \
			-UGLXEXT

COPTS.InitInput.c=	-Wno-error
COPTS.InitOutput.c=	-Wno-error

LDADD+=	${LDADD.dix} \
	${LDADD.fb} \
	${LDADD.mi} \
	${LDADD.xfixes} \
	${LDADD.Xext} \
	${LDADD.config} \
	${LDADD.dbe} \
	${LDADD.record} \
	${LDADD.randr} \
	${LDADD.render} \
	${LDADD.damageext} \
	${LDADD.miext/damage} \
	${LDADD.miext/shadow} \
	${LDADD.miext/sync} \
	${LDADD.Xi} \
	${LDADD.xkb} \
	${LDADD.xkbstubs} \
	${LDADD.composite} \
	${LDADD.os} \
	${LDADD.glx} \
	-lXfont \
	-lfreetype \
	-lXau \
	-lfontenc \
	${LIBCWRAPPER} \
	-lpixman-1 \
	-lXdmcp \
	-lz \
	-lm \
	-lcrypto

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
	${DPADD.miext/sync} \
	${DPADD.Xi} \
	${DPADD.xkb} \
	${DPADD.xkbstubs} \
	${DPADD.composite} \
	${DPADD.os} \
	${DPADD.glx} \
	${LIBXFONT} \
	${LIBFREETYPE} \
	${LIBXAU} \
	${LIBFONTENC} \
	${LIBPIXMAN1} \
	${LIBXDMCP} \
	${LIBZ} \
	${LIBM} \
	${LIBCRYPTO}

.if ${MACHINE_ARCH} == "i386"
LDADD+=	-li386
DPADD+=	${LIBI386}
.endif

.if ${MACHINE_ARCH} == "x86_64"
LDADD+=	-lx86_64
DPADD+=	${LIBX86_64}
.endif

.if ${MACHINE_ARCH} == "alpha"
LDADD+=	-lalpha
DPADD+=	${LIBALPHA}
.endif

COPTS.miinitext.c+=	-Wno-error	# XXX xf86bigfstr.h

.include <bsd.x11.mk>
.include <bsd.prog.mk>
