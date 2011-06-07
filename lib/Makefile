.include <bsd.own.mk>

SUBDIR= csu libc libcurses libdriver libnetdriver libedit libm libsys \
	libtimers libutil libbz2 libl libhgfs libz libfetch libarchive    \
	libvtreefs libaudiodriver libmthread libexec libdevman libusb    \
        libddekit 

.if ${COMPILER_TYPE} == "ack"
SUBDIR+= ack/libd ack/libe ack/libfp ack/liby
.endif

.if ${OBJECT_FMT} == "a.out"
SUBDIR+= libend
.endif

.include <bsd.subdir.mk>

build_ack:
	sh ack_build.sh obj depend all install

.if defined(MINIX_GENERATE_ELF)
build_elf_base:
	sh elf_build_base.sh obj depend all install
.else
build_gnu:
	sh gnu_build.sh obj depend all install
.endif

clean_all:
	sh ack_build.sh clean
	sh clang_build.sh clean
	sh gnu_build.sh clean
	sh elf_build_base.sh clean
	sh elf_build.sh clean
