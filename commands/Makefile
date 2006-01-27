# Makefile for commands.

MAKE	= exec make -$(MAKEFLAGS)
FLEX=flex-2.5.4
GZIP=gzip-1.2.4
BZIP2=bzip2-1.0.3
NVI=nvi-1.79
NVIWORK=$(NVI)/minix

SMALLPROGRAMS=`arch` aal advent ash autil awk bc byacc cawf cron de dhcpd dis88 elle elvis ftp ftpd ftpd200 httpd ibm indent m4 make mdb mined patch pax ps reboot rlogind scripts sh simple talk talkd telnet telnetd urlget yap zmodem

usage:
	@echo "Usage: make all          # Compile all commands" >&2
	@echo "       make install      # Install the result (run as bin!)" >&2
	@echo "       make clean        # Delete .o files and other junk" >&2
	@echo "       make big          # Compile all big commands" >&2
	@echo "       make biginstall   # Install all big commands" >&2
	@echo "       make small        # Install all small commands" >&2
	@echo "       make smallinstall # Install all small commands" >&2
	@echo " "
	@echo "big compiles the commands the require large compiler sizes."
	@echo "small compiles the rest. all compiles all."
	@false

all: small big

install: biginstall smallinstall

small::
	-mkdir $(NVIWORK)
	cd $(GZIP) && CC="$(CC)" /bin/sh ./configure --prefix=/usr/local && $(MAKE) all
	cd $(NVI) && make all

smallinstall:: small
	cd $(GZIP) && $(MAKE) install 
	cd $(NVI) && make install

big:
	cd $(FLEX) && /bin/sh build build
	cd $(BZIP2) && /bin/sh build build

biginstall: big
	cd $(FLEX) && make install
	cd $(BZIP2) && make install

clean::
	if [ -f $(FLEX)/Makefile ] ; then cd $(FLEX) && make $@; fi
	if [ -f $(GZIP)/Makefile ] ; then cd $(GZIP) && make $@; fi
	cd $(BZIP2) && make clean
	if [ -f $(NVIWORK)/Makefile ]; then cd $(NVIWORK) && make clean; fi
	for p in $(SMALLPROGRAMS); do ( cd $$p && make clean ); done

small::
	set -e; for p in $(SMALLPROGRAMS); do ( cd $$p && make all ); done

smallinstall::
	set -e; for p in $(SMALLPROGRAMS); do ( cd $$p && make install ); done

