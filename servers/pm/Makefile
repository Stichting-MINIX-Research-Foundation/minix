# Makefile for Process Manager (PM)
SERVER = pm

include /etc/make.conf

# directories
u = /usr
i = $u/include
s = $i/sys
h = $i/minix
k = $u/src/kernel

# programs, flags, etc.
CC =	exec cc
CPPFLAGS=-I../../kernel/arch/$(ARCH)/include -I$i 
CFLAGS = $(CPROFILE) $(CPPFLAGS)
LDFLAGS = -i

OBJ = 	main.o forkexit.o break.o exec.o time.o timers.o \
	signal.o alloc.o utility.o table.o trace.o getset.o misc.o \
	profile.o

# build local binary
all build:	$(SERVER)
$(SERVER):	$(OBJ)
	$(CC) -o $@ $(LDFLAGS) $(OBJ) -lsysutil  -lsys -ltimers
	install -S 8k $@

# install with other servers
install:	/usr/sbin/$(SERVER)
/usr/sbin/$(SERVER):	$(SERVER)
	install -o root -cs $? $@

# clean up local files
clean:
	rm -f $(SERVER) *.o *.bak 

depend: 
	mkdep "$(CC) -E $(CPPFLAGS)" *.c > .depend

# Include generated dependencies.
include .depend

