# Makefile for the EARM USBD

PROG=		usbd
SRCS=		usbd.c usbd_earm.c hcd.c hcd_common.c hcd_ddekit.c musb_am335x.c musb_core.c
MAN=
BINDIR=		/usr/sbin

.PATH:		${.CURDIR}/../ ${.CURDIR}/../../hcd ${.CURDIR}/../../hcd/musb

CPPFLAGS+=	-I${.CURDIR}/../../include

DPADD+=		${LIBDDEKIT} ${LIBDDEKIT_USB_SERVER} ${LIBDEVMAN} ${LIBUSB} ${LIBMINC} ${LIBCLKCONF} ${LIBSYS} ${LIBMINLIB}
LDADD+=		-lddekit -lddekit_usb_server -ldevman -lusb -lminc -lclkconf -lsys -lminlib

.include <minix.service.mk>
