PROG=		mkfs.mfs
BINDIR=		/sbin
# XXX consider section 8
MAN=		mkfs.mfs.1

.ifndef HOSTPROGNAME
SYMLINKS+= $(BINDIR)/$(PROG) /usr/sbin/mkfs.mfs3
.endif

.include <../Makefile.mkfs>
