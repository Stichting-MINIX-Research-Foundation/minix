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
LIBS = -lsys -lutils 

OBJ = main.o dmp.o dmp_kernel.o dmp_pm.o dmp_fs.o diag.o kputc.o

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

dmp.o:		$a

diag.o:		$a
diag.o:		$k/type.h

dmp_kernel.o:		$a
dmp_kernel.o:		$i/timers.h $i/string.h $b/interrupt.h
dmp_kernel.o:		$k/proc.h $k/sendmask.h $k/type.h $k/const.h

dmp_pm.o:		$a
dmp_pm.o:		$p/mproc.h

dmp_fs.o:		$a
dmp_fs.o:		$f/fproc.h
dmp_fs.o:		$f/dmap.h

kputc.o:	$a

