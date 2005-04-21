# Makefile for commands.

MAKE	= exec make -$(MAKEFLAGS)

usage:
	@echo "Usage: make all       # Compile all commands" >&2
	@echo "       make install   # Install the result (run as bin!)" >&2
	@echo "       make clean     # Delete .o files and other junk" >&2
	@false

all install clean:
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
	cd flex-2.3.7 && $(MAKE) $@
	cd ftp && $(MAKE) $@
	cd ftpd && $(MAKE) $@
	cd ibm && $(MAKE) $@
	cd kermit && $(MAKE) $@
	cd m4 && $(MAKE) $@
	cd make && $(MAKE) $@
	cd mined && $(MAKE) $@
	cd patch && $(MAKE) $@
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
