# $NetBSD: Makefile,v 1.3 2011/02/11 23:44:43 christos Exp $

CC=gcc
CFLAGS=-I../include -Wall -pedantic -std=c99 -fPIC -ggdb
OBJS=
OBJS+= buffer.o
OBJS+= crypto.o
OBJS+= dict.o
OBJS+= error.o
OBJS+= list.o
OBJS+= mech.o parser.o
OBJS+= mech_anonymous.o
OBJS+= mech_crammd5.o
OBJS+= mech_digestmd5.o
OBJS+= mech_external.o
OBJS+= mech_gssapi.o
OBJS+= mech_login.o
OBJS+= mech_plain.o
OBJS+= msg.o
OBJS+= saslc.o
OBJS+= xsess.o

LDFLAGS=-lssl -lgssapi -ggdb

all: libsaslc.so

libsaslc.so: $(OBJS)
	$(CC) -shared -fPIC -o $@ $(OBJS) $(LDFLAGS)

clean:
	rm -f *.o *.so

splint:
	splint -weak -I../include *.[ch]
