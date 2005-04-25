
ETC=/etc
USRETC=/usr/etc
FILES1=fstab group hostname.file inet.conf motd mtab passwd profile protocols rc services termcap ttytab utmp
FILES2=shadow
FILES3=daily dhcptags.conf rc

all::

clean::

install::
	@echo "Installing /etc and /usr/etc.."
	mkdir -p $(ETC)
	for f in $(FILES1); do if [ -f $(ETC)/$$f ]; then echo $$f exists; else cp $$f $(ETC); fi; done
	for f in $(FILES2); do if [ -f $(ETC)/$$f ]; then echo $$f exists; else cp $$f $(ETC); chmod 600 $(ETC)/$$f; fi; done
	for f in $(FILES3); do if [ -f $(USRETC)/$$f ]; then echo $$f exists; else cp usr/$$f $(USRETC); chmod 755 $(USRETC)/$$f; fi; done
	@echo "Making hierarchy.."
	./mtree.sh mtree/minix.tree
	cd /dev && /usr/src/commands/scripts/MAKEDEV.sh std
	(cd ast && tar cf - . ) | (cd /usr/ast && tar xf - )
