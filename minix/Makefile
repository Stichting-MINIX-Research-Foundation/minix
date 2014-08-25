
SUBDIR+=	include .WAIT
SUBDIR+=	benchmarks
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
SUBDIR+=	tests
SUBDIR+=	usr.bin
SUBDIR+=	usr.sbin

# BJG - build drivers last as the ramdisk depends on some other drivers
SUBDIR+=	.WAIT drivers

.include <bsd.subdir.mk>
