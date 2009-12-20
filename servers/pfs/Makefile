# Makefile for Pipe File System (PFS)
SERVER = pfs
NR_BUFS=256 
BS=4096

# directories
u = /usr
i = $u/include
s = $i/sys
h = $i/minix

# programs, flags, etc.
CC =	exec cc
CFLAGS = -I$i $(EXTRA_OPTS) $(CPROFILE) -DNR_BUFS=$(NR_BUFS)
LDFLAGS = -i
LIBS =  -lsys 

OBJ =	 open.o table.o inode.o main.o super.o link.o \
	 buffer.o read.o misc.o utility.o stadir.o

# build local binary 
install all build:	$(SERVER)
$(SERVER):	$(OBJ)
	$(CC) -o $@ $(LDFLAGS) $(OBJ) $(LIBS)

# clean up local files
clean:
	rm -f $(SERVER) *.o *.bak 

depend: 
	mkdep "$(CC) -E $(CPPFLAGS)" *.c > .depend

# Include generated dependencies.
include .depend


