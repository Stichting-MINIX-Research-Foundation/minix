##  $Revision$
##
##  Unix makefile for editline library.
##

LIB=		edit

##  Set your options:
##	-DANSI_ARROWS		ANSI arrows keys work like emacs.
##	-DHAVE_STDLIB		Have <stdlib.h>.
##	-DHAVE_TCGETATTR	Have , .
##	-DHAVE_TERMIO		Have "struct termio" and <termio.h>
##	(If neither of above two, we use <sgttyb.h> and BSD ioctl's)
##	-DHIDE			Make static functions static (non debug).
##	-DHIST_SIZE=n		History size.
##	-DNEED_STRDUP		Don't have .
##	-DUNIQUE_HISTORY	Don't save command if same as last one.
##	-DUSE_DIRENT		Use <dirent.h>, not <sys/dir.h>?
##	-DUSE_TERMCAP		Use the termcap library for terminal size
##				see LDFLAGS, below, if you set this.
##	-DNEED_PERROR		Don't have  (used in testit)
CPPFLAGS+=-DANSI_ARROWS -DHAVE_STDLIB -DHAVE_TCGETATTR -DHIDE -DUSE_DIRENT \
	-DHIST_SIZE=100 -DUSE_TERMCAP -DSYS_UNIX

SRCS=	editline.c complete.c sysunix.c

.include <bsd.lib.mk>
