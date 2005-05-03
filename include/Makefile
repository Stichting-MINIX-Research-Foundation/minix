
INC=/usr/include

all::

clean::

install::
	-rm -rf $(INC)
	mkdir -p $(INC)
	tar cf - `find . -name '*.h'` | (cd $(INC) && tar xf -)
