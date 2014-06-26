# Makefile for the EARM USBD

PROG=		usbd
SRCS=		usbd.c usbd_earm.c hcd.c hcd_common.c hcd_ddekit.c hcd_schedule.c musb_am335x.c musb_core.c
MAN=
BINDIR=		/service

.PATH:		${.CURDIR}/../ ${.CURDIR}/../../hcd ${.CURDIR}/../../hcd/musb

CPPFLAGS+=	-I${.CURDIR}/../../include

DPADD+=		${LIBDDEKIT} ${LIBDDEKIT_USB_SERVER} ${LIBDEVMAN} ${LIBUSB} ${LIBMINC} ${LIBCLKCONF} ${LIBSYS} 
LDADD+=		-lddekit -lddekit_usb_server -ldevman -lusb -lminc -lclkconf -lsys 

.include <minix.service.mk>
