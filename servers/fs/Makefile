# Makefile for File System (FS)
SERVER = fs

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

OBJ =	main.o open.o read.o write.o pipe.o dmap.o \
	device.o path.o mount.o link.o super.o inode.o \
	cache.o cache2.o filedes.o stadir.o protect.o time.o \
	lock.o misc.o utility.o select.o timers.o table.o \
	cdprobe.o

# build local binary 
all build:	$(SERVER)
$(SERVER):	$(OBJ)
	$(CC) -o $@ $(LDFLAGS) $(OBJ) $(LIBS)
	install -S 512w $@

# install with other servers
install:	/usr/sbin/$(SERVER)
/usr/sbin/$(SERVER):	$(SERVER)
	install -o root -cs $? $@	

# clean up local files
clean:
	rm -f $(SERVER) *.o *.bak 

depend: 
	/usr/bin/mkdep "$(CC) -E $(CPPFLAGS)" *.c > .depend

# Include generated dependencies.
include .depend

