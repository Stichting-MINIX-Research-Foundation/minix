# Makefile for kernel

include /etc/make.conf

ARCHAR=$(ARCH).a

# objects, excluding first-stage code, which is $(HEAD).
# the HEAD variable is passed as an argument to this Makefile
# by an upper level Makefile.

OBJS=$(ARCHAR)(exception.o) \
	$(ARCHAR)(i8259.o) \
	$(ARCHAR)(memory.o) \
	$(ARCHAR)(protect.o) \
	$(ARCHAR)(system.o) \
	$(ARCHAR)(clock.o) \
	$(ARCHAR)(klib386.o) \
	$(ARCHAR)(do_readbios.o) \
	$(ARCHAR)(do_int86.o) \
	$(ARCHAR)(do_sdevio.o) \
	$(ARCHAR)(do_iopenable.o)

CPPFLAGS=-Iinclude
CFLAGS=$(CPPFLAGS) -Wall

build: $(HEAD) $(ARCHAR)

$(ARCHAR): $(OBJS)
	aal cr $@ *.o

depend: 
	mkdep "$(CC) -E $(CPPFLAGS)" *.c *.s > .depend

clean:
	rm -f *.a *.o *~

# How to build it
$(ARCHAR)(exception.o): exception.c
	$(CC) $(CFLAGS) -c $<

$(ARCHAR)(i8259.o): i8259.c
	$(CC) $(CFLAGS) -c $<

$(ARCHAR)(memory.o): memory.c
	$(CC) $(CFLAGS) -c $<

$(ARCHAR)(protect.o): protect.c
	$(CC) $(CFLAGS) -c $<

$(ARCHAR)(system.o): system.c
	$(CC) $(CFLAGS) -c $<

$(ARCHAR)(do_int86.o): do_int86.c
	$(CC) $(CFLAGS) -c $<

$(ARCHAR)(do_iopenable.o): do_iopenable.c
	$(CC) $(CFLAGS) -c $<

$(ARCHAR)(do_readbios.o): do_readbios.c
	$(CC) $(CFLAGS) -c $<

$(ARCHAR)(do_sdevio.o): do_sdevio.c
	$(CC) $(CFLAGS) -c $<

$(ARCHAR)(clock.o): clock.c
	$(CC) $(CFLAGS) -c $<

$(ARCHAR)(klib386.o): klib386.s
	$(CC) $(CFLAGS) -c $<

.s.o:
	$(CC) $(CFLAGS) -c -o $@ $<

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

$(HEAD): mpx386.o
	cp $< $@

# Include generated dependencies.
include .depend
