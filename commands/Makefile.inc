CPPFLAGS+= -D_MINIX -D_NETBSD_SOURCE -D_MINIX_COMPAT
LDADD+= -lminlib -lcompat_minix -lasyn -lterminfo
BINDIR?=/usr/bin
