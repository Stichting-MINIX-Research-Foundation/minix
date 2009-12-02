# Makefile for filter driver
DRIVER = filter

# programs, flags, etc.
CC = cc
CFLAGS = -DDEBUG=1 -DDEBUG2=0
LDFLAGS = 
LIBS = -lsys

OBJ = main.o sum.o driver.o util.o optset.o crc.o md5.o

# build local binary
all build:	$(DRIVER)
$(DRIVER):	$(OBJ) 
	$(CC) -o $@ $(LDFLAGS) $(OBJ) $(LIBS)

# install with other drivers
install:	/usr/sbin/$(DRIVER)
/usr/sbin/$(DRIVER):	$(DRIVER)
	install -o root -c $? $@

# clean up local files
clean:
	rm -f *.o *.bak $(DRIVER)

depend: 
	mkdep "$(CC) -E $(CPPFLAGS)" *.c > .depend

# Include generated dependencies.
include .depend

