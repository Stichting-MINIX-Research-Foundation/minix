prefix = /usr
libdir = $(prefix)/lib/bind9

CFLAGS=-fPIC -g -I../include

all: dlz_wildcard_dynamic.so

dlz_dbi.o: ../common/dlz_dbi.c
	$(CC) $(CFLAGS) -c ../common/dlz_dbi.c

dlz_wildcard_dynamic.so: dlz_wildcard_dynamic.c dlz_dbi.o
	$(CC) $(CFLAGS) -shared -o dlz_wildcard_dynamic.so \
		dlz_wildcard_dynamic.c dlz_dbi.o

clean:
	rm -f dlz_wildcard_dynamic.so *.o

install: dlz_wildcard_dynamic.so
	mkdir -p $(DESTDIR)$(libdir)
	install dlz_wildcard_dynamic.so $(DESTDIR)$(libdir)
