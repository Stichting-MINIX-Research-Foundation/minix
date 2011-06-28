# NetBSD libc flags go here
NETBSD_LIBC_CPPFLAGS=-B/usr/netbsd -D__NBSD_LIBC
NETBSD_LIBC_LDFLAGS=-B/usr/netbsd:/usr/netbsd/lib -lminlib -lcompat_minix
# Minix libc flags go here
MINIX_LIBC_CPPFLAGS=
MINIX_LIBC_LDFLAGS=
