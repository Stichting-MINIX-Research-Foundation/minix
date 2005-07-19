# Makefile for Process Manager (PM)
SERVER = pm

# directories
u = /usr
i = $u/include
s = $i/sys
h = $i/minix
k = $u/src/kernel

# programs, flags, etc.
CC =	exec cc
CFLAGS = -I$i
LDFLAGS = -i

OBJ = 	main.o forkexit.o break.o exec.o time.o timers.o \
	signal.o alloc.o utility.o table.o trace.o getset.o misc.o

# build local binary
all build:	$(SERVER)
$(SERVER):	$(OBJ)
	$(CC) -o $@ $(LDFLAGS) $(OBJ) -lsys -lsysutil -ltimers
	install -S 256w $@

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

