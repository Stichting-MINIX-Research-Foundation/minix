# Makefile for patch

CFLAGS= -O -DVOIDSIG -wo -D_MINIX -D_POSIX_SOURCE -DSMALL

OBJ = patch.o pch.o inp.o util.o version.o

all:	patch

patch: $(OBJ)
	cc -i -o patch $(OBJ)
	install -S 32kw patch

install:	/usr/bin/patch

/usr/bin/patch:	patch
	install -cs -o bin patch $@

patch.o:	config.h common.h patch.c inp.h pch.h util.h version.h
pch.o:		config.h common.h pch.c pch.h util.h
inp.o:		config.h common.h inp.c inp.h util.h
util.o:		config.h common.h util.c util.h
version.o:	config.h common.h version.c version.h patchlevel.h util.h


clean:	
	rm -f *.bak *.o *.s core patch
