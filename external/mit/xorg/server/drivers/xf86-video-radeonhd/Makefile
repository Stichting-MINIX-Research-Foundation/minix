#	$NetBSD: Makefile,v 1.7 2010/05/22 23:04:01 mrg Exp $

.include <bsd.own.mk>

DRIVER=		xf86-video-radeonhd
DRIVER_NAME=	radeonhd_drv

.PATH:	${X11SRCDIR.${DRIVER}}/src/AtomBios
SRCS.ATOMBIOS=	CD_Operations.c Decoder.c hwserv_drv.c

.if ${X11DRI} != "no"
SRCS.DRI=	rhd_dri.c r6xx_accel.c r600_exa.c
SRCS.DRI+=	r600_textured_videofuncs.c
CPPFLAGS+=	-DUSE_DRI
CPPFLAGS+=	${X11FLAGS.DRI}
.endif

SRCS.EXA=	r5xx_exa.c radeon_exa_render.c

SRCS=	r5xx_accel.c r5xx_xaa.c r600_shader.c radeon_3d.c
SRCS+=	radeon_textured_videofuncs.c rhd_atombios.c rhd_atomcrtc.c
SRCS+=	rhd_atomout.c rhd_atompll.c rhd_audio.c rhd_biosscratch.c
SRCS+=	rhd_connector.c rhd_crtc.c rhd_cs.c rhd_cursor.c rhd_dac.c
SRCS+=	rhd_ddia.c rhd_dig.c rhd_driver.c rhd_edid.c rhd_hdmi.c
SRCS+=	rhd_helper.c rhd_i2c.c rhd_id.c rhd_lut.c rhd_lvtma.c
SRCS+=	rhd_mc.c rhd_modes.c rhd_monitor.c rhd_output.c rhd_pll.c
SRCS+=	rhd_randr.c rhd_shadow.c rhd_tmds.c rhd_vga.c rhd_video.c
SRCS+=	rhd_pm.c rhd_acpi.c

SRCS+=		$(SRCS.EXA) $(SRCS.DRI) rhd_atomwrapper.c $(SRCS.ATOMBIOS)
MAN=		radeonhd.4

CPPFLAGS+=	-DDRIVER_PARSER -DDISABLE_EASF -DENABLE_ALL_SERVICE_FUNCTIONS

CPPFLAGS+=	-I${X11SRCDIR.${DRIVER}}/src/AtomBios/includes \
		-I${DESTDIR}${X11INCDIR}/libdrm
CPPFLAGS+=	-DHAVE_CONFIG_H -I${X11SRCDIR.${DRIVER}}/../include

# XXX
COPTS=	-Wno-error

#LSC: Clang/Minix3
DBG= -O0

.include "../Makefile.xf86-driver"
