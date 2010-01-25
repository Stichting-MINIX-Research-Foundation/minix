# Makefile for HGFS library
LIBNAME=libhgfs.a

OBJ=backdoor.o attr.o channel.o dir.o error.o file.o \
	link.o misc.o path.o rpc.o time.o

AR=ar
GAS2ACK=gas2ack

all build: $(LIBNAME)

$(LIBNAME): $(OBJ)
	$(AR) cr $@ $(OBJ)

backdoor.o: backdoor.S
	$(GAS2ACK) $< $@.s
	$(CC) $(CFLAGS) -c -o $@ $@.s

clean distclean:
	rm -f $(LIBNAME) $(OBJ) *.o.s

depend:
	mkdep "$(CC) -E $(CPPFLAGS)" *.c > .depend

# Include generated dependencies.
include .depend
