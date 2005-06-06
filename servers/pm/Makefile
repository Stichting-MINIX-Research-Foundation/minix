# Makefile for Process Manager (PM)
SERVER = pm

# directories
u = /usr
i = $u/include
s = $i/sys
h = $i/minix
k = $u/src/kernel

# programs, flags, etc.
CC =	exec cc
CFLAGS = -I$i
LDFLAGS = -i

OBJ = 	main.o forkexit.o break.o exec.o dmp.o time.o \
	signal.o alloc.o utility.o table.o trace.o getset.o misc.o

# build local binary
all build:	$(SERVER)
$(SERVER):	$(OBJ)
	$(CC) -o $@ $(LDFLAGS) $(OBJ) -lsys -lutils
	install -S 256w $@

# install with other servers
install:	/usr/sbin/servers/$(SERVER)
/usr/sbin/servers/$(SERVER):	$(SERVER)
	install -o root -cs $? $@

# clean up local files
clean:
	rm -f $(SERVER) *.o *.bak 

# dependencies
a =	pm.h $h/config.h $s/types.h $h/const.h $h/type.h \
	$i/ansi.h $i/fcntl.h $i/unistd.h $h/syslib.h $h/utils.h \
	$i/limits.h $i/errno.h const.h type.h proto.h glo.h

alloc.o:	$a
alloc.o:	$i/signal.h
alloc.o:	$h/com.h
alloc.o:	$h/callnr.h
alloc.o:	mproc.h
alloc.o:	$k/type.h
alloc.o:	$k/const.h

break.o:	$a
break.o:	$i/signal.h
break.o:	mproc.h
break.o:	param.h

exec.o:	$a
exec.o:	$s/stat.h
exec.o:	$h/callnr.h
exec.o:	$h/com.h
exec.o:	$i/a.out.h
exec.o:	$i/signal.h
exec.o:	$i/string.h
exec.o:	mproc.h
exec.o:	param.h

forkexit.o:	$a
forkexit.o:	$s/wait.h
forkexit.o:	$h/callnr.h
forkexit.o:	$h/com.h
forkexit.o:	$h/utils.h
forkexit.o:	$i/signal.h
forkexit.o:	mproc.h
forkexit.o:	param.h

getset.o:	$a
getset.o:	$h/callnr.h
getset.o:	$i/signal.h
getset.o:	mproc.h
getset.o:	param.h

main.o:	$a
main.o:	$h/callnr.h
main.o:	$h/com.h
main.o:	$i/signal.h
main.o:	$i/fcntl.h
main.o:	$h/ioctl.h
main.o:	$s/ioc_memory.h
main.o:	$h/utils.h
main.o:	mproc.h
main.o:	param.h

misc.o:	$a
misc.o:	$h/callnr.h
misc.o:	$h/utils.h
misc.o:	$i/signal.h
misc.o:	$h/ioctl.h
misc.o:	$s/svrctl.h
misc.o:	mproc.h
misc.o:	param.h

dmp.o:	$a

signal.o:	$a
signal.o:	$s/stat.h
signal.o:	$h/callnr.h
signal.o:	$h/utils.h
signal.o:	$h/com.h
signal.o:	$i/signal.h
signal.o:	$s/sigcontext.h
signal.o:	$i/string.h
signal.o:	mproc.h
signal.o:	param.h

table.o:	$a
table.o:	$h/callnr.h
table.o:	$i/signal.h
table.o:	mproc.h
table.o:	param.h

trace.o:	$a
trace.o:	$h/com.h
trace.o:	$s/ptrace.h
trace.o:	$i/signal.h
trace.o:	mproc.h
trace.o:	param.h

utility.o:	$a
utility.o:	$s/stat.h
utility.o:	$h/callnr.h
utility.o:	$h/com.h
utility.o:	$i/fcntl.h
utility.o:	$i/signal.h
utility.o:	mproc.h
utility.o:	$i/timers.h
utility.o:	$i/string.h
utility.o:	$k/const.h
utility.o:	$k/type.h
utility.o:	$k/proc.h

