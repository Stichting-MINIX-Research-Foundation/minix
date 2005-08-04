# Makefile for Information Server (IS)
SERVER = is

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
CFLAGS = -I$i
LDFLAGS = -i
LIBS = -lsys -lsysutil 

OBJ = is.o dmp.o dmp_kernel.o dmp_pm.o dmp_fs.o 

# build local binary
all build:	$(SERVER)
$(SERVER):	$(OBJ)
	$(CC) -o $@ $(LDFLAGS) $(OBJ) $(LIBS)
#	install -S 256w $@

# install with other servers
install:	/sbin/$(SERVER)
/sbin/$(SERVER):	$(SERVER)
	install -o root -c $? $@
#	install -o root -cs $? $@

# clean up local files
clean:
	rm -f $(SERVER) *.o *.bak 

depend: 
	/usr/bin/mkdep "$(CC) -E $(CPPFLAGS)" *.c > .depend

# Include generated dependencies.
include .depend

