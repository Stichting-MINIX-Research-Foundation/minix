#	$NetBSD: Makefile,v 1.2 2012/11/18 08:24:02 apb Exp $

DRIVER=		xf86-video-xgi
DRIVER_NAME=	xgi_drv

SRCS=		init.c vb_ext.c vb_i2c.c vb_setmode.c xgi_accel.c
#SRC+=		vb_init.c
SRCS+=		xgi_cursor.c  xgi_dac.c xgi_dga.c xgi_driver.c xgi_opt.c
SRCS+=		xgi_setup.c  xgi_vb.c xgi_vga.c xgi_video.c xgi_videohw.c

MAN=		xgi.4

CPPFLAGS+=	-I${DESTDIR}${X11INCDIR}/X11
CPPFLAGS+=	-I${DESTDIR}${X11INCDIR}/X11/dri
CPPFLAGS+=	-I${DESTDIR}${X11INCDIR}/libdrm
# for now, I'd like to see this thing work first
CPPFLAGS+=	-DHAVE_UNISTD_H -Wno-deprecated-declarations

# vb_struct.h contains this declaration:
#
#	typedef struct _XGI_CRT1TableStruct
#	{
#	  UCHAR CR[15];
#	} XGI_CRT1TableStruct;
#
# but code in init.c and vb_setmode.c reads elements beyond the end of
# the array and writes the result into hardware registers.  It's not
# clear how to fix this, so just disable the -Warray-bounds warning to
# allow the build to complete.
#
COPTS.init.c=          -Wno-array-bounds
COPTS.vb_setmode.c=    -Wno-array-bounds

.include "../Makefile.xf86-driver"
