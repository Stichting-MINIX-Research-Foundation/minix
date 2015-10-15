.include <bsd.own.mk>

SUBDIR+=	include .WAIT
SUBDIR+=	bin
SUBDIR+=	commands
SUBDIR+=	fs
SUBDIR+=	kernel
SUBDIR+=	lib
SUBDIR+=	llvm
SUBDIR+=	man
SUBDIR+=	net
SUBDIR+=	sbin
SUBDIR+=	servers
SUBDIR+=	share
.if ${MKATF} == "yes"
SUBDIR+=	tests
.endif
SUBDIR+=	usr.bin
SUBDIR+=	usr.sbin

# BJG - build drivers last as the ramdisk depends on some other drivers
SUBDIR+=	.WAIT drivers

.include <bsd.subdir.mk>
