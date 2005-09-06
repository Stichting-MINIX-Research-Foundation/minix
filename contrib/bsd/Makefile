
NVI=nvi-1.79
NVIWORK=$(NVI)/minix

all:
	-mkdir $(NVIWORK)
	cd $(NVIWORK) && CFLAGS="-D_MINIX=1 -D_POSIX_SOURCE=1" ../build/configure.minix --prefix=/usr/local --host=minix && make $@

install: all
	cd $(NVIWORK) && sh ../build/install.minix

clean:
	if [ -f $(NVIWORK)/Makefile ]; then cd $(NVIWORK) && make clean; fi

