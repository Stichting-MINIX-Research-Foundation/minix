# Makefile for log driver
DRIVER = log

# directories
u = /usr
i = $u/include
s = $i/sys
m = $i/minix
d = ..

# programs, flags, etc.
MAKE = exec make
CC =	exec cc
CFLAGS = -I$i $(CPROFILE)
LDFLAGS = -i -L../libdriver
LIBS = -lsysutil -ldriver  -lsys

OBJ = log.o diag.o kputc.o

# build local binary
all build:	$(DRIVER)
$(DRIVER):	$(OBJ)
	$(CC) -o $@ $(LDFLAGS) $(OBJ) $(LIBS)
	install -S 16kb $(DRIVER)

# install with other drivers
install:	$(DRIVER)
	install -o root -cs $? /sbin/$(DRIVER) 

# clean up local files
clean:
	rm -f $(DRIVER) *.o *.bak 


depend: 
	mkdep "$(CC) -E $(CPPFLAGS)" *.c ../libdriver/*.c > .depend

# Include generated dependencies.
include .depend

