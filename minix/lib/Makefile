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
SUBDIR+=	libfetch
SUBDIR+=	libinputdriver
SUBDIR+=	libminc
SUBDIR+=	libminixfs
SUBDIR+=	libnetdriver
SUBDIR+=	libsffs
SUBDIR+=	libtimers
SUBDIR+=	libusb
SUBDIR+=	libvtreefs

.if (${HAVE_LIBGCC} == "no")
SUBDIR+=	libgcc_s_empty
.endif

.if (${MKLWIP} == "yes")
SUBDIR+=	liblwip
SUBDIR+=	libnetsock
.endif

.if (${MACHINE_ARCH} == "i386")
SUBDIR+=	libhgfs
SUBDIR+=	libvassert
SUBDIR+=	libvboxfs
SUBDIR+=	libvirtio
.endif

.if (${MACHINE_ARCH} == "earm")
SUBDIR+=	libclkconf
SUBDIR+=	libgpio
SUBDIR+=	libi2cdriver
.endif

.include <bsd.buildinstall.mk>
.include <bsd.subdir.mk>
