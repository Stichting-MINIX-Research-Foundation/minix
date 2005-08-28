
INC=/usr/include

all::

clean::

install::
	-rm -rf $(INC)
	mkdir -p $(INC)
	cpdir . $(INC)
	@chown -R bin $(INC)
	@rm -f $(INC)/Makefile
