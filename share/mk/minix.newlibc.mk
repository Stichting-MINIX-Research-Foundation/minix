# Force clang/gcc and using new libc
# Requires NBSD_LIBC and clang/gcc (we default to using clang)
NBSD_LIBC:= yes
CC:=${CC:C/^cc/clang/}
COMPILER_TYPE:= gnu
