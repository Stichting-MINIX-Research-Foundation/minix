prefix = /usr
libdir = $(prefix)/lib/bind9

CFLAGS=-fPIC -g -I../include
LDAP_LIBS=-lldap

all: dlz_ldap_dynamic.so

dlz_dbi.o: ../common/dlz_dbi.c
	$(CC) $(CFLAGS) -c ../common/dlz_dbi.c

dlz_ldap_dynamic.so: dlz_ldap_dynamic.c dlz_dbi.o
	$(CC) $(CFLAGS) -shared -o dlz_ldap_dynamic.so \
		dlz_ldap_dynamic.c dlz_dbi.o $(LDAP_LIBS)

clean:
	rm -f dlz_ldap_dynamic.so *.o

install: dlz_ldap_dynamic.so
	mkdir -p $(DESTDIR)$(libdir)
	install dlz_ldap_dynamic.so $(DESTDIR)$(libdir)
