# $Header$

# CFLAGS : options for the C-compiler
CFLAGS = -O -D_MINIX -D_POSIX_SOURCE -wa
CC=exec cc

# LDFLAGS : flags for the loader
#	-i for a PDP-11 with separate I/D (not necessary)
LDFLAGS = -i

# BINDIR : where the binary will go when you type "make install"
BINDIR = /usr/bin

# LIBRARIES : -ltermcap or -ltermlib,
#	-lPW on USG systems (System III, System V),
# 	-ljobs on Berkeley Unix system (4.1, 2.8?, 2.9)
LIBRARIES =

HFILES= assert.h\
	commands.h\
	display.h\
	getcomm.h\
	getline.h\
	help.h\
	in_all.h\
	keys.h\
	machine.h\
	main.h\
	options.h\
	output.h\
	pattern.h\
	process.h\
	prompt.h\
	term.h

CFILES=	assert.c\
	commands.c\
	display.c\
	getcomm.c\
	getline.c\
	help.c\
	keys.c\
	machine.c\
	main.c\
	options.c\
	output.c\
	pattern.c\
	process.c\
	prompt.c\
	term.c

OFILES=	assert.o\
	commands.o\
	display.o\
	getcomm.o\
	getline.o\
	help.o\
	keys.o\
	machine.o\
	main.o\
	options.o\
	output.o\
	pattern.o\
	process.o\
	prompt.o\
	term.o

all:		yap

yap:		$(OFILES)
		$(CC) $(LDFLAGS) -o yap $(OFILES) $(LIBRARIES)
		install -S 16kw yap

install:	$(BINDIR)/yap $(BINDIR)/more

$(BINDIR)/yap:	yap
		install -cs -o bin yap $@

$(BINDIR)/more:	$(BINDIR)/yap
		install -l $? $@

clean:
		rm -f yap $(OFILES) a.out core *.bak

pr:
		pr Makefile $(HFILES) $(CFILES)

lint:
		lint $(DEFINES) $(CFILES)

#AUTOAUTOAUTOAUTOAUTOAUTOAUTOAUTOAUTOAUTO
assert.o:	assert.h
assert.o:	in_all.h
assert.o:	output.h
assert.o:	term.h
commands.o:	assert.h
commands.o:	commands.h
commands.o:	display.h
commands.o:	getcomm.h
commands.o:	getline.h
commands.o:	help.h
commands.o:	in_all.h
commands.o:	keys.h
commands.o:	machine.h
commands.o:	main.h
commands.o:	options.h
commands.o:	output.h
commands.o:	pattern.h
commands.o:	process.h
commands.o:	prompt.h
commands.o:	term.h
display.o:	assert.h
display.o:	display.h
display.o:	getline.h
display.o:	in_all.h
display.o:	machine.h
display.o:	main.h
display.o:	options.h
display.o:	output.h
display.o:	process.h
display.o:	term.h
getcomm.o:	assert.h
getcomm.o:	commands.h
getcomm.o:	display.h
getcomm.o:	getcomm.h
getcomm.o:	getline.h
getcomm.o:	in_all.h
getcomm.o:	keys.h
getcomm.o:	machine.h
getcomm.o:	main.h
getcomm.o:	output.h
getcomm.o:	process.h
getcomm.o:	prompt.h
getcomm.o:	term.h
getline.o:	assert.h
getline.o:	display.h
getline.o:	getline.h
getline.o:	in_all.h
getline.o:	main.h
getline.o:	options.h
getline.o:	output.h
getline.o:	process.h
getline.o:	term.h
help.o:	commands.h
help.o:	display.h
help.o:	help.h
help.o:	in_all.h
help.o:	keys.h
help.o:	machine.h
help.o:	main.h
help.o:	options.h
help.o:	output.h
help.o:	prompt.h
help.o:	term.h
keys.o:	assert.h
keys.o:	commands.h
keys.o:	in_all.h
keys.o:	keys.h
keys.o:	machine.h
keys.o:	prompt.h
machine.o:	assert.h
machine.o:	getline.h
machine.o:	in_all.h
machine.o:	machine.h
main.o:	commands.h
main.o:	display.h
main.o:	in_all.h
main.o:	main.h
main.o:	options.h
main.o:	output.h
main.o:	process.h
main.o:	prompt.h
main.o:	term.h
options.o:	display.h
options.o:	in_all.h
options.o:	options.h
options.o:	output.h
output.o:	in_all.h
output.o:	main.h
output.o:	output.h
pattern.o:	in_all.h
pattern.o:	pattern.h
process.o:	commands.h
process.o:	display.h
process.o:	getline.h
process.o:	in_all.h
process.o:	main.h
process.o:	options.h
process.o:	output.h
process.o:	process.h
process.o:	prompt.h
prompt.o:	display.h
prompt.o:	getcomm.h
prompt.o:	getline.h
prompt.o:	in_all.h
prompt.o:	main.h
prompt.o:	options.h
prompt.o:	output.h
prompt.o:	process.h
prompt.o:	prompt.h
prompt.o:	term.h
term.o:	display.h
term.o:	getline.h
term.o:	in_all.h
term.o:	keys.h
term.o:	machine.h
term.o:	main.h
term.o:	options.h
term.o:	output.h
term.o:	term.h
