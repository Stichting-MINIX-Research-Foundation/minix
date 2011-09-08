.include <bsd.own.mk>

.if defined(NBSD_LIBC) && (${NBSD_LIBC} != "no")
LIBC_DIR=	nbsd_libc
LIBM_DIR=	nbsd_libm
LIBCOMPAT_DIR=	nbsd_libcompat_minix
LIBMINLIB_DIR=	nbsd_libminlib
LIBASYN_DIR=	nbsd_libasyn
.endif

LIBC_DIR?=	libc
LIBM_DIR?=	libm
LIBCOMPAT_DIR?=
LIBMINLIB_DIR?=
LIBASYN_DIR?=

SUBDIR= csu ${LIBCOMPAT_DIR} ${LIBC_DIR} libdriver libnetdriver \
	libedit ${LIBM_DIR} libsys libtimers libminixutil libbz2 libl libhgfs  \
	libz libfetch libarchive libvtreefs libaudiodriver libmthread     \
	libexec libdevman libusb ${LIBMINLIB_DIR} ${LIBASYN_DIR}          \
	libddekit libminixfs

.if defined(NBSD_LIBC) && (${NBSD_LIBC} != "no")
SUBDIR+= libelf libminc libcrypt libterminfo libcurses libvassert
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
