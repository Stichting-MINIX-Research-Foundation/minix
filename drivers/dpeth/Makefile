##
##  Makefile for ISA ethernet drivers	May  02, 2000
##
##  $Log$
##  Revision 1.1  2005/06/29 10:16:46  beng
##  Import of dpeth 3c501/3c509b/.. ethernet driver by
##  Giovanni Falzoni <fgalzoni@inwind.it>.
##
##  Revision 2.0  2005/06/26 16:16:46  lsodgf0
##  Initial revision for Minix 3.0.6
##
##  $Id$

##  Programs, flags, etc.
DRIVER	= dpeth

debug	= 0

CC	= exec cc
LD	= $(CC)
CPPFLAGS= -I.. -I/usr/include -Ddebug=$(debug)
CFLAGS	= -ws $(CPPFLAGS)
LDFLAGS	= -i -o $@

SRCS	= 3c501.c 3c509.c 3c503.c ne.c wd.c 8390.c devio.c netbuff.c dp.c
OBJS	= 3c501.o 3c509.o 3c503.o ne.o wd.o 8390.o devio.o netbuff.o dp.o
LIBS	= -lutils -lsys # -ltimers

##  Build rules
all build:	$(DRIVER)

$(DRIVER):	$(OBJS)
	$(CC) $(OBJS) $(LIBS) $(LDFLAGS)
	install -S 4kw $(DRIVER)

##  Install with other drivers
install:	/usr/sbin/drivers/$(DRIVER)
/usr/sbin/drivers/$(DRIVER):	$(DRIVER)
	install -o root -cs $? $@

##  Generate dependencies

depend: 
	/usr/bin/mkdep "$(CC) -E $(CPPFLAGS)" *.c > .depend

##  Clean directory
clean:
	@rm -f $(DRIVER) *.o *.BAK

include .depend

##  end
