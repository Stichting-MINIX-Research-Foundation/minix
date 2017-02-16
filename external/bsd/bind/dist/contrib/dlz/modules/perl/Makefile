# For building the dlz_perl_driver driver we don't use
# the bind9 build structure as the aim is to provide an
# perl_driver that is separable from the bind9 source tree

CFLAGS += -fPIC -O2 -I../include
FLAGS_PERL ?= perl
LIBNAME = dlz_perl_driver.so

all: $(LIBNAME)

dlz_perl_driver.o: dlz_perl_driver.c
	$(CC) $(CFLAGS) `${FLAGS_PERL} -MExtUtils::Embed -e ccopts` -c -o dlz_perl_driver.o dlz_perl_driver.c


dlz_perl_callback_clientinfo.c: dlz_perl_callback_clientinfo.xs
	${FLAGS_PERL} `${FLAGS_PERL} -MConfig -le 'print $$Config{privlibexp}'`/ExtUtils/xsubpp -prototypes -typemap `${FLAGS_PERL} -MConfig -le 'print $$Config{privlibexp}'`/ExtUtils/typemap dlz_perl_callback_clientinfo.xs > dlz_perl_callback_clientinfo.c

dlz_perl_callback_clientinfo.o: dlz_perl_callback_clientinfo.c
	$(CC) $(CFLAGS) `${FLAGS_PERL} -MExtUtils::Embed -e ccopts` -c -o dlz_perl_callback_clientinfo.o dlz_perl_callback_clientinfo.c


dlz_perl_callback.c: dlz_perl_callback.xs
	${FLAGS_PERL} `${FLAGS_PERL} -MConfig -le 'print $$Config{privlibexp}'`/ExtUtils/xsubpp -prototypes -typemap `${FLAGS_PERL} -MConfig -le 'print $$Config{privlibexp}'`/ExtUtils/typemap dlz_perl_callback.xs > dlz_perl_callback.c

dlz_perl_callback.o: dlz_perl_callback.c
	$(CC) $(CFLAGS) `${FLAGS_PERL} -MExtUtils::Embed -e ccopts` -c -o dlz_perl_callback.o dlz_perl_callback.c


$(LIBNAME): dlz_perl_driver.o dlz_perl_callback_clientinfo.o dlz_perl_callback.o
	$(CC) $(LDFLAGS) -shared -o $(LIBNAME) dlz_perl_driver.o dlz_perl_callback_clientinfo.o dlz_perl_callback.o `${FLAGS_PERL} -MExtUtils::Embed -e ldopts`

clean:
	rm -f dlz_perl_driver.o dlz_perl_driver.so dlz_perl_callback_clientinfo.c dlz_perl_callback_clientinfo.o dlz_perl_callback.c dlz_perl_callback.o
