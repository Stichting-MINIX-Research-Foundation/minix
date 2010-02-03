# Makefile for commands.

MAKE	= exec make -$(MAKEFLAGS)
BZIP2=bzip2-1.0.3
FLEX=flex-2.5.4

SMALLPROGRAMS=`arch` aal advent ash autil awk byacc cawf cron de dhcpd dis88 elle elvis ftp101 ftpd200 ibm indent m4 make mdb mined patch pax profile ps reboot rlogind scripts sh simple syslogd talk talkd telnet telnetd urlget yap zoneinfo

usage:
	@echo "Usage: make all          # Compile all commands" >&2
	@echo "       make install      # Install the result (run as bin!)" >&2
	@echo "       make clean        # Delete .o files and other junk" >&2

all:
	cd zmodem && make
	cd $(BZIP2) && /bin/sh build build
	set -e; for p in $(SMALLPROGRAMS); do ( cd $$p && make all ); done

install:
	set -e; for p in $(SMALLPROGRAMS); do ( cd $$p && make install ); done
	cd zmodem && make install
	cd $(BZIP2) && make install
	cd $(FLEX) && sh build

clean::
	cd $(BZIP2) && make clean
	cd zmodem && make clean
	if [ -f $(FLEX)/Makefile ]; then cd $(FLEX) && make distclean ; fi
	for p in $(SMALLPROGRAMS); do ( cd $$p && make clean ); done


