# Makefile for Reincarnation Server (RS)
SERVER = rs
UTIL = service

include /etc/make.conf

# directories
u = /usr
i = $u/include
s = $i/sys
m = $i/minix
b = $i/ibm

# programs, flags, etc.
CC =	exec cc
CPPFLAGS = -I../../kernel/arch/$(ARCH)/include
CFLAGS = -I$i $(CPROFILE) $(CPPFLAGS)
LDFLAGS = -i
LIBS = -lsys 

UTIL_OBJ = service.o
OBJ = exec.o main.o manager.o table.o utility.o memory.o

# build local binary
all build:	$(SERVER) $(UTIL)
$(UTIL):	$(UTIL_OBJ)
	$(CC) -o $@ $(LDFLAGS) $(UTIL_OBJ) $(UTIL_LIBS)
$(SERVER):	$(OBJ)
	$(CC) -o $@ $(LDFLAGS) $(OBJ) $(LIBS)
	install -S 450k $@

# install with other servers
install:	/bin/$(UTIL) /usr/sbin/$(SERVER)
/bin/$(UTIL):	$(UTIL)
	install -c $? $@
/usr/sbin/$(SERVER):	$(SERVER)
	install -o root -c $? $@

# clean up local files
clean:
	rm -f $(UTIL) $(SERVER) *.o *.bak 

depend: 
	mkdep "$(CC) -E $(CPPFLAGS)" *.c > .depend

# Include generated dependencies.
include .depend

