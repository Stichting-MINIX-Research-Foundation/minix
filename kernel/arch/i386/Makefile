# Makefile for kernel

include /etc/make.conf

ARCHAR=$(ARCH).a

# objects, excluding first-stage code, which is $(HEAD).
# the HEAD variable is passed as an argument to this Makefile
# by an upper level Makefile.

OBJS=	arch_do_vmctl.o \
	clock.o \
	do_int86.o \
	do_iopenable.o \
	do_readbios.o \
	do_sdevio.o \
	exception.o \
	i8259.o \
	klib386.o \
	memory.o \
	mpx386.o \
	protect.o \
	system.o \
	apic.o \
	apic_asm.o \
	watchdog.o

CPPFLAGS=-Iinclude
CFLAGS=$(CPPFLAGS) -Wall $(CPROFILE)

build: $(HEAD) $(ARCHAR)

$(ARCHAR): $(OBJS)
	aal cr $@ $(OBJS)

depend: 
	mkdep "$(CC) -E $(CPPFLAGS)" *.c *.S > .depend

clean:
	rm -f *.a *.o *~ *.tmp *.s

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

$(ARCHAR)(arch_do_vmctl.o): arch_do_vmctl.c
	$(CC) $(CFLAGS) -c $<

$(ARCHAR)(do_readbios.o): do_readbios.c
	$(CC) $(CFLAGS) -c $<

$(ARCHAR)(do_sdevio.o): do_sdevio.c
	$(CC) $(CFLAGS) -c $<

$(ARCHAR)(clock.o): clock.c
	$(CC) $(CFLAGS) -c $<

klib386.o: klib386.S
	$(CC) $(CFLAGS)  -E -D__ASSEMBLY__ -o $@.tmp $<
	gas2ack $@.tmp $@.s
	$(CC) $(CFLAGS) -c -o $@ $@.s

mpx386.o: mpx386.S
	$(CC) $(CFLAGS) -E -D__ASSEMBLY__ -o $@.tmp $<
	gas2ack $@.tmp $@.s
	$(CC) $(CFLAGS) -c -o $@ $@.s

apic_asm.o: apic_asm.S
	$(CC) $(CFLAGS) -E -D__ASSEMBLY__ -o $@.tmp $<
	gas2ack $@.tmp $@.s
	$(CC) $(CFLAGS) -c -o $@ $@.s

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

$(HEAD): mpx386.o
	cp $< $@

# Include generated dependencies.
include .depend
