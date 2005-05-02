
MAN=/usr/man

all::

clean::

install::
	-rm -rf $(MAN)
	mkdir $(MAN)
	tar cf - `find . -name '*.[0-9]'` | (cd $(MAN) && tar xf -)
	chown -R bin /usr/man
	chgrp -R operator /usr/man
	find /usr/man -type f | xargs chmod 644
	find /usr/man -type d | xargs chmod 755
	makewhatis $(MAN)
