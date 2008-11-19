# Makefile for driver library

# Directories
u = /usr
i = $u/include
s = $i/sys
b = $i/ibm
m = $i/minix

# Programs, flags, etc.
CC = exec cc
CFLAGS = -I$i $(CPROFILE)
LDFLAGS = -i
LIBS = -lsys
LIB = libdriver.a

OBJECTS = driver.o drvlib.o mq.o

all build install: $(LIB)

$(LIB): $(OBJECTS)	
	ar rc $(LIB) $(OBJECTS)

# $(CC) -c $@ $(LDFLAGS) $(OBJ) $(LIBS)

clean:
	rm -f *.o *.bak *.a

depend: 
	mkdep "$(CC) -E $(CPPFLAGS)" *.c > .depend

# Include generated dependencies.
include .depend

