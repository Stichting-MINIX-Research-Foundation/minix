prefix = /usr
libdir = $(prefix)/lib/bind9

CFLAGS=-fPIC -g -I../include

all: dlz_filesystem_dynamic.so

dir.o: dir.c
	$(CC) $(CFLAGS) -c dir.c

dlz_filesystem_dynamic.so: dlz_filesystem_dynamic.c dir.o
	$(CC) $(CFLAGS) -shared -o dlz_filesystem_dynamic.so \
		dlz_filesystem_dynamic.c dir.o

clean:
	rm -f dlz_filesystem_dynamic.so *.o

install: dlz_filesystem_dynamic.so
	mkdir -p $(DESTDIR)$(libdir)
	install dlz_filesystem_dynamic.so $(DESTDIR)$(libdir)
