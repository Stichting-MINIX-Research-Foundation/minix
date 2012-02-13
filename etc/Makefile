ETC=/etc/
USR=/usr/
USRETC=/usr/etc/
DEFAULTS=/etc/defaults
FILES1=group hostname.file inet.conf motd.install mtab profile \
	protocols rc services termcap ttytab utmp rc.cd  \
	syslog.conf rc.daemons.dist \
	rs.inet rs.single make.conf system.conf ttys resolv.conf rc.conf \
	rc.subr rc.subr.minix man.conf shells boot.cfg.default

PWFILES=master.passwd
FILES3=daily dhcptags.conf rc 
DEFAULTFILES=rc.conf minix.rc.conf 
USRFILES=Makefile

TOOL_PWD_MKDB= pwd_mkdb

clean::

install:: installpw	# installpw needed to bootstrap pw db
	@echo "Installing /etc, /usr/etc and /usr/lib.."
	mkdir -p $(ETC) $(USRLIB)
	@for f in $(FILES1); do if [ -f $(ETC)/$$f ]; then :; else cp $$f $(ETC)/$$f; chmod 755 $(ETC)/$$f; fi; done
	@for f in $(USRFILES); do cp usr/$$f $(USR)/$$f; chmod 644 $(USR)/$$f; done
	@echo "Making hierarchy.."
	sh mtree.sh mtree/minix.tree
	@for f in $(FILES3); do if [ -f $(USRETC)/$$f ]; then :; else cp usr/$$f $(USRETC); chmod 755 $(USRETC)/$$f; fi; done
	for f in $(DEFAULTFILES); do cp defaults/$$f $(DEFAULTS)/; chmod 644 $(DEFAULTS)/$$f; done
	@echo "Making devices.."
	p=`pwd` && cd /dev && sh $$p/../commands/MAKEDEV/MAKEDEV.sh null
	p=`pwd` && cd /dev && sh $$p/../commands/MAKEDEV/MAKEDEV.sh std  2>/dev/null
	install -o root -g operator -m 755 crontab /usr/lib
	@echo "Making user homedirs.."
	for u in /usr/ast ~root; do cp ast/.[aepv]* $$u ; done
	@echo "Installing fonts.."
	install -m 644 -o root -g operator fonts/*.fnt /usr/lib/fonts/

installforce:: $(ETC)/rc $(ETC)/rs.inet $(ETC)/rs.single $(ETC)/system.conf $(ETC)/rc.subr.minix $(USRETC)/rc $(USR)/Makefile installpw

installpw::
	if [ ! -d $(ETC) ]; then mkdir $(ETC); chmod 755 $(ETC); fi
	@for f in $(PWFILES); do if [ -f $(ETC)/$$f ]; then :; else cp $$f $(ETC)/$$f; chmod 600 $(ETC)/$$f; fi; done
	touch /etc/pwd.db; touch /etc/spwd.db; ${TOOL_PWD_MKDB} -p -V 0 /etc/master.passwd

$(ETC)/rc: rc .PHONY
	install -m 755 -o root -g operator $> $@

$(ETC)/rs.inet: rs.inet .PHONY
	install -m 755 -o root -g operator $> $@

$(ETC)/rs.single: rs.single .PHONY
	install -m 755 -o root -g operator $> $@

$(ETC)/system.conf: system.conf .PHONY
	install -m 644 -o root -g operator $> $@

$(ETC)/rc.subr.minix: rc.subr.minix .PHONY
	install -m 644 -o root -g operator $> $@

$(USRETC)/rc: usr/rc .PHONY
	install -m 755 -o root -g operator $> $@

$(USR)/Makefile: usr/Makefile .PHONY
	install -m 644 -o root -g operator $> $@
