# Makefile for the process status utility.
#
u=/usr
CC=	exec cc
CFLAGS=	-O -D_MINIX -D_POSIX_SOURCE
MAKE=	exec make -$(MAKEFLAGS)

all: ps

# process status utility
ps:	ps.c /usr/include/minix/config.h /usr/include/minix/const.h \
		/usr/src/kernel/const.h /usr/src/kernel/type.h \
		/usr/src/kernel/proc.h /usr/src/servers/pm/mproc.h \
		/usr/src/servers/fs/fproc.h /usr/src/servers/fs/const.h
	$(CC) -i $(CFLAGS) -o $@ ps.c
	install -S 32kw $@
install:	/usr/bin/ps
/usr/bin/ps:	ps
	install -cs -o bin -g kmem -m 2755 $? $@



# clean up compile results
clean:
	rm -f *.bak ps 
