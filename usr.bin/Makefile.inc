# Require clang and NBSD_LIBC
NBSD_LIBC:= yes
CC:= clang
COMPILER_TYPE:= gnu

CPPFLAGS+= -D_NETBSD_SOURCE

BINDIR?=/usr/bin

