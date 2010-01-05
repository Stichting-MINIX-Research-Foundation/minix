
ETC=/etc/
USRETC=/usr/etc/
FILES1=fstab group hostname.file inet.conf motd.install mtab passwd profile \
	protocols rc services termcap ttytab utmp rc.cd binary_sizes \
	binary_sizes.big binary_sizes.xxl syslog.conf rc.daemons.dist \
	rs.inet rs.single make.conf system.conf
FILES2=shadow
FILES3=daily dhcptags.conf rc

all::

clean::

install::
	@echo "Installing /etc and /usr/etc.."
	mkdir -p $(ETC)
	@for f in $(FILES1); do if [ -f $(ETC)/$$f ]; then :; else cp $$f $(ETC)/$$f; chmod 755 $(ETC)/$$f; fi; done
	@for f in $(FILES2); do if [ -f $(ETC)/$$f ]; then :; else cp $$f $(ETC)/$$f; chmod 600 $(ETC)/$$f; fi; done
	@echo "Making hierarchy.."
	sh mtree.sh mtree/minix.tree
	@for f in $(FILES3); do if [ -f $(USRETC)/$$f ]; then :; else cp usr/$$f $(USRETC); chmod 755 $(USRETC)/$$f; fi; done
	@echo "Making devices.."
	p=`pwd` && cd /dev && sh $$p/../commands/scripts/MAKEDEV.sh null 
	p=`pwd` && cd /dev && sh $$p/../commands/scripts/MAKEDEV.sh std  2>/dev/null
	install -o bin -g operator -m 755 crontab /usr/lib
	@echo "Making user homedirs.."
	for u in /usr/ast ~root; do cp ast/.[aepv]* $$u ; done
	@echo "Installing fonts.."
	install -m 644 -o bin -g operator fonts/*.fnt /usr/lib/fonts/

postinstall:
	binsizes normal

