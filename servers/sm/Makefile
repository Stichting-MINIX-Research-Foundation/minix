# Makefile for System Process Manager (SM)
SERVER = sm
UTIL = service

# directories
u = /usr
i = $u/include
s = $i/sys
m = $i/minix
b = $i/ibm

# programs, flags, etc.
CC =	exec cc
CFLAGS = -I$i
LDFLAGS = -i
UTIL_LIBS = -lsys 
LIBS = -lsys -lsysutil 

UTIL_OBJ = service.o
OBJ = sm.o manager.o 

# build local binary
all build:	$(SERVER) $(UTIL)
$(UTIL):	$(UTIL_OBJ)
	$(CC) -o $@ $(LDFLAGS) $(UTIL_OBJ) $(UTIL_LIBS)
$(SERVER):	$(OBJ)
	$(CC) -o $@ $(LDFLAGS) $(OBJ) $(LIBS)

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
	/usr/bin/mkdep "$(CC) -E $(CPPFLAGS)" *.c > .depend

# Include generated dependencies.
include .depend

