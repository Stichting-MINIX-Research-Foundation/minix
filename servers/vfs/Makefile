# Makefile for File System (FS)
SERVER = vfs

# directories
u = /usr
i = $u/include
s = $i/sys
h = $i/minix

# programs, flags, etc.
CC =	exec cc
CFLAGS = -I$i $(EXTRA_OPTS) $(CPROFILE)
LDFLAGS = -i
LIBS = -lsys -ltimers

OBJ =	main.o open.o read.o write.o pipe.o dmap.o \
	path.o device.o mount.o link.o exec.o \
	filedes.o stadir.o protect.o time.o \
	lock.o misc.o utility.o select.o timers.o table.o \
	vnode.o vmnt.o request.o mmap.o fscall.o

# build local binary 
install all build:	$(SERVER)

$(SERVER):	$(OBJ)
	$(CC) -o $@ $(LDFLAGS) $(OBJ) $(LIBS)
	install -S 16k $@

# clean up local files
clean:
	rm -f $(SERVER) *.o *.bak 

depend: 
	mkdep "$(CC) -E $(CPPFLAGS)" *.c > .depend

# Include generated dependencies.
include .depend
