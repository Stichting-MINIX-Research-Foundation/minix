# Makefile for aal

CC=exec cc
CFLAGS=-I. -wo -DAAL -DSTB -DNDEBUG -DDISTRIBUTION -D_POSIX_SOURCE -D_MINIX
LDFLAGS=-i

all:	aal

OFILES=	archiver.o \
	print.o \
	rd.o \
	rd_arhdr.o \
	rd_unsig2.o \
	sprint.o \
	wr_arhdr.o \
	wr_bytes.o \
	wr_int2.o \
	wr_long.o \
	wr_ranlib.o \
	format.o \
	rd_bytes.o \
	system.o \
	write.o \
	long2str.o

aal: $(OFILES)
	$(CC) $(LDFLAGS) -o aal $(OFILES)
	install -S 64k $@

install:	/usr/bin/aal /usr/bin/ar

/usr/bin/aal:	aal
	install -cs -o bin aal $@

/usr/bin/ar:	/usr/bin/aal
	install -l $? $@

archiver.o:
print.o:
rd.o:
rd_arhdr.o:
rd_unsig2.o:
sprint.o:
wr_arhdr.o:
wr_bytes.o:
wr_int2.o:
wr_long.o:
wr_ranlib.o:
format.o:
rd_bytes.o:
system.o:
write.o:
long2str.o:

clean:	
	rm -f *.o core *.bak aal
