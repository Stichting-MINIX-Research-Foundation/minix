# Makefile for ramdisk image

.include <bsd.own.mk>

BINFILES = dev2name mount  sh service \
           sysenv

SBINFILES = ahci at_wini ext2 floppy procfs \
            fsck.mfs

ETCFILES  = system.conf mtab passwd pwd.db spwd.db master.passwd \
            rs.single

USRBINFILES  = loadramdisk cdprobe
USRSBINFILES  = acpi pci
IMAGE_DIRS= bin sbin etc usr/bin usr/sbin

install:
all:

# as we are taking the binaries directly out of the destree
# we have to make sure they are already installed
# (that's a little bit to coarse grain though...)
install_files:
	${MAKE} -C ${NETBSDSRCDIR}/commands install
	${MAKE} -C ${NETBSDSRCDIR}/usr.bin install
	${MAKE} -C ${NETBSDSRCDIR}/usr.sbin install
	${MAKE} -C ${NETBSDSRCDIR}/usr.bin install
	${MAKE} -C ${NETBSDSRCDIR}/sbin install
	${MAKE} -C ${NETBSDSRCDIR}/bin install
	${MAKE} -C ${NETBSDSRCDIR}/servers install
	${MAKE} -C ${NETBSDSRCDIR}/drivers/acpi install
	${MAKE} -C ${NETBSDSRCDIR}/drivers/ahci install
	${MAKE} -C ${NETBSDSRCDIR}/drivers/at_wini install
	${MAKE} -C ${NETBSDSRCDIR}/drivers/floppy install
	${MAKE} -C ${NETBSDSRCDIR}/drivers/pci install


image.d: install_files
	@echo -n "preparing image... "
	rm -rf image.d
	${INSTALL_DIR} image.d
	@for f in $(IMAGE_DIRS); do ${INSTALL_DIR} image.d/$$f;  done
	@for f in $(BINFILES); do  cp $(DESTDIR)/bin/$$f image.d/bin;  done
	@for f in $(SBINFILES); do  cp $(DESTDIR)/sbin/$$f image.d/sbin;  done
	@for f in $(ETCFILES); do  cp $(DESTDIR)/etc/$$f image.d/etc;  done
	@for f in $(USRBINFILES); do  cp $(DESTDIR)/usr/bin/$$f image.d/usr/bin;  done
	@for f in $(USRSBINFILES); do  cp $(DESTDIR)/usr/sbin/$$f image.d/usr/sbin;  done
	@cp rc image.d/etc;
	if [ ! -z ${EXT2_EXTRA_TREE} ] ; \
	then \
		echo "Copying additional files";\
		cp -a ${EXT2_EXTRA_TREE}/* image.d/; \
	fi
	@echo "done"

image:: image.d
	@echo -n "generating image... "
	genext2fs -q -N 1000 -D device_table.txt -b 21000 -U -d image.d  $@
	rm -rf image.d
	@echo "done."

clean:
	rm -rf image.d image
.PHONY:  image

.include <minix.service.mk>
