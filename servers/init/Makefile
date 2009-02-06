# Makefile for the init program (INIT)
SERVER = init

# directories
u = /usr
i = $u/include
s = $i/sys
h = $i/minix
k = $u/src/kernel

# programs, flags, etc.
CC =	exec cc
CFLAGS = -I$i -O -D_MINIX -D_POSIX_SOURCE
LDFLAGS = -i

OBJ = 	init.o

# build local binary
all build:	$(SERVER)
$(SERVER):	$(OBJ)
	$(CC) $(CFLAGS) -o $@ $(LDFLAGS) $(OBJ) -lsys
	install -S 64k $@

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

