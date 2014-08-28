# Makefile for Mass Storage driver
PROG= usb_storage
SRCS= urb_helper.c bulk.c scsi.c usb_storage.c

FILES= $(PROG).conf
FILESNAME= $(PROG)
FILESDIR= /etc/system.conf.d

DPADD+= ${LIBBLOCKDRIVER} ${LIBDDEKIT_USB_CLIENT} ${LIBSYS} ${LIBDDEKIT} ${LIBUSB}
LDADD+= -lblockdriver -lddekit_usb_client -lsys -lddekit -lusb

#For easier debugging, uncomment:
#LDADD+= -Wl,-Ttext=0x800000
#CPPFLAGS+= -DMASS_DEBUG

.include <minix.service.mk>
