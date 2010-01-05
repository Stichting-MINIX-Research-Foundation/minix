# Makefile for memory driver (MEMORY)
DRIVER = memory

# directories
u = /usr
i = $u/include
s = $i/sys
m = $i/minix
b = $i/ibm
d = ..

# programs, flags, etc.
MAKE = exec make
CC =	exec cc
CFLAGS = -I$i
LDFLAGS = -i -L../libdriver
LIBS =  -ldriver -lsys  

# imgrd_s.s is the ACK assembler version of the ramdisk. For more portability,
# use the C version imgrd.c. However, the C compiler takes too much memory
# compiling imgrd.c.
IMGRD=imgrd_s.o
#IMGRD=imgrd.c

OBJ = memory.o $(IMGRD)

# build local binary
all build:	$(DRIVER)

$(DRIVER):	ramdisk_image $(OBJ)
	$(CC) -o $@ $(LDFLAGS) $(OBJ) $(LIBS)
	install -S 8k $(DRIVER)

imgrd.o:        ramdisk/image.c

imgrd_s.o:      ramdisk/image.s
	TMPDIR=/usr/tmp $(CC) -T /usr/tmp -c imgrd_s.s

ramdisk_image:
	cd ramdisk && make

# install with other drivers
install: /usr/sbin/$(DRIVER)
/usr/sbin/$(DRIVER):	$(DRIVER)
	install -o root -cs $? $@

# clean up local files
clean:
	rm -f $(DRIVER) *.o *.bak 
	cd ramdisk && make clean

depend: 
	mkdep "$(CC) -E $(CPPFLAGS)" memory.c ../libdriver/*.c > .depend
	cd ramdisk && make depend

# Include generated dependencies.
include .depend

