
MAN=/usr/man

all::

clean::

install::
	-rm -rf $(MAN)
	-mkdir $(MAN)
	cpdir . $(MAN)
	chown -R bin $(MAN)
	chgrp -R operator $(MAN)
	@rm -f $(MAN)/Makefile
	find /usr/man -type f | xargs chmod 644
	find /usr/man -type d | xargs chmod 755
	makewhatis $(MAN)
