##  $Revision$
##
##  Unix makefile for editline library.
##

##  Set your options:
##	-DANSI_ARROWS		ANSI arrows keys work like emacs.
##	-DHAVE_STDLIB		Have <stdlib.h>.
##	-DHAVE_TCGETATTR	Have tcgetattr(), tcsetattr().
##	-DHAVE_TERMIO		Have "struct termio" and <termio.h>
##	(If neither of above two, we use <sgttyb.h> and BSD ioctl's)
##	-DHIDE			Make static functions static (non debug).
##	-DHIST_SIZE=n		History size.
##	-DNEED_STRDUP		Don't have strdup().
##	-DUNIQUE_HISTORY	Don't save command if same as last one.
##	-DUSE_DIRENT		Use <dirent.h>, not <sys/dir.h>?
##	-DUSE_TERMCAP		Use the termcap library for terminal size
##				see LDFLAGS, below, if you set this.
##	-DNEED_PERROR		Don't have perror() (used in testit)
DEFS	= -DANSI_ARROWS -DHAVE_STDLIB -DHAVE_TCGETATTR -DHIDE -DUSE_DIRENT \
	  -DHIST_SIZE=100 -DUSE_TERMCAP -DSYS_UNIX -DNEED_STRDUP

CFLAGS	= -O -D_MINIX -D_POSIX_SOURCE $(DEFS) -wo
CC1	= $(CC) $(CFLAGS) -c

##  If you have -DUSE_TERMCAP, set this as appropriate:
#LDFLAGS = -ltermlib
#LDFLAGS = -ltermcap

##  End of configuration.

SOURCES	= editline.c complete.c sysunix.c
LIBRARY = ../libedit.a
OBJECTS	= $(LIBRARY)(editline.o) $(LIBRARY)(complete.o) $(LIBRARY)(sysunix.o)
SHARFILES =	README Makefile editline.3 editline.h unix.h editline.c \
		complete.c sysunix.c testit.c

install:	$(LIBRARY)

testit:		testit.c $(LIBRARY)
	$(CC) $(CFLAGS) -o testit testit.c $(LIBRARY) $(LDFLAGS)

shar:		$(SHARFILES)
	shar $(SHARFILES) >shar

clean:
	rm -f *.[oa] testit foo core tags lint lint.all a.out shar

lint:		testit
	lint -a -b -u -x $(DEFS) $(SOURCES) testit.c >lint.all
	sed -e '/warning: function prototype not in scope/d' \
		-e '/warning: old style argument declaration/'d \
		-e '/mix of old and new style function declaration/'d \
		<lint.all >lint

$(LIBRARY):	$(OBJECTS)
	aal cr $@ *.o
	rm *.o

$(OBJECTS):	editline.h

$(LIBRARY)(editline.o):	editline.c
	$(CC1) editline.c

$(LIBRARY)(complete.o):	complete.c
	$(CC1) complete.c

$(LIBRARY)(sysunix.o):	sysunix.c
	$(CC1) sysunix.c
