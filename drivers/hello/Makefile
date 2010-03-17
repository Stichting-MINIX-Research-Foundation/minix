#
# Makefile for the hello driver.
#
DRIVER = hello

#
# Directories.
#
u = /usr
i = $u/include
s = $i/sys
m = $i/minix
b = $i/ibm
d = ..

#
# Build Programs, Flags and Variables.
#
CC      = exec cc
CFLAGS  = -I$i $(CPROFILE)
LDFLAGS = -i -L../libdriver
LIBS    = -ldriver -lsys
OBJ     = hello.o

# build local binary
all build:      $(DRIVER)
$(DRIVER):      $(OBJ)
	$(CC) -o $@ $(LDFLAGS) $(OBJ) $(LIBS)
	install -S 128k $(DRIVER)

# install with other drivers
install:        /usr/sbin/$(DRIVER)
/usr/sbin/$(DRIVER):    $(DRIVER)
	install -o root -cs $? $@

# clean up local files
clean:
	rm -f *.o $(DRIVER)

depend:
	mkdep "$(CC) -E $(CPPFLAGS)" *.c > .depend

# Include generated dependencies.
include .depend