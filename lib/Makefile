.include <minix.own.mk>

SUBDIR= csu libc libcurses libdriver libnetdriver libend libedit libm libsys \
	libtimers libutil libbz2 libl libhgfs

.if ${COMPILER_TYPE} == "ack"
SUBDIR+= ack/libd ack/libe ack/libfp ack/liby
.endif

.include <minix.subdir.mk>
