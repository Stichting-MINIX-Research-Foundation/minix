prefix = /usr
libdir = $(prefix)/lib/bind9

CFLAGS=-fPIC -g -I../include
SQLITE3_LIBS=-lsqlite3

all: dlz_sqlite3_dynamic.so

dlz_dbi.o: ../common/dlz_dbi.c
	$(CC) $(CFLAGS) -c ../common/dlz_dbi.c

dlz_sqlite3_dynamic.so: dlz_sqlite3_dynamic.c dlz_dbi.o
	$(CC) $(CFLAGS) -shared -o dlz_sqlite3_dynamic.so \
		dlz_sqlite3_dynamic.c dlz_dbi.o $(SQLITE3_LIBS)

clean:
	rm -f dlz_sqlite3_dynamic.so *.o

install: dlz_sqlite3_dynamic.so
	mkdir -p $(DESTDIR)$(libdir)
	install dlz_sqlite3_dynamic.so $(DESTDIR)$(libdir)
