.include <bsd.own.mk>

SUBDIR+=	libsys
SUBDIR+=	libmthread
SUBDIR+=	.WAIT

SUBDIR+=	libasyn
SUBDIR+=	libaudiodriver
SUBDIR+=	libbdev
SUBDIR+=	libblockdriver
SUBDIR+=	libchardriver
SUBDIR+=	libddekit
SUBDIR+=	libdevman
SUBDIR+=	libexec
SUBDIR+=	libfsdriver
SUBDIR+=	libinputdriver
SUBDIR+=	liblwip
SUBDIR+=	libminc
SUBDIR+=	libminixfs
SUBDIR+=	libnetdriver
SUBDIR+=	libsffs
SUBDIR+=	libsockdriver
SUBDIR+=	libsockevent
SUBDIR+=	libtimers
SUBDIR+=	libusb
SUBDIR+=	libvtreefs

.if (${HAVE_LIBGCC} == "no")
SUBDIR+=	libgcc_s_empty
.endif

.if (${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "x86_64")
SUBDIR+=	libacpi
.endif

.if (${MACHINE_ARCH} == "i386")
SUBDIR+=	libhgfs
SUBDIR+=	libvassert
SUBDIR+=	libvboxfs
.endif

.if (${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "x86_64")
SUBDIR+=	libvirtio
.endif

.if (${MACHINE_ARCH} == "earm")
SUBDIR+=	libclkconf
SUBDIR+=	libgpio
SUBDIR+=	libi2cdriver
.endif

.if (${MKMAGIC} == "yes")
SUBDIR+=	libmagicrt
.endif

.include <bsd.buildinstall.mk>
.include <bsd.subdir.mk>
