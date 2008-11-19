##
##  Makefile for ISA ethernet drivers	May  02, 2000
##

##  Programs, flags, etc.
DRIVER	= dpeth

debug	= 0

CC	= exec cc
LD	= $(CC)
CPPFLAGS= -I.. -I/usr/include -Ddebug=$(debug) $(CPROFILE)
CFLAGS	= -ws $(CPPFLAGS)
LDFLAGS	= -i -o $@

SRCS	= 3c501.c 3c509.c 3c503.c ne.c wd.c 8390.c devio.c netbuff.c dp.c
OBJS	= 3c501.o 3c509.o 3c503.o ne.o wd.o 8390.o devio.o netbuff.o dp.o
LIBS	= -lsys

##  Build rules
all build:	$(DRIVER)

$(DRIVER):	$(OBJS)
	$(CC) $(OBJS) $(LIBS) $(LDFLAGS)
	install -S 16k $(DRIVER)

##  Install with other drivers
install:	/usr/sbin/$(DRIVER)
/usr/sbin/$(DRIVER):	$(DRIVER)
	install -o root -cs $? $@

##  Generate dependencies

depend: 
	mkdep "$(CC) -E $(CPPFLAGS)" *.c > .depend

##  Clean directory
clean:
	@rm -f $(DRIVER) *.o *.BAK

include .depend

##  end
