
INC=/usr/include
GCCVERSION=3.4.3
MKHEADERS=/usr/gnu//libexec/gcc/i386-pc-minix/$(GCCVERSION)/install-tools/mkheaders

all::

clean::

install::
	-rm -rf $(INC)
	mkdir -p $(INC)
	cpdir . $(INC)
	@chown -R bin $(INC)
	@rm -f $(INC)/Makefile

gcc: install
	SHELL=/bin/sh; if [ -f $(MKHEADERS) ] ; then sh -e $(MKHEADERS) ; fi
