.include <bsd.own.mk>

.if defined(NBSD_LIBC) && (${NBSD_LIBC} != "no")
LIBC_DIR=	nbsd_libc
LIBM_DIR=	nbsd_libm
LIBUTIL_DIR=	libutil
LIBCOMPAT_DIR=	nbsd_libcompat_minix
LIBMINLIB_DIR=	nbsd_libminlib
LIBASYN_DIR=	nbsd_libasyn
.endif

LIBC_DIR?=	libc
LIBM_DIR?=	libm
LIBUTIL_DIR?=	libminixutil
LIBCOMPAT_DIR?=
LIBMINLIB_DIR?=
LIBASYN_DIR?=

SUBDIR= csu ${LIBCOMPAT_DIR} ${LIBC_DIR} libblockdriver libchardriver     \
	libnetdriver libedit ${LIBM_DIR} libsys libtimers ${LIBUTIL_DIR}  \
	libl libhgfs libz libfetch libvtreefs libaudiodriver libmthread   \
	libexec libdevman libusb ${LIBMINLIB_DIR} ${LIBASYN_DIR}          \
	libddekit libminixfs libbdev

.if defined(NBSD_LIBC) && (${NBSD_LIBC} != "no")
SUBDIR+= libelf libminc libcrypt libterminfo libcurses libvassert libutil \
	 libpuffs librefuse libbz2 libarchive libprop
.endif

.if ${COMPILER_TYPE} == "ack"
SUBDIR+= ack/libd ack/libe ack/libfp ack/liby
.endif

.if ${OBJECT_FMT} == "a.out"
SUBDIR+= libend
.endif
.include <bsd.subdir.mk>

build_ack:
	sh ack_build.sh obj dependall install

build_elf:
	sh elf_build.sh obj dependall install

clean_all:
	sh ack_build.sh clean
	sh elf_build.sh clean

cleandepend_all:
	sh ack_build.sh cleandepend
	sh elf_build.sh cleandepend
