# Makefile for ISO9660 fs
SERVER = isofs
NR_BUFS = 100

# directories
u = /usr
i = $u/include
s = $i/sys
h = $i/minix

# programs, flags, etc.
CC =	exec cc
CFLAGS = -I$i $(EXTRA_OPTS) $(CPROFILE) -DNR_BUFS=$(NR_BUFS)
LDFLAGS = -i
LIBS = -lsys -ltimers

OBJ =	main.o table.o mount.o super.o inode.o device.o \
	utility.o misc.o path.o read.o stadir.o cache.o 

# build local binary 
all build:	$(SERVER)
$(SERVER):	$(OBJ)
	$(CC) -o $@ $(LDFLAGS) $(OBJ) $(LIBS)
	install -S 64k $(SERVER)

install: $(SERVER)
	install $(SERVER) /sbin/$(SERVER)

# clean up local files
clean:
	rm -f $(SERVER) *.o *.bak *~

depend: 
	mkdep "$(CC) -E $(CPPFLAGS)" *.c > .depend

# Include generated dependencies.
include .depend
