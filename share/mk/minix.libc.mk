# NetBSD libc flags go here
NETBSD_LIBC_CPPFLAGS=-nostdinc -D__NBSD_LIBC -isystem /usr/netbsd/include
NETBSD_LIBC_LDFLAGS=-nostdlib /usr/netbsd/lib/crt1.o /usr/netbsd/lib/crti.o /usr/netbsd/lib/crtn.o -L/usr/netbsd/lib -lc -L/usr/pkg/lib -lgcc -L/usr/netbsd/lib -lminlib -lcompat_minix

# Minix libc flags go here
MINIX_LIBC_CPPFLAGS=
MINIX_LIBC_LDFLAGS=
