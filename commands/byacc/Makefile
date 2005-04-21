# Makefile for Berkeley yacc.

BINDIR	      = /usr/bin

HDRS	      = defs.h

CFLAGS	      = -DNDEBUG -D_MINIX -D_POSIX_SOURCE -wo $(OPT)

LDFLAGS	      = -i

LIBS	      =

LINKER	      = $(CC)

MAKEFILE      = Makefile

OBJS	      = closure.o \
		error.o \
		lalr.o \
		lr0.o \
		main.o \
		mkpar.o \
		output.o \
		reader.o \
		skeleton.o \
		symtab.o \
		verbose.o \
		warshall.o

PRINT	      = pr -f -l88

PROGRAM	      = yacc

SRCS	      = closure.c \
		error.c \
		lalr.c \
		lr0.c \
		main.c \
		mkpar.c \
		output.c \
		reader.c \
		skeleton.c \
		symtab.c \
		verbose.c \
		warshall.c

all:		$(PROGRAM)

$(PROGRAM):     $(OBJS) $(LIBS)
		$(LINKER) $(LDFLAGS) -o $(PROGRAM) $(OBJS) $(LIBS)
		install -S 384k $(PROGRAM)

install:	$(BINDIR)/yacc

$(BINDIR)/yacc:	$(PROGRAM)
		install -cs -o bin $(PROGRAM) $@

clean:;		rm -f $(OBJS) $(PROGRAM) core

#depend:;	@mkmf -f $(MAKEFILE) PROGRAM=$(PROGRAM) DEST=$(DEST)
#
#index:;	@ctags -wx $(HDRS) $(SRCS)
#
#listing:;	@$(PRINT) Makefile $(HDRS) $(SRCS) | lpr
#
#lint:;		@lint $(SRCS)
#
#program:        $(PROGRAM)
#
#tags:           $(HDRS) $(SRCS); @ctags $(HDRS) $(SRCS)

###
closure.o: defs.h
error.o: defs.h
lalr.o: defs.h
lr0.o: defs.h
main.o: defs.h
mkpar.o: defs.h
output.o: defs.h
reader.o: defs.h
skeleton.o: defs.h
symtab.o: defs.h
verbose.o: defs.h
warshall.o: defs.h
