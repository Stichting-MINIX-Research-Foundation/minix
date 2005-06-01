# Makefile for Information Server (IS)
SERVER = is

# directories
u = /usr
i = $u/include
s = $i/sys
m = $i/minix
b = $i/ibm
k = $u/src/kernel

# programs, flags, etc.
CC =	exec cc
CFLAGS = -I$i
LDFLAGS = -i
LIBS = -lsys -lutils 

OBJ = main.o dmp.o diag.o kputc.o

# build local binary
all build:	$(SERVER)
$(SERVER):	$(OBJ)
	$(CC) -o $@ $(LDFLAGS) $(OBJ) $(LIBS)
	install -S 256w $@

# install with other servers
install:	/usr/sbin/servers/$(SERVER)
/usr/sbin/servers/$(SERVER):	$(SERVER)
	install -o root -cs $? $@

# clean up local files
clean:
	rm -f $(SERVER) *.o *.bak 

# dependencies
a = 	is.h proto.h glo.h \
	$i/unistd.h $i/stdlib.h $i/stdio.h $i/ansi.h $i/limits.h $i/errno.h \
	$s/types.h $m/config.h $m/type.h $m/const.h $m/com.h $m/keymap.h \
	$m/syslib.h $s/types.h \
	$m/utils.h $m/devio.h

main.o:		$a

diag.o:		$a
diag.o:		$k/type.h

dmp.o:		$a
dmp.o:		$i/timers.h $i/string.h $b/interrupt.h
dmp.o:		$k/proc.h $k/sendmask.h $k/type.h $k/const.h

kputc.o:	$a

