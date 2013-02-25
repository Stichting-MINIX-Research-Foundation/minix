CPPFLAGS+= -D_MINIX_COMPAT
LDADD+= -lminlib -lcompat_minix -lasyn -lterminfo
BINDIR?=/usr/bin
