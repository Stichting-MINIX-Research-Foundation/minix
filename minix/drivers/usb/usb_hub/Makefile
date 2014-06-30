# Makefile for USB hub driver
PROG=usb_hub
SRCS=usb_hub.c urb_helper.c

FILES=$(PROG).conf
FILESNAME=$(PROG)
FILESDIR=/etc/system.conf.d

DPADD+=${LIBDDEKIT_USB_CLIENT} ${LIBDDEKIT} ${LIBUSB} ${LIBSYS} ${LIBMINLIB}
LDADD+=-lddekit_usb_client -lddekit -lusb -lsys -lminlib

#For easier debugging, uncomment:
#LDADD+=-Wl,-Ttext=0x800000
#CPPFLAGS+=-DHUB_DEBUG

MAN=

BINDIR?= /usr/sbin

.include <minix.service.mk>
