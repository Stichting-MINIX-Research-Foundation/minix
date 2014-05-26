#
# Makefile for the EARM USBD
#

PROG=		usbd
SRCS=		usbd.c usbd_earm.c hcd.c hcd_common.c hcd_ddekit.c musb_am335x.c musb_core.c

.PATH:		${.CURDIR}/../ ${.CURDIR}/../../hcd ${.CURDIR}/../../hcd/musb

CPPFLAGS+=	-I${.CURDIR}/../../include

DPADD+=		${LIBDDEKIT} ${LIBDDEKIT_USB_SERVER} ${LIBDEVMAN} ${LIBUSB} ${LIBMINC} ${LIBCLKCONF}
LDADD+=		-lddekit -lddekit_usb_server -ldevman -lusb -lminc -lclkconf
LDADD+=		-lsys -lminlib

MAN=

BINDIR?=	/usr/sbin

.include <minix.service.mk>
