
EMACS=emacs-21.4
LYNX=lynx2-8-5
COREUTILS=gnu-coreutils-5.2.1

all::
	cd $(LYNX) && /bin/sh makeme.sh
	cd $(EMACS) && /bin/sh makeme.sh
	cd $(COREUTILS) && /bin/sh makeme.sh

install:: all
	cd $(LYNX) && make install
	cd $(EMACS) && make install
	cd $(COREUTILS) && make install

clean::
	if [ -f $(LYNX)/Makefile ] ; then cd $(LYNX) && make clean; fi
	if [ -f $(EMACS)/Makefile ] ; then cd $(EMACS) && make clean; fi
	cd $(COREUTILS) && make clean

