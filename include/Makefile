
INC=/usr/include
MKHEADERS343=/usr/gnu//libexec/gcc/i386-pc-minix/3.4.3/install-tools/mkheaders
MKHEADERS411=/usr/gnu//libexec/gcc/i386-pc-minix/4.1.1/install-tools/mkheaders

all::

clean::

install::
	-rm -rf $(INC)
	mkdir -p $(INC)
	cpdir . $(INC)
	@chown -R bin $(INC)
	@rm -f $(INC)/Makefile

gcc: install
	SHELL=/bin/sh; if [ -f $(MKHEADERS343) ] ; then sh -e $(MKHEADERS343) ; fi
	SHELL=/bin/sh; if [ -f $(MKHEADERS411) ] ; then sh -e $(MKHEADERS411) ; fi
