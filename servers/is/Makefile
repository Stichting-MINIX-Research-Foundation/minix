# Makefile for Information Server (IS)
SERVER = is

include /etc/make.conf

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
CPPFLAGS=-I../../kernel/arch/$(ARCH)/include -I$i 
CFLAGS = $(CPROFILE) $(CPPFLAGS)
LDFLAGS = -i
LIBS = -lsys 

OBJ = main.o dmp.o dmp_kernel.o dmp_pm.o dmp_fs.o dmp_rs.o dmp_ds.o dmp_vm.o

# build local binary
all build:	$(SERVER)
$(SERVER):	$(OBJ)
	$(CC) -o $@ $(LDFLAGS) $(OBJ) $(LIBS)
#	install -S 256w $@

# install with other servers
install:	$(SERVER)
	install -o root -c $< /sbin/$(SERVER) 

# clean up local files
clean:
	rm -f $(SERVER) *.o *.bak 

depend: 
	mkdep "$(CC) -E $(CPPFLAGS)" *.c > .depend

# Include generated dependencies.
include .depend

