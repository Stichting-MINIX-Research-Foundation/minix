# Makefile for shutdown / halt / reboot.

CFLAGS=$(OPT) -D_MINIX -D_POSIX_SOURCE
LDFLAGS=-i

PROGRAMS=	shutdown halt tinyhalt
MANUALS=	shutdown.8 halt.8 reboot.2 reboot.8

all:	$(PROGRAMS)

shutdown:	shutdown.o sh_wall.o log.o
	$(CC) $(LDFLAGS) -o shutdown shutdown.o sh_wall.o log.o
	install -S 4kw $@

halt:	halt.o log.o
	$(CC) $(LDFLAGS) -o halt halt.o log.o
	install -S 4kw $@

tinyhalt:	tinyhalt.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $?
	install -S 4kw $@

install:	/usr/bin/halt /usr/bin/reboot /usr/bin/shutdown \
		/bin/halt /bin/reboot

/usr/bin/halt:	halt
	install -cs -o root -g operator -m 744 $? $@

/usr/bin/reboot:	/usr/bin/halt
	install -l $? $@

/usr/bin/shutdown:	shutdown
	install -cs -o root -g operator -m 4754 $? $@

/bin/halt:	tinyhalt
	install -cs -o root -g operator -m 744 $? $@

/bin/reboot:	/bin/halt
	install -l $? $@

clean:
	rm -f *.o a.out core $(PROGRAMS)
