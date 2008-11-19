# Makefile for Data Store Server (DS)
SERVER = ds

# directories
u = /usr
i = $u/include
s = $i/sys
m = $i/minix
b = $i/ibm
k = $u/src/kernel
p = $u/src/servers/pm
f = $u/src/servers/fs

# programs, flags, etc.
CC =	exec cc
CFLAGS = -I$i $(CPROFILE)
LDFLAGS = -i
LIBS = -lsys 

OBJ = main.o store.o 

# build local binary
all build:	$(SERVER)
$(SERVER):	$(OBJ)
	$(CC) -o $@ $(LDFLAGS) $(OBJ) $(LIBS)
	install -S 16k $@

# install with other servers
install: $(SERVER)

# clean up local files
clean:
	rm -f $(SERVER) *.o *.bak 

depend: 
	mkdep "$(CC) -E $(CPPFLAGS)" *.c > .depend

# Include generated dependencies.
include .depend

