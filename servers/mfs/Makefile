# Makefile for File System (FS)
SERVER = mfs

# directories
u = /usr
i = $u/include
s = $i/sys
h = $i/minix

# programs, flags, etc.
CC =	exec cc
CFLAGS = -I$i $(EXTRA_OPTS)
LDFLAGS = -i
LIBS = -lsys -lsysutil -ltimers

OBJ =	cache.o device.o link.o \
	mount.o misc.o open.o pipe.o protect.o read.o \
	stadir.o table.o time.o utility.o \
	write.o inode.o main.o path.o super.o

# build local binary 
install all build:	$(SERVER)
$(SERVER):	$(OBJ)
	$(CC) -o $@ $(LDFLAGS) $(OBJ) $(LIBS)
	install -S 16k $@ /sbin

# clean up local files
clean:
	rm -f $(SERVER) *.o *.bak 

depend: 
	/usr/bin/mkdep "$(CC) -E $(CPPFLAGS)" *.c > .depend

# Include generated dependencies.
include .depend


