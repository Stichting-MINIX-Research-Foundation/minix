# Makefile for dhcpd.

CFLAGS	= $(OPT) -D_MINIX
LDFLAGS	=

all:	dhcpd

OBJ=	dhcpd.o tags.o devices.o ether.o

dhcpd:	$(OBJ)
	$(CC) $(LDFLAGS) -o $@ $(OBJ)
	install -S 12kw $@

install:	/usr/bin/dhcpd

/usr/bin/dhcpd:	dhcpd
	install -c $? $@

clean:
	rm -f *.o dhcpd core a.out

# Dependencies.
$(OBJ):			dhcpd.h
dhcpd.o ether.o:	arp.h
