CPPFLAGS+=-DTIME -DMINIX=1 -D_MINIX=1 -D_POSIX_SOURCE=1 -I../../src/ -D_NETBSD_SOURCE
HZ=60
BINDIR=/usr/benchmarks/unixbench/pgms

.PATH: ${.CURDIR}/../../src
