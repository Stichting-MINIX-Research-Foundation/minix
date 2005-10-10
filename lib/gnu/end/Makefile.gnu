CC=gcc
AR=gar
AS=gas

VPATH=$(SRCDIR)/gnu/end

LIBRARY=../../end.a
OBJECTS=gnu_end.o

all: $(LIBRARY)

$(LIBRARY): $(OBJECTS)
	$(AR) cr $@ *.o

gnu_end.o: gnu_end.s

