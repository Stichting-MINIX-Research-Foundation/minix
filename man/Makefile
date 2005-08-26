
MAN=/usr/man

all::

clean::

install::
	-rm -rf $(MAN)
	mkdir $(MAN)
	cpdir . $(MAN)
	chown -R bin /usr/man
	chgrp -R operator /usr/man
	find /usr/man -type f | xargs chmod 644
	find /usr/man -type d | xargs chmod 755
	makewhatis $(MAN)
