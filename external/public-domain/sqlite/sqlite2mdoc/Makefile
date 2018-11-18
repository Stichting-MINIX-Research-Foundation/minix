# If you're on Linux, un-comment the following.
#LDADD	 = -lbsd

#####################################################################
# You probably don't want to change anything beneath here.
#####################################################################

CFLAGS	+= -g -W -Wall
PREFIX	 = /usr/local

sqlite2mdoc: main.o
	$(CC) -o $@ main.o $(LDADD)

install:
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	mkdir -p $(DESTDIR)$(PREFIX)/man/man1
	install -m 0755 sqlite2mdoc $(DESTDIR)$(PREFIX)/bin
	install -m 0444 sqlite2mdoc.1 $(DESTDIR)$(PREFIX)/man/man1

clean:
	rm -f sqlite2mdoc main.o
	rm -rf sqlite2mdoc.dSYM
