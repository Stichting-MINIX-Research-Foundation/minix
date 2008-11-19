# Makefile for readclock 'driver'
DRIVER = readclock.drv

# programs, flags, etc.
MAKE = exec make
CC =	exec cc
CFLAGS=-D_MINIX=1 -D_POSIX_SOURCE=1 -D_SYSTEM=1
LDFLAGS = -i
LIBS =  -lsys

OBJ = readclock.o 

# build local binary
all build:	$(DRIVER)
$(DRIVER):	$(OBJ)
	$(CC) -o $@ $(LDFLAGS) $(OBJ) $(LIBS)
	install -S 8k $(DRIVER)

# install with other drivers
install:	/sbin/$(DRIVER)
/sbin/$(DRIVER):	$(DRIVER)
	install -o root -cs $? $@

# clean up local files
clean:
	rm -f $(DRIVER) *.o *.bak 

depend: 
	mkdep "$(CC) -E $(CPPFLAGS)" *.c > .depend

# Include generated dependencies.
include .depend

