# Requires clang/gcc for NetBSD libc
CC:=${CC:C/^cc/clang/}
COMPILER_TYPE:= gnu

CPPFLAGS+= -D_NETBSD_SOURCE -D__NBSD_LIBC=1

BINDIR?=/usr/bin

