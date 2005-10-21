
ACKBASE=$(CURDIR)/obj-ack
GNUBASE=$(CURDIR)/obj-gnu
ZLIB=zlib-1.2.3

OBJDIR=.

PATH:=$(PATH):/usr/gnu/bin

SUBDIRS = ansi \
	curses \
	dummy \
	editline \
	end \
	ip \
	math \
	other \
	posix \
	regex \
	stdio \
	syscall \
	syslib \
	util \
	sysutil \
	timers \
	i386 \
	$(ZLIB) \
	ack \
	gnu
	

include Makefile.inc

install::
	cd $(ZLIB) && $(MAKE) ACKBASE=$(ACKBASE) GNUBASE=$(GNUBASE) install

install:: all
	install -o bin $(ACKBASE)/*.[ao] /usr/lib/i386
	install -o bin $(GNUBASE)/*.[ao] /usr/gnu/lib
