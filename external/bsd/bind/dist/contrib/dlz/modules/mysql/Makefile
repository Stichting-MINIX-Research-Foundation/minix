prefix = /usr
libdir = $(prefix)/lib/bind9

CFLAGS=-fPIC -g -I../include
MYSQL_LIBS=-lmysqlclient

all: dlz_mysql_dynamic.so

dlz_dbi.o: ../common/dlz_dbi.c
	$(CC) $(CFLAGS) -c ../common/dlz_dbi.c

dlz_mysql_dynamic.so: dlz_mysql_dynamic.c dlz_dbi.o
	$(CC) $(CFLAGS) -shared -o dlz_mysql_dynamic.so \
		dlz_mysql_dynamic.c dlz_dbi.o $(MYSQL_LIBS)

clean:
	rm -f dlz_mysql_dynamic.so *.o

install: dlz_mysql_dynamic.so
	mkdir -p $(DESTDIR)$(libdir)
	install dlz_mysql_dynamic.so $(DESTDIR)$(libdir)
