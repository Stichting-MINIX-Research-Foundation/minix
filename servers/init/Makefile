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
	$(CC) $(CFLAGS) -o $@ $(LDFLAGS) $(OBJ) -lutils
	install -S 192w $@

# install with other servers
install:	/usr/sbin/servers/$(SERVER)
/usr/sbin/servers/$(SERVER):	$(SERVER)
	install -o root -cs $? $@

# clean up local files
clean:
	rm -f $(SERVER) *.o *.bak 

# dependencies
a =	$s/types.h $s/wait.h $s/stat.h $s/svrctl.h \
	$i/ttyent.h $i/fcntl.h $i/unistd.h $i/time.h $i/stdlib.h \
	$i/limits.h $i/errno.h $i/signal.h $i/string.h $i/utmp.h

init.o:	$a
