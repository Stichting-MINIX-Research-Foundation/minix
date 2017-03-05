# $eterna: Makefile.boot,v 1.9 2010/05/10 04:57:50 mrg Exp $
#
# very simple makefile to compile bozohttpd, should work with every make.
# see Makefile for a list of compile options that may be placed in CFLAGS.

CC=	cc
OPT=	-O
LARGE_CFLAGS=	-D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64
LOCAL_CFLAGS=	-DNO_LUA_SUPPORT
CFLAGS=	$(OPT) $(LARGE_CFLAGS) $(LOCAL_CFLAGS)

GROFF=	groff -Tascii
CRYPTOLIBDIR=	# -L/usr/local/lib
CRYPTOLIBS=	$(CRYPTOLIBDIR) -lcrypto -lssl

FILES=	bozohttpd.c auth-bozo.c cgi-bozo.c content-bozo.c daemon-bozo.c \
	dir-index-bozo.c lua-bozo.c ssl-bozo.c tilde-luzah-bozo.c main.c

all:
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o bozohttpd $(FILES) $(CRYPTOLIBS)

man:
	$(GROFF) -mandoc bozohttpd.8 > bozohttpd.cat8

clean:
	rm -f bozohttpd bozohttpd.cat8 *.o
