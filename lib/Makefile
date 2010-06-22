.include <minix.own.mk>

SUBDIR= csu libc libcurses libdriver libnetdriver libend libedit libm libsys \
	libtimers libutil libbz2 libl libhgfs libz

.if ${COMPILER_TYPE} == "ack"
SUBDIR+= ack/libd ack/libe ack/libfp ack/liby
.endif

.include <minix.subdir.mk>

build_ack:
	sh ack_build.sh obj depend all install

build_gnu:
	sh gnu_build.sh obj depend all install

clean_ack:
	sh ack_build.sh clean

clean_gnu:
	sh gnu_build.sh clean
