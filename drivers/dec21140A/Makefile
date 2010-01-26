##
##  Makefile for Dec21140A ethernet board driver
##

##  Programs, flags, etc.
DRIVER	= dec21140A

debug	= 0

CC	= exec cc
LD	= $(CC)
CPPFLAGS= -I.. -I/usr/include -Ddebug=$(debug) $(CPROFILE)
CFLAGS	= -ws -w -Wall $(CPPFLAGS)
LDFLAGS	= -i -o $@

SRCS	= dec21140A.c
OBJS	= dec21140A.o
LIBS	=  -lsys # -ltimers

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
	@rm -f $(DRIVER) *.o *.BAK *~

include .depend

##  end
