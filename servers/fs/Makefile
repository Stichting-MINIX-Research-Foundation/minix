# Makefile for File System (FS)
SERVER = fs

# directories
u = /usr
i = $u/include
s = $i/sys
h = $i/minix

# programs, flags, etc.
CC =	exec cc
CFLAGS = -I$i
LDFLAGS = -i
LIBS = -lsys -lutils

OBJ =	main.o open.o read.o write.o pipe.o dmap.o \
	device.o path.o mount.o link.o super.o inode.o \
	cache.o cache2.o filedes.o stadir.o protect.o time.o \
	cmostime.o lock.o misc.o utility.o select.o table.o 

# build local binary 
all build:	$(SERVER)
$(SERVER):	$(OBJ)
	$(CC) -o $@ $(LDFLAGS) $(OBJ) $(LIBS)
	install -S 512w $@

# install with other servers
install:	/usr/sbin/servers/$(SERVER)
/usr/sbin/servers/$(SERVER):	$(SERVER)
	install -o root -cs $? $@	

# clean up local files
clean:
	rm -f $(SERVER) *.o *.bak 

# dependencies
a=	fs.h $h/config.h $s/types.h $h/const.h $h/type.h \
	$i/limits.h $i/errno.h $i/ansi.h $h/syslib.h \
	const.h type.h proto.h glo.h $s/dir.h

cache.o:	$a
cache.o:	$h/com.h
cache.o:	buf.h
cache.o:	file.h
cache.o:	fproc.h
cache.o:	super.h

cache2.o:	$a
cache2.o:	$h/com.h
cache2.o:	buf.h

device.o:	$a
device.o:	$i/fcntl.h
device.o:	$h/callnr.h
device.o:	$h/com.h
device.o:	dmap.h
device.o:	file.h
device.o:	fproc.h
device.o:	inode.h
device.o:	param.h

dmap.o:	$a
dmap.o:	$i/string.h
dmap.o:	$h/com.h
dmap.o:	$h/utils.h
dmap.o:	dmap.h

filedes.o:	$a
filedes.o:	file.h
filedes.o:	fproc.h
filedes.o:	inode.h

inode.o:	$a
inode.o:	buf.h
inode.o:	file.h
inode.o:	fproc.h
inode.o:	inode.h
inode.o:	super.h

link.o:	$a
link.o:	$s/stat.h
link.o:	$i/string.h
link.o:	$h/com.h
link.o:	$h/callnr.h
link.o:	buf.h
link.o:	file.h
link.o:	fproc.h
link.o:	inode.h
link.o:	param.h
link.o:	super.h

lock.o:	$a
lock.o:	$h/com.h
lock.o:	$i/fcntl.h
lock.o:	$i/unistd.h
lock.o:	file.h
lock.o:	fproc.h
lock.o:	inode.h
lock.o:	lock.h
lock.o:	param.h

main.o:	$a
main.o:	$i/fcntl.h
main.o:	$i/string.h
main.o:	$i/stdlib.h
main.o:	$h/ioctl.h
main.o:	$h/utils.h
main.o:	$s/ioc_memory.h
main.o:	$s/svrctl.h
main.o:	$h/callnr.h
main.o:	$h/com.h
main.o:	$h/keymap.h
main.o:	buf.h
main.o:	dmap.h
main.o:	file.h
main.o:	fproc.h
main.o:	inode.h
main.o:	param.h
main.o:	super.h

misc.o:	$a
misc.o:	$i/fcntl.h
misc.o:	$i/unistd.h
misc.o:	$h/callnr.h
misc.o:	$h/com.h
misc.o:	$s/svrctl.h
misc.o:	buf.h
misc.o:	file.h
misc.o:	fproc.h
misc.o:	inode.h
misc.o:	dmap.h
misc.o:	param.h
misc.o:	super.h

mount.o:	$a
mount.o:	$i/fcntl.h
mount.o:	$h/com.h
mount.o:	$s/stat.h
mount.o:	buf.h
mount.o:	dmap.h
mount.o:	file.h
mount.o:	fproc.h
mount.o:	inode.h
mount.o:	param.h
mount.o:	super.h

open.o:	$a
open.o:	$s/stat.h
open.o:	$i/fcntl.h
open.o:	$h/callnr.h
open.o:	$h/com.h
open.o:	buf.h
open.o:	dmap.h
open.o:	file.h
open.o:	fproc.h
open.o:	inode.h
open.o:	lock.h
open.o:	param.h
open.o:	super.h

path.o:	$a
path.o:	$i/string.h
path.o:	$h/callnr.h
path.o:	buf.h
path.o:	file.h
path.o:	fproc.h
path.o:	inode.h
path.o:	super.h

pipe.o:	$a
pipe.o:	$i/fcntl.h
pipe.o:	$i/signal.h
pipe.o:	$h/callnr.h
pipe.o:	$h/com.h
pipe.o:	dmap.h
pipe.o:	file.h
pipe.o:	fproc.h
pipe.o:	inode.h
pipe.o:	param.h

protect.o:	$a
protect.o:	$i/unistd.h
protect.o:	$h/callnr.h
protect.o:	buf.h
protect.o:	file.h
protect.o:	fproc.h
protect.o:	inode.h
protect.o:	param.h
protect.o:	super.h

read.o:	$a
read.o:	$i/fcntl.h
read.o:	$h/com.h
read.o:	buf.h
read.o:	file.h
read.o:	fproc.h
read.o:	inode.h
read.o:	param.h
read.o:	super.h

stadir.o:	$a
stadir.o:	$s/stat.h
stadir.o:	$h/com.h
stadir.o:	file.h
stadir.o:	fproc.h
stadir.o:	inode.h
stadir.o:	param.h

super.o:	$a
super.o:	$i/string.h
super.o:	buf.h
super.o:	inode.h
super.o:	super.h

table.o:	$a
table.o:	$h/callnr.h
table.o:	$h/com.h
table.o:	buf.h
table.o:	file.h
table.o:	fproc.h
table.o:	inode.h
table.o:	lock.h
table.o:	super.h

time.o:	$a
time.o:	$h/callnr.h
time.o:	$h/com.h
time.o:	file.h
time.o:	fproc.h
time.o:	inode.h
time.o:	param.h

cmostime.o: $a
cmostime.o: $i/time.h
cmostime.o: $i/ibm/cmos.h
cmostime.o: $h/callnr.h
cmostime.o: $h/com.h

utility.o:	$a
utility.o:	$h/com.h
utility.o:	$i/unistd.h
utility.o:	buf.h
utility.o:	file.h
utility.o:	fproc.h
utility.o:	inode.h
utility.o:	param.h


write.o:	$a
write.o:	$i/string.h
write.o:	buf.h
write.o:	file.h
write.o:	fproc.h
write.o:	inode.h
write.o:	super.h

