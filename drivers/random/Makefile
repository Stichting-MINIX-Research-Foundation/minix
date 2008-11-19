# Makefile for random driver (RANDOM)
DRIVER = random

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
CFLAGS = -I$i $(CPROFILE)
LDFLAGS = -i -L../libdriver_asyn
LIBS =  -ldriver -lsys 
LIB_DEPS=../libdriver_asyn/libdriver.a

OBJ = main.o random.o sha2.o aes/rijndael_api.o aes/rijndael_alg.o

# build local binary
all build:	$(DRIVER)
$(DRIVER):	$(OBJ) $(LIB_DEPS)
	$(CC) -o $@ $(LDFLAGS) $(OBJ) $(LIBS)
	install -S 8k $(DRIVER)

aes/rijndael_api.o:
	$(CC) -c -o $@ aes/rijndael_api.c

aes/rijndael_alg.o:
	$(CC) -c -o $@ aes/rijndael_alg.c

# install with other drivers
install:	/usr/sbin/$(DRIVER)
/usr/sbin/$(DRIVER):	$(DRIVER)
	install -o root -cs $? $@

# clean up local files
clean:
	rm -f $(DRIVER) *.o *.bak aes/*.o


depend: 
	mkdep "$(CC) -E $(CPPFLAGS)" *.c ../libdriver/*.c aes/*.c > .depend

# Include generated dependencies.
include .depend

