# Makefile for IPC server
SERVER = ipc

include /etc/make.conf

OBJ = main.o utility.o shm.o sem.o

CPPFLAGS=
CFLAGS = $(CPROFILE) $(CPPFLAGS)

# build local binary

all build:	$(SERVER)

install: $(SERVER)
	install -o root -c $< /usr/sbin/$(SERVER) 

$(SERVER):	$(OBJ)
	$(CC) -o $@ $(LDFLAGS) $(OBJ) -lsys  

# clean up local files
clean:
	rm -f $(SERVER) *.o *.bak 

depend: 
	mkdep "$(CC) -E $(CPPFLAGS)" *.c > .depend

# Include generated dependencies.
include .depend

