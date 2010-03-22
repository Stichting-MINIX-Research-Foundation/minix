.include <minix.own.mk>

SUBDIR= csu libc libcurses libdriver libend libedit libm libsys \
	libtimers libutil

.if ${COMPILER_TYPE} == "ack"
SUBDIR+= ack/libd ack/libe ack/libfp ack/liby
.endif

.include <minix.subdir.mk>
