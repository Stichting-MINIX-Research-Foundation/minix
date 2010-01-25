# Makefile for VMware Host/Guest File System (HGFS) server
SERVER=hgfs

LIBHGFSDIR=./libhgfs
LIBHGFS=$(LIBHGFSDIR)/libhgfs.a

DEST=/sbin/$(SERVER)
LIBS=-lsys -L$(LIBHGFSDIR) -lhgfs

OBJ=dentry.o handle.o inode.o link.o lookup.o main.o \
	misc.o mount.o name.o optset.o path.o read.o \
	stat.o table.o util.o verify.o write.o

all build: $(SERVER)

$(SERVER): $(LIBHGFS) $(OBJ)
	$(CC) -o $@ $(LDFLAGS) $(OBJ) $(LIBS)

$(LIBHGFS):
	cd $(LIBHGFSDIR) && $(MAKE)

install: $(SERVER)
	install -c $(SERVER) $(DEST)

clean:
	cd $(LIBHGFSDIR) && $(MAKE) $@
	rm -f $(SERVER) $(OBJ)

depend:
	cd $(LIBHGFSDIR) && $(MAKE) $@
	mkdep "$(CC) -E $(CPPFLAGS)" *.c > .depend

# Include generated dependencies.
include .depend
