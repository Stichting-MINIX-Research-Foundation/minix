# Makefile for AMD's DEV
DRIVER = amddev

# programs, flags, etc.
CC =	exec cc
CFLAGS = $(CPROFILE)
LDFLAGS = -i 
LIBS = -lsys 

OBJ = amddev.o

# build local binary
all build:	$(DRIVER)
$(DRIVER):	$(OBJ) 
	$(CC) -o $@ $(LDFLAGS) $(OBJ) $(LIBS)
	install -S 320k $(DRIVER)

# install with other drivers
install:	/usr/sbin/$(DRIVER)
/usr/sbin/$(DRIVER):	$(DRIVER)
	install -o root -cs $? $@

# clean up local files
clean:
	rm -f *.o *.bak $(DRIVER)

depend: 
	mkdep "$(CC) -E $(CPPFLAGS)" *.c > .depend

# Include generated dependencies.
include .depend

