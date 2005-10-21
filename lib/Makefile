
ACKBASE=$(CURDIR)/obj-ack
GNUBASE=$(CURDIR)/obj-gnu

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
	zlib-1.2.3 \
	ack \
	gnu
	

include Makefile.inc

install:: all
	install -o bin $(ACKBASE)/*.[ao] /usr/lib/i386
	install -o bin $(ACKBASE)/*.[ao] /usr/local/lib
	install -o bin $(GNUBASE)/*.[ao] /usr/gnu/lib
