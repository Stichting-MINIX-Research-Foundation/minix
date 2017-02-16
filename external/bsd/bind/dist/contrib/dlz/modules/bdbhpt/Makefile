prefix = /usr
libdir = $(prefix)/lib/bind9

CFLAGS=-fPIC -g -I../include
BDB_LIBS=-ldb

all: dlz_bdbhpt_dynamic.so

dlz_bdbhpt_dynamic.so: dlz_bdbhpt_dynamic.c
	$(CC) $(CFLAGS) -shared -o dlz_bdbhpt_dynamic.so \
		dlz_bdbhpt_dynamic.c $(BDB_LIBS)

clean:
	rm -f dlz_bdbhpt_dynamic.so

install: dlz_bdbhpt_dynamic.so
	mkdir -p $(DESTDIR)$(libdir)
	install dlz_bdbhpt_dynamic.so $(DESTDIR)$(libdir)
