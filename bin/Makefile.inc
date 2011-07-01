# Requires NBSD_LIBC and clang/gcc (we default to using clang)
NBSD_LIBC:= yes
CC:=${CC:C/^cc/clang/}
COMPILER_TYPE:= gnu

CPPFLAGS+= -D_NETBSD_SOURCE

BINDIR?=/bin

