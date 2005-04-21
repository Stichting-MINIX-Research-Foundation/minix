# Makefile for lib/i86/misc.

CFLAGS	= -O -D_MINIX -D_POSIX_SOURCE -Was-ncc
CC1	= $(CC) $(CFLAGS) -c

LIBRARY	= ../../libc.a
all:	$(LIBRARY)

OBJECTS	= \
	$(LIBRARY)(alloca.o) \
	$(LIBRARY)(get_bp.o) \
	$(LIBRARY)(getprocessor.o) \
	$(LIBRARY)(hton86.o) \
	$(LIBRARY)(io_inb.o) \
	$(LIBRARY)(io_inl.o) \
	$(LIBRARY)(io_insb.o) \
	$(LIBRARY)(io_insl.o) \
	$(LIBRARY)(io_insw.o) \
	$(LIBRARY)(io_intr.o) \
	$(LIBRARY)(io_inw.o) \
	$(LIBRARY)(io_outb.o) \
	$(LIBRARY)(io_outl.o) \
	$(LIBRARY)(io_outsb.o) \
	$(LIBRARY)(io_outsl.o) \
	$(LIBRARY)(io_outsw.o) \
	$(LIBRARY)(io_outw.o) \
	$(LIBRARY)(oneC_sum.o) \

$(LIBRARY):	$(OBJECTS)
	aal cr $@ *.o
	rm *.o

$(LIBRARY)(alloca.o):	alloca.s
	$(CC1) alloca.s

$(LIBRARY)(get_bp.o):	get_bp.s
	$(CC1) get_bp.s

$(LIBRARY)(getprocessor.o):	getprocessor.s
	$(CC1) getprocessor.s

$(LIBRARY)(hton86.o):	hton86.s
	$(CC1) hton86.s

$(LIBRARY)(io_inb.o):	io_inb.s
	$(CC1) io_inb.s

$(LIBRARY)(io_inl.o):	io_inl.s
	$(CC1) io_inl.s

$(LIBRARY)(io_insb.o):	io_insb.s
	$(CC1) io_insb.s

$(LIBRARY)(io_insl.o):	io_insl.s
	$(CC1) io_insl.s

$(LIBRARY)(io_insw.o):	io_insw.s
	$(CC1) io_insw.s

$(LIBRARY)(io_intr.o):	io_intr.s
	$(CC1) io_intr.s

$(LIBRARY)(io_inw.o):	io_inw.s
	$(CC1) io_inw.s

$(LIBRARY)(io_outb.o):	io_outb.s
	$(CC1) io_outb.s

$(LIBRARY)(io_outl.o):	io_outl.s
	$(CC1) io_outl.s

$(LIBRARY)(io_outsb.o):	io_outsb.s
	$(CC1) io_outsb.s

$(LIBRARY)(io_outsl.o):	io_outsl.s
	$(CC1) io_outsl.s

$(LIBRARY)(io_outsw.o):	io_outsw.s
	$(CC1) io_outsw.s

$(LIBRARY)(io_outw.o):	io_outw.s
	$(CC1) io_outw.s

$(LIBRARY)(oneC_sum.o):	oneC_sum.s
	$(CC1) oneC_sum.s
