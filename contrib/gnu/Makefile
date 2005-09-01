
EMACS=emacs-21.4
LYNX=lynx2-8-5

#-[ -f $(EMACS)/Makefile ] && cd $(EMACS) && $(MAKE) $@

all install::
	cd $(LYNX) && sh MINIX/lynx.sh && bigmake $@

all install clean::
	if [ -f $(LYNX)/Makefile ] ; then cd $(LYNX) && bigmake $@; fi
