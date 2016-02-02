# Just do user doc for the moment.
all: doxy-user 

doxy-user: doxygen-user.cfg
	[ -d doxy-user ] || mkdir doxy-user
	doxygen doxygen-user.cfg

doxy-dev: doxygen-dev.cfg
	[ -d doxy-dev ] || mkdir doxy-dev
	doxygen doxygen-dev.cfg

clean:
	rm -rf doxy-user doxy-dev

.PHONY: doxy-user doxy-dev
