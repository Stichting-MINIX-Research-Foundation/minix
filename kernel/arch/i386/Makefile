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
CFLAGS=-Iinclude -Wall $(CPROFILE)

build: $(HEAD) $(ARCHAR)

$(ARCHAR): $(ARCHAR)($(OBJS))
	aal cr $@ $(OBJS)
#	ar cru $(.TARGET) $(.OODATE)

depend: 
	mkdep "$(CC) -E $(CPPFLAGS)" *.c *.S > .depend

clean:
	rm -f *.a *.o *~ *.tmp *.s

# How to build it
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

$(HEAD): mpx386.o
	cp $< $@

