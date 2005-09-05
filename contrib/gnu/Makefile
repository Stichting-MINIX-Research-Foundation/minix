
EMACS=emacs-21.4
LYNX=lynx2-8-5

all install::
	cd $(LYNX) && sh MINIX/lynx.sh && bigmake $@
	cd $(EMACS) && bigmake $@

all install clean::
	if [ -f $(LYNX)/Makefile ] ; then cd $(LYNX) && bigmake $@; fi
	cd $(EMACS) && bigmake $@

