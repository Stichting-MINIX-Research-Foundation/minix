
INC=/usr/include

all::

clean::

install::
	-rm -rf $(INC)
	mkdir $(INC)
	tar cf - `find . -name '*.h'` | (cd $(INC) && tar xf -)
