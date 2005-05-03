# Makefile for driver library

# Directories
u = /usr
i = $u/include
s = $i/sys
b = $i/ibm
m = $i/minix

# Programs, flags, etc.
CC =	exec cc
CFLAGS = -I$i
LDFLAGS = -i
LIBS = -lsys -lutils

OBJECTS = driver.o drvlib.o

all build: $(OBJECTS)	

# $(CC) -c $@ $(LDFLAGS) $(OBJ) $(LIBS)

clean:
	rm -f *.o *.bak 

install:

# Dependencies
a = $m/config.h $i/ansi.h $m/type.h $m/com.h $m/callnr.h $s/types.h \
	$m/const.h $m/syslib.h $m/utils.h \
	$i/string.h $i/limits.h $i/stddef.h $i/errno.h \
	$m/partition.h $m/u64.h

driver.o:	driver.h $a
driver.o:	$s/ioc_disk.h

drvlib.o:	driver.h $a
drvlib.o:	drvlib.h $b/partition.h

