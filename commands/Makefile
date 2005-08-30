# Makefile for commands.

MAKE	= exec make -$(MAKEFLAGS)
FLEX=flex-2.5.4
GZIP=gzip-1.2.4
PYTHON=python-1.5.2
BZIP2=bzip2-1.0.3
KERMIT=kermit-2.1.1

usage:
	@echo "Usage: make all        # Compile all commands" >&2
	@echo "       make install    # Install the result (run as bin!)" >&2
	@echo "       make clean      # Delete .o files and other junk" >&2
	@echo "       make bigall     # Compile all big commands" >&2
	@echo "       make biginstall # Install all big commands" >&2
	@false

bigall:
	cd $(FLEX) && /bin/sh ./configure --prefix=/usr/local && bigmake all
	cd $(GZIP) && /bin/sh ./configure --prefix=/usr/local && bigmake all
	cd $(PYTHON) && /bin/sh ./configure --prefix=/usr/local && bigmake all
	cd $(BZIP2) && bigmake all
	cd $(KERMIT) && bigmake all

biginstall:
	cd $(FLEX) && /bin/sh ./configure --prefix=/usr/local && bigmake install
	cd $(GZIP) && /bin/sh ./configure --prefix=/usr/local && bigmake install
	cd $(PYTHON) && /bin/sh ./configure --prefix=/usr/local && bigmake install
	cd $(BZIP2) && bigmake install
	cd $(KERMIT) && bigmake install

clean::
	if [ -f $(FLEX)/Makefile ] ; then cd $(FLEX) && bigmake $@; fi
	if [ -f $(GZIP)/Makefile ] ; then cd $(GZIP) && bigmake $@; fi
	if [ -f $(PYTHON)/Makefile ] ; then cd $(PYTHON) && bigmake $@; fi
	cd $(BZIP2) && bigmake clean
	cd $(KERMIT) && bigmake clean

all install clean::
	cd `arch` && $(MAKE) $@
	cd aal && $(MAKE) $@
	cd advent && $(MAKE) $@
	cd ash && $(MAKE) $@
	cd autil && $(MAKE) $@
	cd awk && $(MAKE) $@
	cd bc && $(MAKE) $@
	cd byacc && $(MAKE) $@
	cd cawf && $(MAKE) $@
	cd cron && $(MAKE) $@
	cd de && $(MAKE) $@
	cd dhcpd && $(MAKE) $@
	cd dis88 && $(MAKE) $@
	cd elle && $(MAKE) $@
	cd elvis && $(MAKE) $@
	cd ftp && $(MAKE) $@
	cd ftpd && $(MAKE) $@
	cd ftpd200 && $(MAKE) $@
	cd ibm && $(MAKE) $@
	cd indent && $(MAKE) $@
	cd m4 && $(MAKE) $@
	cd make && $(MAKE) $@
	cd mdb && $(MAKE) $@
	cd mined && $(MAKE) $@
	cd patch && $(MAKE) $@
	cd ps && $(MAKE) $@
	cd reboot && $(MAKE) $@
	cd rlogind && $(MAKE) $@
	cd scripts && $(MAKE) $@
	cd sh && $(MAKE) $@
	cd simple && $(MAKE) $@
	cd talk && $(MAKE) $@
	cd talkd && $(MAKE) $@
	cd telnet && $(MAKE) $@
	cd telnetd && $(MAKE) $@
	cd urlget && $(MAKE) $@
	cd yap && $(MAKE) $@
	cd zmodem && $(MAKE) $@
